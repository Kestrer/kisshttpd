CC::=clang
CXX::=clang++
CFLAGS::=-Weverything -Werror -Wno-padded -Wno-disabled-macro-expansion -Wno-cast-qual -g -Og
CXXFLAGS::=-std=c++2a -Wno-c++98-compat -Wno-c++11-compat -Wno-c++17-compat -Wno-global-constructors
LIBS::=-lz -pthread
LDFLAGS::=-rdynamic -Wl,-rpath=/usr/local/lib -L/usr/local/lib

PREFIX::=/usr/local
