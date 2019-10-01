# Kisshttpd

The simplest and easiest HTTP server around. It fully supports IPv6 and HTTP/1.1.

I made this because I wanted to make an HTTP server, but all the others that I found were too complex and confusing, so I aimed to make one that is as simple as possible. It deliberately contains no CGI support.

## Building From Source

Kisshttpd can only be built on UNIX-like systems (i.e. not Windows).

Dependencies

- zlib
- A C11 compiler (by default clang)

Clone and cd into this repository:

	git clone https://github.com/Koxiaet/kisshttpd
	cd kisshttpd

Run make:

	make

and install it to your system:

	sudo make install

To use it in a project, add this to the linker command:

	-lkisshttpd -pthread -lz

## How To Use

Start a server with `makeServer`:

```C
Server*
makeServer(struct Response (*callback)(struct Request, void* userdata), void* userdata, uint16_t port);
```

If `makeServer` fails it will return NULL, and you can usually obtain why it failed with `perror`. Keep in mind that port numbers under 1024 require root to use.

Whenever you get a request, a new thread will be created and the function `callback` called, with details about the request and the userdata that you passed into `makeServer`. This also means that you will have to make sure that callback is thread-safe.

Stop a server with `stopServer`:

```C
void
stopServer(Server* server);
```

The definitions for `struct Request` and `struct Response` are in `kisshttpd.h`.

A few things to note:

- You cannot tell the difference between a GET and HEAD request, as HEAD requests are automatically translated into GET requests, but the body of the message is never sent (in other words, don't worry about HEAD requests).
- This library automatically compresses files with gzip if it can.
- Once you have started the server, you cannot change where userdata points to. If you want to do that, have userdata point to a pointer, which you can change.
- This library deliberately prevents you from not sending a response or sending an invalid response to a client, for obvious reasons.
- If the client is using IPv4 then the library will not give the real IPv4 address. This is due to how `inet_ntop` handles IPv4-mapped-IPv6 addresses, and it cannot be changed.
- The way this library works is that it makes a master thread that continually checks the opened port for incoming connections. When it receives one, it creates a thread that parses the request and calls `callback`. Then that thread forms an HTTP request and sends it back to the client.

Server logging, HTTP request parsing, HTTP response generating, and thread managing are all done by the library.

## Todo list

- Better managing of Host header
- Common headers in `struct Request`
- More compression formats other than gzip
- Ability to redirect server log to a file
- HTTP/2 and TLS support
- HTTP/3 support

## License

WTFPL
