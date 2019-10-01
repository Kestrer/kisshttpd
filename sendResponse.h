#pragma once
#ifndef sendResponse_h
#define sendResponse_h

#include "kisshttpd.h"

#include <stdbool.h>
#include <stdio.h>

void
sendResponse(FILE* connection, struct Response response, bool const noBody, bool const acceptsGzip);

#endif
