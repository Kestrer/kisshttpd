#include "sendResponse.h"

#include "gzip.h"
#include "kisshttpd_internal.h"

#include <stdlib.h>
#include <string.h>

static uv_buf_t
vbufprintf(char const* fmt, va_list args)
{
	int const size = vsnprintf(NULL, 0, fmt, args);
	if (size < 0) return {.base = NULL, .len = 0};

	uv_buf_t buf;
	buf.len = size;
	buf.base = malloc(buf.len);
	if (!buf.base) return {.base = NULL, .len = 0};

	if (vsprintf(buf.base, fmt, args) < 0) {
		free(buf.base);
		return {.base = NULL, .len = 0};
	}
	return buf;
}

static uv_buf_t
bufprintf(char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	uv_buf_t const ret = vbufprintf(fmt, args);
	va_end(args);
	return ret;
}

void
sendResponse(FILE* connection, struct Response response, bool const noBody, bool const acceptsGzip)
{
	uv_buf_t bufs[10];
	size_t nbufs = 0;

	bufs[nbufs++] = bufprintf("HTTP/1.1 %i \r\n", response.code);
	if (!bufs[nbufs].base) return;

	if (response.uri != NULL) {
		++nbufs;
		bufs[nbufs++] = bufprintf("Location: %s\r\n", response.uri);
		if (!bufs[nbufs].base) {
			for (size_t i = 0; i < nbufs; ++i) free(bufs[i].base);
			return;
		}
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

				bufs[nbufs++] = bufprintf("Content-Encoding: gzip\r\nVary: Accept-Encoding\r\n");
				if (!bufs[nbufs].base) {
					free(gzData);
					for (size_t i = 0; i < nbufs; ++i) free(bufs[i].base);
					return;
				}
			} else {
				free(gzData);
			}
		}
	}

	if (response.body.len) {
		bufs[nbufs++] = bufprintf("Content-Length: %zu\r\n", response.body.len);
		if (!bufs[nbufs].base) {
			if (gzipped) free((unsigned char*)response.body.data);
			for (size_t i = 0; i < nbufs; ++i) free(bufs[i].base);
			return;
		}
		if (response.body.type != Body_Unknown) {
			bufs[nbufs++] = bufprintf("Content-Type: %s; charset=utf-8\r\n", mimeTable[response.body.type]);
			if (!bufs[nbufs].base) {
				if (gzipped) free((unsigned char*)response.body.data);
				for (size_t i = 0; i < nbufs; ++i) free(bufs[i].base);
				return;
			}
		}
		bufs[nbufs++] = bufprintf("\r\n");
		if (!bufs[nbufs].base) {
			if (gzipped) free((unsigned char*)response.body.data);
			for (size_t i = 0; i < nbufs; ++i) free(bufs[i].base);
			return;
		}
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
