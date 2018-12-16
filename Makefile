CC=gcc
CFLAGS=-Wall -Wextra
LDFLAGS=-lrt
OUT=pingpong

all: build

build: pre
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(OUT) pingpong.o gthreads.o

pre: pingpong.c gthreads.c gthreads.h
	$(CC) $(CFLAGS) -c pingpong.c gthreads.c

debug: CFLAGS+=-DDEBUG_ON
debug: build

clean:
	$(RM) *.o $(OUT)
