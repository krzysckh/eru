CC=clang
CFLAGS=-O2 -Wall -Wextra -ggdb

all: eru
eru: eru.c
clean:
	rm -f eru
