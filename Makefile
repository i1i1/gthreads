CC=gcc
CFLAGS=-Wall -Wextra -g
LDFLAGS=-lrt

all: pingpong switch sieve

pingpong: pingpong.o gthreads.o
switch: switch.o gthreads.o
sieve: sieve.o gthreads.o

debug: CFLAGS+=-DDEBUG_ON
debug: all

clean:
	$(RM) *.o pingpong switch sieve
