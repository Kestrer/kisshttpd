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

static void
errorSend(FILE* connection, struct Response response, bool const noBody, bool const acceptsGzip)
{
	sendResponse(connection, response, noBody, acceptsGzip);

	startServerLog();
	printf("[ERROR DUMP]: %s\n", response.info ? response.info : (char*)response.body.data);
}

static size_t
strtozu(char const* str)
{
	size_t val = 0;
	while (*str) {
		if (!isdigit(*str))
			return SIZE_MAX;
		val *= 10;
		val += (size_t)(*str - '0');
		++str;
	}
	return val;
}

static enum ParseError
parseMethod(struct DynamicString methodStr, enum Method* method, bool* noBody)
{
	if (!strcmp(methodStr.data, "GET")) {
		*method = HTTP_GET;
	} else if (!strcmp(methodStr.data, "HEAD")) {
		*method = HTTP_GET;
		*noBody = true;
	} else if (!strcmp(methodStr.data, "POST")) {
		*method = HTTP_POST;
	} else if (!strcmp(methodStr.data, "PUT")) {
		*method = HTTP_PUT;
	} else if (!strcmp(methodStr.data, "DELETE")) {
		*method = HTTP_DELETE;
	} else {
		free(methodStr.data);
		return ParseError_UnknownMethod;
	}

	free(methodStr.data);
	return ParseError_Success;
}

static enum ParseError
parsePath(struct DynamicString pathStr, char** path, char** host)
{
	if (pathStr[0] != '/') {
		// deal with absolute-form in request-target
		// see RFC 7230 section 5.3.2
		for (size_t i = 0; i < pathStr.len && pathStr.data[i] != ':'; ++i) {
			pathStr.data[i] = (char)tolower(pathStr.data[i]);
		}
		if (strncmp(pathStr.data, "http://", 7)) {
			free(pathStr.data);
			return ParseError_BadRequest;
		}
		char const* const hostEnd = strchr(pathStr.data + 7, '/');
		size_t const hostLen = hostEnd != NULL ? (size_t)(hostEnd - *path) : pathStr.len;

		*host = malloc(hostLen + 1);
		if (*host == NULL) {
			free(pathStr.data);
			return ParseError_NoMem;
		}
		memcpy(*host, pathStr.data, hostLen);
		(*host)[hostLen] = '\0';
		memmove(pathStr.data, pathStr.data + hostLen, pathStr.len - hostLen + 1);
		if (hostEnd == NULL) {
			pathStr.data[0] = '/';
			pathStr.data[1] = '\0';
		}
	}
	*path = pathStr.data;
	return ParseError_Success;
}

static int
parseVersion(struct DynamicString versionStr)
{
	char const httpVersion[] = "HTTP/1.1\r";
	if (strcmp(httpVersion, versionStr.data)) {
		free(versionStr.data);
		return ParseError_InvalidVersion;
	}
	free(versionStr.data);
	return ParseError_Success;
}

static int
parseHeader(struct DynamicString headerName, struct DynamicString headerValue, size_t* contentLength, enum BodyType* contentType, char** host, bool* acceptsGzip)
{
	// remove whitespace at end of value
	while (isws(headerValue.data[headerValue.len-1])) --headerValue.len;
	headerValue.data[headerValue.len] = '\0';

	// remove whitespace at start of value
	size_t firstChar = 0;
	while (isws(headerValue.data[firstChar])) ++firstChar;
	memmove(headerValue.data, headerValue.data + firstChar, headerValue.len - firstChar + 1);

	if (!strcmp(headerName.data, "Content-Length")) {
		*contentLength = strtozu(headerValue.data);
		free(headerValue.data);
		if (*contentLength == SIZE_MAX) {
			free(headerName.data);
			return ParseError_BadRequest;
		}
	} else if (!strcmp(headerName.data, "Content-Type")) {
		for (size_t i = 0; i < sizeof(mimeTable)/sizeof(char const*); ++i) {
			if (!strcmp(headerValue.data, mimeTable[i])) {
				*contentType = (enum BodyType)i;
				break;
			}
		}
		free(headerValue.data);
	} else if (!strcmp(headerName.data, "Accept-Encoding")) {
		if (strstr(headerValue.data, "gzip")) *acceptsGzip = true;
		free(headerValue.data);
	} else if (!strcmp(headerName, "Host")) {
		free(*host);
		*host = headerValue.data;
	} else {
		free(headerValue.data);
	}

	free(headerName.data);
	return ParseError_Success;
}

static void
dynamicStringAddc(struct DynamicString str, char c)
{
	while (str.len+1 >= str.mem) {
		str.mem += 256;
		char* const newData = realloc(str.data, str.mem);
		if (newData == NULL) {
			free(str.data);
			str.data = NULL;
			str.len = 0;
			str.mem = 0;
			return;
		}
		str.data = newData;
	}
	str.data[str.len++] = c;
	str.data[str.len] = '\0';
}

void
startParse(struct ParseState* state, char const* sender)
{
	memset(state, 0, sizeof(struct ParseState));
	strncpy(state->request.public.sender, sender, sizeof(state->request.public.sender));
}

enum ParseError
parseChar(struct ParseState* state, char c)
{
	switch (state->stage) {
	case ParseStage_Method:
		if (c == ' ') {
			enum ParseError const r = parseMethod(state->method, &state->request.public.method, &state->request.noBody);
			++state->stage;
			state->path = {NULL, 0, 0};
			return r;
		}
		dynamicStringAddc(state->method, c);
		if (state->method.data == NULL) return ParseError_NoMem;
		break;
	case ParseStage_Path:
		if (c == ' ') {
			enum ParseError const r = parsePath(state->path, &state->request.public.path, &state->request.public.host);
			++state->stage;
			state->version = {NULL, 0, 0};
			return r;
		}
		dynamicStringAddc(state->path, c);
		if (state->method.data == NULL) {
			free(state->request.public.host);
			free(state->request.public.path);
			return ParseError_NoMem;
		}
		break;
	case ParseStage_Version:
		if (c == '\n') {
			enum ParseError const r = parseVersion(state->version);
			++state->stage;
			state->headerName = {NULL, 0, 0};
			state->headerValue = {NULL, 0, 0};
			return r;
		}
		dynamicStringAddc(state->version, c);
		if (state->version.data == NULL) {
			free(state->request.public.host);
			free(state->request.public.path);
			return ParseError_NoMem;
		}
		break;
	case ParseStage_HeaderName:
		if (c == '\r' && !state->headerName.data) break;
		if (c == '\n' && !state->headerName.data) {
			stage->stage = ParseStage_Body;
			if (!state->request.public.body.len) return ParseError_Complete;
			state->request.public.body.data = malloc(state->request.public.body.len+1);
			if (state->request.public.body.data == NULL) {
				free(state->request.public.host);
				free(state->request.public.path);
				return ParseError_NoMem;
			}
			state->bodyLen = 0;
			break;
		}
		if (c == ':') {
			++state->stage;
			break;
		}
		dynamicStringAddc(state->headerName, c);
		if (state->headerName.data == NULL) {
			free(state->request.public.host);
			free(state->request.public.path);
			return ParseError_NoMem;
		}
		break;
	case ParseStage_HeaderValue:
		if (c == '\r') break;
		if (c == '\n') {
			enum ParseError const r = parseHeader(state->headerName, state->headerValue, &state->request.public.body.len, &state->request.public.body.type, &state->request.public.host, &state->request.acceptsGzip);
			--state->stage;
			state->headerName = {NULL, 0, 0};
			state->headerValue = {NULL, 0, 0};
			return r;
		}
		dynamicStringAddc(state->headerValue, c);
		if (state->headerValue.data == NULL) {
			free(state->headerName.data);
			free(state->request.public.host);
			free(state->request.public.path);
			return ParseError_NoMem;
		}
		break;
	case ParseStage_Body:
		state->request.public.body.data[state->bodyLen++] = c;
		if (state->bodyLen == state->request.public.body.len) {
			state->request.public.body.data[state->bodyLen] = '\0';
			return ParseError_Complete;
		}
		break;
	}
	return ParseError_Success;
}

void
freeServerRequest(struct ServerRequest request)
{
	free(request.public.host);
	free(request.public.path);
	free(request.public.body.data);
}
