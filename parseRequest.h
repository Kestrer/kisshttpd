#pragma once
#ifndef parseRequest_h
#define parseRequest_h

#include "kisshttpd.h"

#include <stdbool.h>
#include <stdio.h>

void
serverLog(struct Request request, struct Response response);

int
parseRequest(FILE* connection, struct sockaddr_in6 address, struct Request* request, bool* noBody, bool* acceptsGzip);

void
freeRequest(struct Request request);

#endif
