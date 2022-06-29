#pragma once
#ifndef parseRequest_h
#define parseRequest_h

#include "kisshttpd.h"

#include <stdbool.h>
#include <stdio.h>

enum ParseStage {
	ParseStage_Method,
	ParseStage_Path,
	ParseStage_Version,
	ParseStage_HeaderName,
	ParseStage_HeaderValue,
	ParseStage_Body,
};

struct DynamicString {
	char* data;
	size_t len;
	size_t mem;
};

struct ParseState {
	enum ParseStage stage;

	union {
		struct DynamicString method;
		struct DynamicString path;
		struct DynamicString version;
		struct {
			struct DynamicString headerName;
			struct DynamicString headerValue;
		};
		size_t bodyLen;
	};

	struct ServerRequest request;
};

struct ServerRequest {
	struct Request public;
	bool noBody;
	bool acceptsGzip;
};

enum ParseError {
	ParseError_Success,
	ParseError_NoMem,
	ParseError_UnknownMethod,
	ParseError_BadRequest,
	ParseError_InvalidVersion,
	ParseError_Complete,
};

void
startParse(struct ParseState* state, char const* sender);

enum ParseError
parseChar(struct ParseState* state, char c);

void
freeParseState(struct ParseState state);

void
freeServerRequest(struct ServerRequest request);

#endif
