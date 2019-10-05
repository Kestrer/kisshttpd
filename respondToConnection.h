#pragma once
#ifndef respondToConnection_h
#define respondToConnection_h

#include "kisshttpd.h"

#include <sys/socket.h>

struct
Connection
{
	int fd;
	struct sockaddr_in6 addr;
	struct Response (*callback)(struct Request, void* userdata);
	void* userdata;
};

int
respondToConnection(struct Connection* connection);

#endif
