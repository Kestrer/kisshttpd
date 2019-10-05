// KISSHTTPD

#pragma once
#ifndef server_h
#define server_h

#ifdef __cplusplus
extern "C" {
#endif

#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

enum BodyType {Body_Unknown, Body_Plain, Body_JSON, Body_HTML};

struct Body
{
	size_t len; // if len == SIZE_MAX then strlen((char*)data) is used instead
	enum BodyType type;
	unsigned char* data;
};

enum Method {HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE};

struct Request
{
	char sender[INET6_ADDRSTRLEN]; // IPv6 address of the sender
	enum Method method;
	char* path; // Includes host, and query if there is one
	struct Body body;
};

struct Response
{
	int code; // HTTP response code (e.g. 200, 403, etc)
	struct Body body;
	char* info; // Extra info to be printed to the server log
	char* uri; // Set Location header for 3XX responses
	void (*freeFunc)(struct Response); // To call free() on the body, set this to freeResponseBody, or you can define your own custom functions
};

typedef struct Server Server;

Server*
makeServer(struct Response (*callback)(struct Request, void* userdata), void* userdata, uint16_t port);

void
stopServer(Server* server);

// common function for freeFunc
void
freeResponseBody(struct Response response);

#ifdef __cplusplus
}
#endif

#endif
