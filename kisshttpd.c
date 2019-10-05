#include "kisshttpd.h"

#include "respondToConnection.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <unistd.h>

struct Server
{
	int socket;
	struct Response (*callback)(struct Request, void* userdata);
	void* userdata;
	thrd_t listener;
	atomic_bool stopped;
};

static int
listenOnServer(struct Server* server)
{
	while (!atomic_load(&server->stopped)) {
		for (;;) {
			struct sockaddr_in6 clientAddress;
			socklen_t clientAddressLen = sizeof(struct sockaddr_in6);
			errno = 0;
			int const connectionFd = accept(server->socket, (struct sockaddr*)&clientAddress, &clientAddressLen);
			if (connectionFd < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
				perror("accept on v6 failed");
				continue;
			} else if (connectionFd < 0) {
				break;
			}
			struct Connection* connection = malloc(sizeof(struct Connection));
			if (connection == NULL) {
				close(connectionFd);
				perror("malloc connection struct failed");
				continue;
			}

			connection->fd = connectionFd;
			connection->addr = clientAddress;
			connection->callback = server->callback;
			connection->userdata = server->userdata;

			thrd_t callbackThread;
			if (thrd_create(&callbackThread, (int (*)(void*))respondToConnection, connection) != thrd_success) {
				close(connection->fd);
				free(connection);
				perror("creating callback thread failed");
				continue;
			}

			thrd_detach(callbackThread);
		}
		struct timespec const sleepLen = {.tv_sec = 0, .tv_nsec = 100 * 1000}; // 100 us
		thrd_sleep(&sleepLen, NULL);
	}
	return 0;
}

struct Server*
makeServer(struct Response (*callback)(struct Request, void* userdata), void* userdata, uint16_t port)
{
	struct Server* server = malloc(sizeof(struct Server));
	if (server == NULL) {
		return NULL;
	}

	server->callback = callback;
	server->userdata = userdata;

	atomic_init(&server->stopped, false);

	if ((server->socket = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
		free(server);
		return NULL;
	}

	int const disable = 0;
	int const enable = 1;
	// reuse old connections
	setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
	// enable ipv4 and ipv6
	setsockopt(server->socket, IPPROTO_IPV6, IPV6_V6ONLY, &disable, sizeof(disable));

	struct sockaddr_in6 const address = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(port),
		.sin6_addr = in6addr_any,
	};

	if (bind(server->socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
		close(server->socket);
		free(server);
		return NULL;
	}

	int const backlog = 20;

	if (listen(server->socket, backlog) < 0) {
		close(server->socket);
		free(server);
		return NULL;
	}

	if (thrd_create(&server->listener, (int (*)(void*))listenOnServer, server) != thrd_success) {
		close(server->socket);
		free(server);
		return NULL;
	}

	return server;
}

void
stopServer(struct Server* server)
{
	atomic_store(&server->stopped, true);
	thrd_join(server->listener, NULL);
	close(server->socket);
	free(server);
}

void
freeResponseBody(struct Response response)
{
	free(response.body.data);
}
