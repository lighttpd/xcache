#! /usr/bin/make -f

EXES=allocator_test
OBJS=allocator.o
CC=gcc
CFLAGS=-g -O0 -I. -D TEST -D HAVE_XCACHE_TEST -Wall
TEST=valgrind

all: allocator

allocator_test: xcache/xc_allocator.h xcache/xc_allocator.c xcache/xc_malloc.c xcache/xc_allocator_bestfit.c util/xc_trace.c util/xc_trace.h
	$(CC) $(CFLAGS) -o allocator_test xcache/xc_allocator.c xcache/xc_malloc.c xcache/xc_allocator_bestfit.c util/xc_trace.c
	
allocator: allocator_test
	$(TEST) ./allocator_test

clean:
	rm -f $(OBJS) $(EXES)
