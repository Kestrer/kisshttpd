// zlib is too complex, simple gzip function

#pragma once
#ifndef gzip_h
#define gzip_h

#include <stddef.h>

unsigned char*
gzip(unsigned char const* data, size_t length, size_t* gzLen);

#endif
