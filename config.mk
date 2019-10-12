CC::=clang
CXX::=clang++
CFLAGS::=-Wall -Wextra -pedantic -Werror -Weverything -Wno-padded -Wno-disabled-macro-expansion -Wno-cast-qual -g -Og
CXXFLAGS::=-std=c++2a
LIBS::=-lz -pthread
LDFLAGS::=-rdynamic -Wl,-rpath=/usr/local/lib -L/usr/local/lib

PREFIX::=/usr/local
