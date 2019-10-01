#pragma once
#ifndef kisshttpd_internal_h
#define kisshttpd_internal_h

// each row corresponds to a BodyType
static char const* const mimeTable[] =
{
	"unknown",
	"text/plain",
	"application/json",
	"text/html",
};

// each row corresponds to a Method
static char const* const methodTable[] =
{
	"GET",
	"POST",
	"PUT",
	"DELETE",
};

#endif
