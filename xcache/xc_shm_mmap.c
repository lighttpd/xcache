
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

/* mmap */
#ifdef ZEND_WIN32
#	define ftruncate chsize
#	define getuid() 0
#	include <process.h>
#	define XCacheCreateFileMapping(size, perm, name) \
		CreateFileMapping(INVALID_HANDLE_VALUE, NULL, perm, (sizeof(xc_shmsize_t) > 4) ? size >> 32 : 0, size & 0xffffffff, name)
#	define XCACHE_MAP_FAILED NULL
#	define munmap(p, s) UnmapViewOfFile(p)
#else
#	include <unistd.h>
/* make sure to mark(change) it to NULL to keep consistent */
#	define XCACHE_MAP_FAILED MAP_FAILED
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef ZEND_WIN32
#include <sys/mman.h>
#endif

#include "php.h"
#define XC_SHM_IMPL _xc_mmap_shm_t
#include "xc_shm.h"
#include "xc_utils.h"

#ifndef max
#define max(a, b) ((a) < (b) ? (b) : (a))
#endif

/* {{{ xc_shm_t */
struct _xc_mmap_shm_t {
	xc_shm_handlers_t *handlers;
	zend_bool disabled;
	void *ptr;
	void *ptr_ro;
	long  diff;
	xc_shmsize_t size;
	xc_shmsize_t memoffset;
	char *name;
#ifdef ZEND_WIN32
	HANDLE hmap;
	HANDLE hmap_ro;
#else
	int newfile;
#endif
};

/* }}} */
#define CHECK(x, e) do { if ((x) == NULL) { zend_error(E_ERROR, "XCache: " e); goto err; } } while (0)
#define PTR_ADD(ptr, v) (((char *) (ptr)) + (v))
#define PTR_SUB(ptr, v) (((char *) (ptr)) - (v))

static XC_SHM_CAN_READONLY(xc_mmap_can_readonly) /* {{{ */
{
	return shm->ptr_ro != NULL;
}
/* }}} */
static XC_SHM_IS_READWRITE(xc_mmap_is_readwrite) /* {{{ */
{
	return p >= shm->ptr && (char *)p < (char *)shm->ptr + shm->size;
}
/* }}} */
static XC_SHM_IS_READONLY(xc_mmap_is_readonly) /* {{{ */
{
	return xc_mmap_can_readonly(shm) && p >= shm->ptr_ro && (char *)p < (char *)shm->ptr_ro + shm->size;
}
/* }}} */
static XC_SHM_TO_READWRITE(xc_mmap_to_readwrite) /* {{{ */
{
	if (shm->diff) {
		assert(xc_mmap_is_readonly(shm, p));
		p = PTR_SUB(p, shm->diff);
	}
	assert(xc_mmap_is_readwrite(shm, p));
	return p;
}
/* }}} */
static XC_SHM_TO_READONLY(xc_mmap_to_readonly) /* {{{ */
{
	assert(xc_mmap_is_readwrite(shm, p));
	if (shm->diff) {
		p = PTR_ADD(p, shm->diff);
		assert(xc_mmap_is_readonly(shm, p));
	}
	return p;
}
/* }}} */

static XC_SHM_DESTROY(xc_mmap_destroy) /* {{{ */
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
#ifdef ZEND_WIN32
	if (shm->hmap) {
		CloseHandle(shm->hmap);
	}
	if (shm->hmap_ro) {
		CloseHandle(shm->hmap_ro);
	}
#endif

	if (shm->name) {
#ifndef ZEND_WIN32
#	ifdef __CYGWIN__
		if (shm->newfile) {
			unlink(shm->name);
		}
#	endif
#endif
		free(shm->name);
	}
	/*
	shm->size = NULL;
	shm->diff = 0;
	*/

	free(shm);
	return;
}
/* }}} */
static XC_SHM_INIT(xc_mmap_init) /* {{{ */
{
#ifdef ZEND_WIN32
#	define TMP_PATH "XCache"
#else
#	define TMP_PATH "/tmp/XCache"
	int fd = -1;
#endif
	xc_shm_t *shm = NULL;
	int ro_ok;
	volatile void *romem;
	char tmpname[sizeof(TMP_PATH) - 1 + 4 * 10 + 100] = { 0 };
	const char *errstr = NULL;
	const char *path = (const char *) arg1;
	static int instanceId = 0;

	CHECK(shm = calloc(1, sizeof(xc_shm_t)), "shm OOM");
	shm->size = size;

	if (path == NULL || !path[0]) {
		snprintf(tmpname, sizeof(tmpname) - 1, "%s.%d.%d.%d", TMP_PATH, (int) getuid(), (int) getpid(), ++instanceId);
		path = tmpname;
	}
#ifdef ZEND_WIN32
	else {
		snprintf(tmpname, sizeof(tmpname) - 1, "%s.%d.%d.%d", path, (int) getuid(), (int) getpid(), ++instanceId);
		path = tmpname;
	}
#endif

	shm->name = strdup(path);

#ifndef ZEND_WIN32
#	define XCACHE_MMAP_PERMISSION (S_IRUSR | S_IWUSR)
	fd = open(shm->name, O_RDWR, XCACHE_MMAP_PERMISSION);
	if (fd == -1) {
		/* do not create file in /dev */
		if (strncmp(shm->name, "/dev", 4) == 0) {
			perror(shm->name);
			errstr = "Cannot open file set by xcache.mmap_path, check the xcache.size/var_size against system limitation";
			goto err;
		}
		fd = open(shm->name, O_CREAT | O_RDWR, XCACHE_MMAP_PERMISSION);
		shm->newfile = 1;
		if (fd == -1) {
			perror(shm->name);
			errstr = "Cannot open or create file set by xcache.mmap_path, check the path permission or check xcache.size/var_size against system limitation";
			goto err;
		}
	}

	if (ftruncate(fd, size) != 0 && errno != EINVAL) {
		perror(shm->name);
		errstr = "Failed to ftruncate the file";
		goto err;
	}
#endif

#ifdef ZEND_WIN32
	shm->hmap = XCacheCreateFileMapping(size, PAGE_READWRITE, shm->name);
	shm->ptr = (LPSTR) MapViewOfFile(shm->hmap, FILE_MAP_WRITE, 0, 0, 0);
#else
	shm->ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
#endif

	if (shm->ptr == XCACHE_MAP_FAILED) {
		perror(shm->name);
		errstr = "Failed creating file mapping";
		shm->ptr = NULL;
		goto err;
	}

	/* {{{ readonly protection, mmap it readonly and check if ptr_ro works */
	if (readonly_protection) {
		ro_ok = 0;

#ifdef ZEND_WIN32
		shm->hmap_ro = XCacheCreateFileMapping(size, PAGE_READONLY, shm->name);
		shm->ptr_ro = (LPSTR) MapViewOfFile(shm->hmap_ro, FILE_MAP_READ, 0, 0, 0);
#else
		shm->ptr_ro = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
#endif
		if (shm->ptr_ro == XCACHE_MAP_FAILED) {
			shm->ptr_ro = NULL;
		}
		romem = shm->ptr_ro;

		do {
			if (romem == NULL || romem == shm->ptr) {
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

		if (ro_ok) {
			shm->diff = PTR_SUB(shm->ptr_ro, (char *) shm->ptr);
			/* no overlap */
			assert((xc_shmsize_t) abs(shm->diff) >= size);
		}
		else {
			if (shm->ptr_ro) {
				munmap(shm->ptr_ro, size);
			}
#ifdef ZEND_WIN32
			if (shm->hmap_ro) {
				CloseHandle(shm->hmap_ro);
			}
#endif
			shm->ptr_ro = NULL;
			shm->diff = 0;
		}
	}

	/* }}} */

#ifndef ZEND_WIN32
	close(fd);

#	ifndef __CYGWIN__
	if (shm->newfile) {
		unlink(shm->name);
	}
#	endif
#endif

	return shm;

err:
#ifndef ZEND_WIN32
	if (fd != -1) {
		close(fd);
	}
#endif
	if (shm) {
		xc_mmap_destroy(shm);
	}
	if (errstr) {
		fprintf(stderr, "%s\n", errstr);
		zend_error(E_ERROR, "%s", errstr);
	}
	return NULL;
}
/* }}} */

static XC_SHM_MEMINIT(xc_mmap_meminit) /* {{{ */
{
	void *mem;
	if (shm->memoffset + size > shm->size) {
		zend_error(E_ERROR, "XCache: internal error at %s#%d", __FILE__, __LINE__);
		return NULL;
	}
	mem = PTR_ADD(shm->ptr, shm->memoffset);
	shm->memoffset += size;
	return mem;
}
/* }}} */
static XC_SHM_MEMDESTROY(xc_mmap_memdestroy) /* {{{ */
{
}
/* }}} */

static xc_shm_handlers_t xc_shm_mmap_handlers = XC_SHM_HANDLERS(mmap);
void xc_shm_mmap_register() /* {{{ */
{
	if (xc_shm_scheme_register("mmap", &xc_shm_mmap_handlers) == 0) {
		zend_error(E_ERROR, "XCache: failed to register mmap shm_scheme");
	}
	return;
}
/* }}} */
