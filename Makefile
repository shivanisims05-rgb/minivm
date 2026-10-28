# mbc - mini bytecode compiler + virtual machine
# Needs only a C99 compiler (gcc / clang / MinGW).

CC     ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic
SRC     = $(wildcard src/*.c)
HDR     = $(wildcard src/*.h)

all: mbc

mbc: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: mbc
	sh tests/run_tests.sh

demo: mbc
	sh demo.sh

web: mbc
	python3 server.py

clean:
	rm -f mbc mbc.exe *.mbc

.PHONY: all test demo web clean
