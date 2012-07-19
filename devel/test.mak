#! /usr/bin/make -f

EXES=mem_test
OBJS=mem.o
CC=gcc
CFLAGS=-g -O0 -I. -D TEST -Wall
TEST=valgrind

all: mem

mem_test: xcache/xc_mem.c xcache/xc_mem.h util/xc_trace.c util/xc_trace.h
	$(CC) $(CFLAGS) -o mem_test xcache/xc_mem.c util/xc_trace.c
	
mem: mem_test
	$(TEST) ./mem_test

clean:
	rm -f $(OBJS) $(EXES)
