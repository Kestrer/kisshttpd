#include "sendResponse.h"

#include "gzip.h"
#include "kisshttpd_internal.h"

#include <stdlib.h>
#include <string.h>

void
sendResponse(FILE* connection, struct Response response, bool const noBody, bool const acceptsGzip)
{
	fprintf(connection, "HTTP/1.1 ");

	fprintf(connection, "%i \r\n", response.code);

	if (response.uri != NULL) {
		fprintf(connection, "Location: %s\r\n", response.uri);
	}

	if (response.body.len == SIZE_MAX) {
		response.body.len = strlen((char*)response.body.data);
	}

	bool gzipped = false;
	if (!noBody && response.body.len && acceptsGzip) {
		size_t gzLen;
		unsigned char* gzData = gzip(response.body.data, response.body.len, &gzLen);
		if (gzData != NULL) {
			if (gzLen < response.body.len) {
				gzipped = true;
				response.body.data = gzData;
				response.body.len = gzLen;
				fprintf(connection, "Content-Encoding: gzip\r\nVary: Accept-Encoding\r\n");
			} else {
				free(gzData);
			}
		}
	}

	if (response.body.len) {
		fprintf(connection, "Content-Length: %zu\r\n", response.body.len);
		if (response.body.type != Body_Unknown) {
			fprintf(connection, "Content-Type: %s; charset=utf-8\r\n", mimeTable[response.body.type]);
		}
		fprintf(connection, "\r\n");
		if (!noBody) {
			fwrite(response.body.data, 1, response.body.len, connection);
		}
	} else {
		fprintf(connection, "Content-Length: 0\r\n\r\n");
	}

	if (gzipped) {
		free((unsigned char*)response.body.data);
	}
}
