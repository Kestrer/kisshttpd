#include "respondToConnection.h"

#include "parseRequest.h"
#include "sendResponse.h"

#include <stdlib.h>
#include <unistd.h>

static void
httpLog(struct Request request, struct Response response)
{
	startServerLog();
	printf("[%s][\"%s %s%s", request.sender, methodTable[request.method], request.host, request.path);
	printf("\"->%d %s ", response.code, mimeTable[response.body.type]);
	if (response.body.len != SIZE_MAX) {
		printf("<%zuB>", response.body.len);
	} else {
		printf("\"%s\"", response.body.data);
	}
	putc(']', stdout);
	if (response.info) {
		printf(": %s", response.info);
	}
	putc('\n', stdout);
}

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
	
	if (parseRequest(stream, connection->addr, &request, &noBody, &acceptsGzip) < 0) {
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
