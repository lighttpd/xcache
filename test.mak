#! /usr/bin/make -f

EXES=mem_test
OBJS=mem.o
CC=gcc
CFLAGS=-g -O0 -D TEST -Wall
TEST=valgrind

all: mem

mem_test: mem.c
	$(CC) $(CFLAGS) -o mem_test mem.c
	
mem: mem_test
	$(TEST) ./mem_test

clean:
	rm -f $(OBJS) $(EXES)

leakcheck:
	valgrind php -c test.ini test.ini
