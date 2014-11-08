#! /usr/bin/make -f

CC=gcc
LDFLAGS=
CFLAGS=-g -O0 -I. -D TEST -D HAVE_XCACHE_TEST -Wall
TEST=valgrind

all: allocator vector

allocator_test: xcache/xc_allocator.h xcache/xc_allocator.c xcache/xc_malloc.c xcache/xc_allocator_bestfit.c util/xc_trace.c util/xc_trace.h
	$(CC) $(LDFLAGS) $(CFLAGS) -o allocator_test xcache/xc_allocator.c xcache/xc_malloc.c xcache/xc_allocator_bestfit.c util/xc_trace.c
	
allocator: allocator_test
	$(TEST) ./allocator_test

vector_test: util/xc_vector_test.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o vector_test util/xc_vector_test.c
	
vector: vector_test
	$(TEST) ./vector_test

clean:
	rm -f allocator_test vector_test
