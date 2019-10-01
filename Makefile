CC=clang

all: objects/respondToConnection.o objects/gzip.o objects/sendResponse.o objects/kisshttpd.o objects/parseRequest.o libkisshttpd.a

clean: uninstall
	-@rm objects/* libkisshttpd.a

objects/respondToConnection.o:  respondToConnection.c respondToConnection.h kisshttpd.h kisshttpd_internal.h sendResponse.h
	-@mkdir -p objects
	$(CC) -c $< -o $@ -Wall -Wextra -Werror -g -Og

objects/gzip.o:  gzip.c gzip.h
	-@mkdir -p objects
	$(CC) -c $< -o $@ -Wall -Wextra -Werror -g -Og

objects/sendResponse.o:  sendResponse.c sendResponse.h kisshttpd.h gzip.h kisshttpd_internal.h
	-@mkdir -p objects
	$(CC) -c $< -o $@ -Wall -Wextra -Werror -g -Og

objects/kisshttpd.o:  kisshttpd.c kisshttpd.h respondToConnection.h
	-@mkdir -p objects
	$(CC) -c $< -o $@ -Wall -Wextra -Werror -g -Og

objects/parseRequest.o:  parseRequest.c parseRequest.h kisshttpd.h
	-@mkdir -p objects
	$(CC) -c $< -o $@ -Wall -Wextra -Werror -g -Og

libkisshttpd.a: objects/sendResponse.o objects/respondToConnection.o objects/kisshttpd.o objects/gzip.o objects/parseRequest.o
	ar rcs $@ $^

install:
	cp libkisshttpd.a /usr/local/lib/
	cp kisshttpd.h /usr/local/include/
uninstall:
	-@rm /usr/local/lib/libkisshttpd.a /usr/local/include/kisshttpd.h

.PHONY: all clean install uninstall
