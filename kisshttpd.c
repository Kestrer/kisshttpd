#include "kisshttpd.h"

#include "respondToConnection.h"
#include "serverLog.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <unistd.h>
#include <uv.h>

struct Server
{
	thrd_t thread;
	atomic_bool stopped;
};

static void
allocateSuggested(uv_handle_t* handle, size_t suggested, uv_buf_t* buffer)
{
	buffer->base = malloc(suggested);
	buffer->len = suggested;
}

void
readRequest(uv_stream_t* stream, ssize_t nread, uv_buf_t const* data)
{
	if (nread == UV_EOF) {
		free(data->base);
		return;
	}
	if (nread < 0) {
		startServerLog();
		printf("[TCP ERROR]: Failed to read from stream: %s\n", uv_strerror(nread));
		uv_close((uv_handle_t*)stream, NULL);
		free(data->base);
		return;
	}
	struct ParseState* parseState = stream->data;
	enum ParseError parseError;
	for (size_t i = 0; i < data->len; ++i) {
		parseError = parseChar(parseState, data->base[i]);
		if (parseError != ParseError_Sucess) {
			break;
		}
	}
	uv_write_t client;
	if (parseError != ParseError_Complete) {
		switch (parseError) {
		case ParseError_NoMem:
			errorSend(connection, (struct Response){.code = 500, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Internal Server Error."}, .info = "No memory."}, parseState->request.noBody, parseState->request.acceptsGzip);
			break;
		case ParseError_UnknownMethod:
			errorSend(connection, (struct Response){.code = 501, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Unknown HTTP method."}}, parseState->request.noBody, parseState->request.acceptsGzip);
			break;
		case ParseError_BadRequest:
			errorSend(connection, (struct Response){.code = 400, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Bad request."}}, parseState->request.noBody, parseState->request.acceptsGzip);
			break;
		case ParseError_InvalidVersion:
			errorSend(connection, (struct Response){.code = 505, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"This server only supports HTTP/1.1."}}, parseState->request.noBody, parseState->request.acceptsGzip);
			break;
			uv_close((uv_handle_t*)stream, NULL);
			free(data->base);
			return;
		}
	}
	uv_close((uv_handle_t*)stream, NULL);
	free(data->base);
}

static void
handleConnection(uv_stream_t* server, int status)
{
	if (status < 0) {
		startServerLog();
		printf("[TCP ERROR]: Failed to get connection: %s.\n", uv_strerror(status));
		return;
	}

	uv_tcp_t client;
	uv_tcp_init(server->loop, &client);
	int uvError = uv_accept(server, (uv_stream_t*)client);
	if (uvError < 0) {
		printf("[TCP ERROR]: Failed to accept connection: %s.\n", uv_strerror(uvError));
		uv_close((uv_handle_t*)client, NULL);
		return;
	}

	/* TODO: parse request and delegate callback */
	client.data = malloc(sizeof(struct ParseState));
	if (client.data == NULL) {
		printf("[MEM ERROR]: %s.\n", strerror(errno));
		uv_close((uv_handle_t*)client, NULL);
		return;
	}
	startParse(&client.data, /* TODO */);

	uv_read_start((uv_stream_t*)client, allocateSuggested, readRequest);

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

struct LoopParameters
{
	struct Response (*callback)(struct Request, void* userdata);
	void* userdata;
	uv_loop_t loop;
	uv_tcp_t server;
};

static int
enterLoop(struct LoopParameters* parameters)
{
	int const r = uv_run(&parameters->loop, UV_RUN_DEFAULT);

	uv_close((uv_handle_t*)&parameters->server);
	uv_loop_close(&parameters->loop);
	free(parameters);

	return r;
}

struct Server*
makeServer(struct Response (*callback)(struct Request, void* userdata), void* userdata, uint16_t port)
{
	struct Server* server = malloc(sizeof(struct Server));
	if (server == NULL) return NULL;

	atomic_init(&server->stopped, false);

	struct LoopParameters* parameters = malloc(sizeof(struct LoopParameters));
	if (parameters == NULL) return NULL;

	parameters->callback = callback;
	parameters->userdata = userdata;

	errno = -uv_loop_init(&parameters->loop);
	if (errno) {
		free(parameters);
		free(server);
		return NULL;
	}

	errno = -uv_tcp_init(&parameters->loop, &parameters->server);
	if (errno) {
		uv_loop_close(&parameters->loop);
		free(parameters);
		free(server);
		return NULL;
	}
	struct sockaddr_in6 address;
	errno = -uv_ip6_addr("localhost", port, &address);
	if (errno) {
		uv_close((uv_handle_t*)&parameters->server);
		uv_loop_close(&parameters->loop);
		free(parameters);
		free(server);
		return NULL;
	}
	errno = -uv_tcp_bind(&parameters->server, (struct sockaddr const*)&address, 0);
	if (errno) {
		uv_close((uv_handle_t*)&parameters->server);
		uv_loop_close(&parameters->loop);
		free(parameters);
		free(server);
		return NULL;
	}
	errno = -uv_listen((uv_stream_t*)&parameters->server, 20, handleConnection);
	if (errno) {
		uv_close((uv_handle_t*)&parameters->server);
		uv_loop_close(&parameters->loop);
		free(parameters);
		free(server);
		return NULL;
	}

	if (thrd_create(&server->thread, (thrd_start_t)enterLoop, parameters) != thrd_success) {
		uv_close((uv_handle_t*)&parameters->server);
		uv_loop_close(&parameters->loop);
		free(parameters);
		free(server);
		return NULL;
	}

	return server;
}

void
stopServer(struct Server* server)
{
	atomic_store(&server->stopped, true);
	thrd_join(server->thread, NULL);
	free(server);
}

void
freeResponseBody(struct Response response)
{
	free(response.body.data);
}
