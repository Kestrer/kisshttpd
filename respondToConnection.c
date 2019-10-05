#include "respondToConnection.h"

#include "parseRequest.h"
#include "sendResponse.h"

#include <stdlib.h>
#include <unistd.h>

int
respondToConnection(struct Connection* connection)
{
	FILE* stream = fdopen(connection->fd, "r+");

	if (stream == NULL) {
		perror("fdopen failed");
		close(connection->fd);
		free(connection);
		return 1;
	}

	struct Response (*callback)(struct Request, void* userdata) = connection->callback;
	void* userdata = connection->userdata;

	struct Request request;
	bool noBody;
	bool acceptsGzip;
	
	if (parseRequest(stream, *(struct sockaddr_in6*)&connection->addr, &request, &noBody, &acceptsGzip) < 0) {
		fclose(stream);
		free(connection);
		return -1;
	}

	free(connection);

	struct Response response = callback(request, userdata);

	sendResponse(stream, response, noBody, acceptsGzip);
	serverLog(request, response);

	if (response.freeFunc != NULL) {
		response.freeFunc(response);
	}

	freeRequest(request);

	fclose(stream);

	return 0;
}
