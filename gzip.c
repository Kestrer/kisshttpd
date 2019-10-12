#include "gzip.h"

#include <stdlib.h>
#include <zlib.h>

unsigned char*
gzip(unsigned char const* const data, size_t const length, size_t* const gzLen)
{
	z_stream stream = {.next_in = (unsigned char*)data, .zalloc = Z_NULL, .zfree = Z_NULL, .opaque = Z_NULL};

	if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		return NULL;

	gz_header gzipHeader = {.os = 3};

	if (deflateSetHeader(&stream, &gzipHeader) != Z_OK) {
		deflateEnd(&stream);
		return NULL;
	}

	size_t const bufLen = deflateBound(&stream, length);
	unsigned char* const buf = malloc(bufLen);
	if (buf == NULL) {
		deflateEnd(&stream);
		return NULL;
	}

	stream.next_in = (unsigned char*)data;
	stream.avail_in = (uInt)length;
	stream.next_out = buf;
	stream.avail_out = (uInt)bufLen;

	if (deflate(&stream, Z_FINISH) != Z_STREAM_END) {
		deflateEnd(&stream);
		free(buf);
		return NULL;
	}

	if (deflateEnd(&stream) != Z_OK) {
		free(buf);
		return NULL;
	}

	*gzLen = stream.total_out;

	return buf;
}
