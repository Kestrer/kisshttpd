#include "parseRequest.h"

#include "kisshttpd_internal.h"
#include "sendResponse.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

static inline bool
isws(char const c)
{
	return c == ' ' || c == '\t';
}

#define MAXTIME sizeof("2038-01-19T03:14:07Z")
static inline void
getIsoTime(char* const timeStr)
{
	time_t const now = time(NULL);
	strftime(timeStr, MAXTIME, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
}

void
serverLog(struct Request request, struct Response response)
{
	char isoTime[MAXTIME];
	getIsoTime(isoTime);

	printf("[%s][%s][\"%s %s", isoTime, request.sender, methodTable[request.method], request.path);
	printf("\"->%d %s ", response.code, mimeTable[response.body.type]);
	if (response.body.len != SIZE_MAX)
		printf("<%zuB>", response.body.len);
	else
		printf("\"%s\"", response.body.data);
	putc(']', stdout);
	if (response.info)
		printf(": %s", response.info);
	putc('\n', stdout);
}

static void
errorSend(FILE* connection, struct Response response, bool const noBody, bool const acceptsGzip)
{
	sendResponse(connection, response, noBody, acceptsGzip);

	char isoTime[MAXTIME];
	getIsoTime(isoTime);

	printf("[%s][ERROR DUMP]: %s\n", isoTime, response.info ? response.info : (char*)response.body.data);
}

static size_t
strtozu(char const* str)
{
	size_t val = 0;
	while (*str) {
		if (!isdigit(*str))
			return SIZE_MAX;
		val *= 10;
		val += *str - '0';
		++str;
	}
	return val;
}

static int
parseMethod(FILE* connection, enum Method* method, bool* noBody)
{
	char methodStr[sizeof("OPTIONS")];
	size_t i = 0;
	for (int c; i < sizeof(methodStr)-1 && (c = fgetc(connection)) != ' '; ++i) {
		if (c == EOF) {
			return -1;
		}
		methodStr[i] = c;
	}
	if (i == sizeof(methodStr)-1) {
		return -1;
	}
	methodStr[i] = '\0';
	*noBody = false;
	if (!strcmp(methodStr, "GET")) {
		*method = HTTP_GET;
	} else if (!strcmp(methodStr, "HEAD")) {
		*method = HTTP_GET;
		*noBody = true;
	} else if (!strcmp(methodStr, "POST")) {
		*method = HTTP_POST;
	} else if (!strcmp(methodStr, "PUT")) {
		*method = HTTP_PUT;
	} else if (!strcmp(methodStr, "DELETE")) {
		*method = HTTP_DELETE;
	} else {
		errorSend(connection, (struct Response){.code = 501, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Unknown HTTP method."}}, false, false);
		return -1;
	}
	return 0;
}

static int
parsePath(FILE* connection, bool noBody, char** path)
{
	*path = NULL;
	size_t pathLen;
	if (getdelim(path, &pathLen, ' ', connection) < 0) {
		free(*path);
		if (!feof(connection)) {
			errorSend(connection, (struct Response){.code = 500, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Internal server error."}, .info = "stream error while parsing path"}, noBody, false);
		}
		return -1;
	}
	pathLen = strlen(*path)-1;
	(*path)[pathLen] = '\0';
	return 0;
}

static int
parseVersion(FILE* connection, bool noBody)
{
	char const httpVersion[] = "HTTP/1.1\r\n";
	char buffer[sizeof(httpVersion)-1];
	if (fread(buffer, 1, sizeof(httpVersion)-1, connection) != sizeof(httpVersion)-1) {
		if (!feof(connection)) {
			errorSend(connection, (struct Response){.code = 500, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Internal server error."}, .info = "stream error while parsing HTTP version"}, noBody, false);
		}
		return -1;
	}
	if (memcmp(httpVersion, buffer, sizeof(httpVersion)-1)) {
		errorSend(connection, (struct Response){.code = 505, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"This server only supports HTTP/1.1."}}, noBody, false);
		return -1;
	}
	return 0;
}

static int
parseHeaders(FILE* connection, bool noBody, size_t* contentLength, enum BodyType* contentType, bool* acceptsGzip)
{
	*contentLength = 0;
	*contentType = Body_Unknown;
	*acceptsGzip = false;
	for (;;) {
		int const headersEnd = fgetc(connection);
		if (headersEnd == EOF) {
			return -1;
		}
		if (headersEnd == '\r') {
			break;
		}
		ungetc(headersEnd, connection);

		char* headerName = NULL;
		size_t headerNameLen;
		if (getdelim(&headerName, &headerNameLen, ':', connection) < 0) {
			if (!feof(connection)) {
				errorSend(connection, (struct Response){.code = 500, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Internal server error."}, .info = "stream error while getting header name"}, noBody, *acceptsGzip);
			}
			free(headerName);
			return -1;
		}
		headerNameLen = strlen(headerName)-1;
		headerName[headerNameLen] = '\0';

		// consume whitespace between colon and value
		int ws;
		do {
			ws = fgetc(connection);
		} while (ws != EOF && isws(ws));
		if (ws == EOF) {
			free(headerName);
			return -1;
		}
		ungetc(ws, connection);

		char* headerValue = NULL;
		size_t headerValueLen;
		if (getdelim(&headerValue, &headerValueLen, '\r', connection) < 0) {
			if (!feof(connection)) {
				errorSend(connection, (struct Response){.code = 500, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Internal server error."}, .info = "stream error while getting header value"}, noBody, *acceptsGzip);
			}
			free(headerValue);
			free(headerName);
			return -1;
		}
		// remove whitespace between end of value and \r\n
		headerValueLen = strlen(headerValue)-1;
		while (isws(headerValue[headerValueLen])) {
			--headerValueLen;
		}
		++headerValueLen;
		headerValue[headerValueLen] = '\0';

		if (fgetc(connection) != '\n') {
			errorSend(connection, (struct Response){.code = 400, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Bad request."}, .info = "Bad request at line " STRINGIFY(__LINE__)}, noBody, *acceptsGzip);
			free(headerValue);
			free(headerName);
			return -1;
		}

		if (!strcmp(headerName, "Content-Length")) {
			*contentLength = strtozu(headerValue);
			if (*contentLength == SIZE_MAX) {
				errorSend(connection, (struct Response){.code = 400, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Bad request."}, .info = "Bad request at line " STRINGIFY(__LINE__)}, noBody, *acceptsGzip);
				free(headerValue);
				free(headerName);
				return -1;
			}
		}
		if (!strcmp(headerName, "Content-Type")) {
			for (size_t i = 0; i < sizeof(mimeTable)/sizeof(char const*); ++i) {
				if (!strcmp(headerValue, mimeTable[i])) {
					*contentType = (enum BodyType)i;
					break;
				}
			}
		}
		if (!strcmp(headerName, "Accept-Encoding")) {
			if (strstr(headerValue, "gzip") != NULL) {
				*acceptsGzip = true;
			}
		}

		free(headerValue);
		free(headerName);
	}
	if (fgetc(connection) != '\n') {
		errorSend(connection, (struct Response){.code = 400, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Bad request."}, .info = "Bad request at line " STRINGIFY(__LINE__)}, noBody, *acceptsGzip);
		return -1;
	}
	return 0;
}

int
parseBody(FILE* connection, bool noBody, bool acceptsGzip, size_t len, unsigned char** body)
{
	*body = NULL;
	if (!len) {
		return 0;
	}

	*body = malloc(len + 1);
	if (*body == NULL) {
		errorSend(connection, (struct Response){.code = 500, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Internal server error."}, "malloc failed while getting body"}, noBody, acceptsGzip);
		return -1;
	}
	if (fread(*body, 1, len, connection) != len)  {
		if (!feof(connection)) {
			errorSend(connection, (struct Response){.code = 500, .body = {SIZE_MAX, Body_Plain, (unsigned char*)"Internal server error."}, "stream error while getting body"}, noBody, acceptsGzip);
		}
		free(*body);
		return -1;
	}
	(*body)[len] = '\0';
	return 0;
}

int
parseRequest(FILE* connection, struct sockaddr_in6 address, struct Request* request, bool* noBody, bool* acceptsGzip)
{
	inet_ntop(AF_INET6, &address.sin6_addr, request->sender, sizeof(request->sender));

	if (parseMethod(connection, &request->method, noBody) < 0) {
		return -1;
	}

	if (parsePath(connection, *noBody, &request->path) < 0) {
		return -1;
	}

	if (parseVersion(connection, *noBody) < 0) {
		free(request->path);
		return -1;
	}

	if (parseHeaders(connection, *noBody, &request->body.len, &request->body.type, acceptsGzip) < 0) {
		free(request->path);
		return -1;
	}
	
	if (parseBody(connection, *noBody, *acceptsGzip, request->body.len, &request->body.data) < 0) {
		free(request->path);
		return -1;
	}

	return 0;
}

void
freeRequest(struct Request request)
{
	free(request.path);
	free(request.body.data);
}
