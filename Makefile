CC=gcc
CFLAGS=-Wall -Wextra
LDFLAGS=-lrt
OUT0=pingpong
OUT1=switch

all: build

build: pre
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(OUT0) pingpong.o gthreads.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(OUT1) switch.o gthreads.o

pre: pingpong.c gthreads.c gthreads.h
	$(CC) $(CFLAGS) -c pingpong.c switch.c gthreads.c

debug: CFLAGS+=-DDEBUG_ON
debug: build

clean:
	$(RM) *.o $(OUT0) $(OUT1)

