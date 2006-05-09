
#undef ALLOC_DEBUG

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
/* mmap */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "php.h"
#include "myshm.h"

#ifndef max
#define max(a, b) ((a) < (b) ? (b) : (a))
#endif

// {{{ xc_shm_t
struct _xc_shm_t {
	void *ptr;
	void *ptr_ro;
	long  diff;
	xc_shmsize_t size;
};

#ifdef ALLOC_DEBUG
#	undef NDEBUG
#	define inline
#else
#	define NDEBUG
#endif
#include <assert.h>
/* }}} */
#define CHECK(x, e) do { if ((x) == NULL) { zend_error(E_ERROR, "XCache: " e); goto err; } } while (0)

int xc_shm_can_readonly(xc_shm_t *shm) /* {{{ */
{
	return shm->ptr_ro != NULL;
}
/* }}} */
int xc_shm_is_readwrite(xc_shm_t *shm, const void *p) /* {{{ */
{
	return p >= shm->ptr && (char *)p < (char *)shm->ptr + shm->size;
}
/* }}} */
int xc_shm_is_readonly(xc_shm_t *shm, const void *p) /* {{{ */
{
	return xc_shm_can_readonly(shm) && p >= shm->ptr_ro && (char *)p < (char *)shm->ptr_ro + shm->size;
}
/* }}} */
void *xc_shm_to_readwrite(xc_shm_t *shm, void *p) /* {{{ */
{
	if (shm->diff) {
		assert(xc_shm_is_readonly(p));
		p = p - shm->diff;
	}
	assert(xc_shm_is_readwrite(p));
	return p;
}
/* }}} */
void *xc_shm_to_readonly(xc_shm_t *shm, void *p) /* {{{ */
{
	assert(xc_shm_is_readwrite(p));
	if (shm->diff) {
		p = p + shm->diff;
		assert(xc_shm_is_readonly(p));
	}
	return p;
}
/* }}} */

void xc_shm_destroy(xc_shm_t *shm) /* {{{ */
{
	if (shm->ptr_ro) {
		munmap(shm->ptr_ro, shm->size);
		/*
		shm->ptr_ro = NULL;
		*/
	}
	if (shm->ptr) {
		/* shm->size depends on shm->ptr */
		munmap(shm->ptr, shm->size);
		/*
		shm->ptr = NULL;
		*/
	}
	/*
	shm->size = NULL;
	shm->diff = 0;
	*/

	free(shm);
	return;
}
/* }}} */
xc_shm_t *xc_shm_init(const char *path, xc_shmsize_t size, zend_bool readonly_protection) /* {{{ */
{
	xc_shm_t *shm = NULL;
	int fd;
	int ro_ok;
	volatile void *romem;
	int created = 0;

	CHECK(shm = calloc(1, sizeof(xc_shm_t)), "shm OOM");
	shm->size = size;
	if (path == NULL || !path[0]) {
		path = "/tmp/xcache";
	}
	fd = open(path, O_RDWR, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		created = 1;
		fd = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
		if (fd == -1) {
			if (created) {
				unlink(path);
			}
			goto err;
		}
	}
	ftruncate(fd, size);

	shm->ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (shm->ptr == MAP_FAILED) {
		shm->ptr = NULL;
		close(fd);
		if (created) {
			unlink(path);
		}
		goto err;
	}

	ro_ok = 0;
	if (readonly_protection) {
		shm->ptr_ro = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
		romem = shm->ptr_ro;

		/* {{{ check if ptr_ro works */
		do {
			if (shm->ptr_ro == MAP_FAILED || shm->ptr_ro == shm->ptr) {
				break;
			}
			*(char *)shm->ptr = 1;
			if (*(char *)romem != 1) {
				break;
			}
			*(char *)shm->ptr = 2;
			if (*(char *)romem != 2) {
				break;
			}
			ro_ok = 1;
		} while (0);
	}

	if (ro_ok) {
		shm->diff = shm->ptr_ro - shm->ptr;
		assert(abs(shm->diff) >= size);
	}
	else {
		if (shm->ptr_ro != MAP_FAILED) {
			munmap(shm->ptr_ro, size);
		}
		shm->ptr_ro = NULL;
		shm->diff = 0;
	}
	/* }}} */

	close(fd);
	if (created) {
		unlink(path);
	}

	return shm;

err:
	if (shm) {
		xc_shm_destroy(shm);
	}
	return NULL;
}
/* }}} */

void *xc_shm_ptr(xc_shm_t *shm) /* {{{ */
{
	return shm->ptr;
}
/* }}} */
