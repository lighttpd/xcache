#define XC_SHM_IMPL _xc_malloc_shm_t
#define _xc_allocator_t _xc_allocator_malloc_t
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xc_shm.h"
#include "xc_allocator.h"
#ifndef TEST
#include "xcache.h"
#endif
#include "util/xc_align.h"

struct _xc_allocator_malloc_t {
	const xc_allocator_vtable_t *vtable;
	xc_shm_t *shm;
	xc_memsize_t size;
	xc_memsize_t avail;       /* total free */
};

/* {{{ _xc_malloc_shm_t */
struct _xc_malloc_shm_t {
	xc_shm_handlers_t *handlers;
	xc_shmsize_t       size;
	xc_shmsize_t       memoffset;
#ifndef TEST
	HashTable blocks;
#endif
};
/* }}} */

#ifndef TEST
#	define CHECK(x, e) do { if ((x) == NULL) { zend_error(E_ERROR, "XCache: " e); goto err; } } while (0)
#else
#	define CHECK(x, e) do { if ((x) == NULL) { fprintf(stderr, "%s\n", "XCache: " e); goto err; } } while (0)
#endif

static void *xc_add_to_blocks(xc_allocator_t *allocator, void *p, size_t size) /* {{{ */
{
	if (p) {
		allocator->avail -= size;
#ifndef TEST
		zend_hash_add(&allocator->shm->blocks, (void *) &p, sizeof(p), (void *) &size, sizeof(size), NULL);
#endif
	}
	return p;
}
/* }}} */
static void xc_del_from_blocks(xc_allocator_t *allocator, void *p) /* {{{ */
{
#ifdef TEST
	/* TODO: allocator->avail += size; */
#else
	zend_hash_del(&allocator->shm->blocks, (void *) &p, sizeof(p));
#endif
}
/* }}} */

static XC_ALLOCATOR_MALLOC(xc_allocator_malloc_malloc) /* {{{ */
{
	if (allocator->avail < size) {
		return NULL;
	}
	return xc_add_to_blocks(allocator, malloc(size), size);
}
/* }}} */
static XC_ALLOCATOR_FREE(xc_allocator_malloc_free) /* {{{ return block size freed */
{
	xc_del_from_blocks(allocator, (void *) p);
	free((void *) p);
	return 0;
}
/* }}} */
static XC_ALLOCATOR_CALLOC(xc_allocator_malloc_calloc) /* {{{ */
{
	if (allocator->avail < size) {
		return NULL;
	}
	return xc_add_to_blocks(allocator, calloc(memb, size), size);
}
/* }}} */
static XC_ALLOCATOR_REALLOC(xc_allocator_malloc_realloc) /* {{{ */
{
	return xc_add_to_blocks(allocator, realloc((void *) p, size), size);
}
/* }}} */

static XC_ALLOCATOR_AVAIL(xc_allocator_malloc_avail) /* {{{ */
{
	return allocator->avail;
}
/* }}} */
static XC_ALLOCATOR_SIZE(xc_allocator_malloc_size) /* {{{ */
{
	return allocator->size;
}
/* }}} */

static XC_ALLOCATOR_FREEBLOCK_FIRST(xc_allocator_malloc_freeblock_first) /* {{{ */
{
	return (void *) -1;
}
/* }}} */
static XC_ALLOCATOR_FREEBLOCK_NEXT(xc_allocator_malloc_freeblock_next) /* {{{ */
{
	return NULL;
}
/* }}} */
static XC_ALLOCATOR_BLOCK_SIZE(xc_allocator_malloc_block_size) /* {{{ */
{
	return 0;
}
/* }}} */
static XC_ALLOCATOR_BLOCK_OFFSET(xc_allocator_malloc_block_offset) /* {{{ */
{
	return 0;
}
/* }}} */

static XC_ALLOCATOR_INIT(xc_allocator_malloc_init) /* {{{ */
{
#define MINSIZE (ALIGN(sizeof(xc_allocator_t)))
	/* requires at least the header and 1 tail block */
	if (size < MINSIZE) {
		fprintf(stderr, "xc_allocator_malloc_init requires %lu bytes at least\n", (unsigned long) MINSIZE);
		return NULL;
	}
	allocator->shm = shm;
	allocator->size = size;
	allocator->avail = size - MINSIZE;
#undef MINSIZE

	return allocator;
}
/* }}} */
static XC_ALLOCATOR_DESTROY(xc_allocator_malloc_destroy) /* {{{ */
{
}
/* }}} */

static XC_SHM_CAN_READONLY(xc_malloc_can_readonly) /* {{{ */
{
	return 0;
}
/* }}} */
static XC_SHM_IS_READWRITE(xc_malloc_is_readwrite) /* {{{ */
{
#ifndef TEST
	HashPosition pos;
	size_t *psize;
	char **ptr;

	zend_hash_internal_pointer_reset_ex(&shm->blocks, &pos);
	while (zend_hash_get_current_data_ex(&shm->blocks, (void **) &psize, &pos) == SUCCESS) {
		zend_hash_get_current_key_ex(&shm->blocks, (void *) &ptr, NULL, NULL, 0, &pos);
		if ((char *) p >= *ptr && (char *) p < *ptr + *psize) {
			return 1;
		}
		zend_hash_move_forward_ex(&shm->blocks, &pos);
	}
#endif

	return 0;
}
/* }}} */
static XC_SHM_IS_READONLY(xc_malloc_is_readonly) /* {{{ */
{
	return 0;
}
/* }}} */
static XC_SHM_TO_READWRITE(xc_malloc_to_readwrite) /* {{{ */
{
	return p;
}
/* }}} */
static XC_SHM_TO_READONLY(xc_malloc_to_readonly) /* {{{ */
{
	return p;
}
/* }}} */

static XC_SHM_DESTROY(xc_malloc_destroy) /* {{{ */
{
#ifndef TEST
	zend_hash_destroy(&shm->blocks);
#endif
	free(shm);
	return;
}
/* }}} */
static XC_SHM_INIT(xc_malloc_init) /* {{{ */
{
	xc_shm_t *shm;
	CHECK(shm = calloc(1, sizeof(xc_shm_t)), "shm OOM");
	shm->size = size;

#ifndef TEST
	zend_hash_init(&shm->blocks, 64, NULL, NULL, 1);
#endif
	return shm;
err:
	return NULL;
}
/* }}} */

static XC_SHM_MEMINIT(xc_malloc_meminit) /* {{{ */
{
	void *mem;
	if (shm->memoffset + size > shm->size) {
#ifndef TEST
		zend_error(E_ERROR, "XCache: internal error at %s#%d", __FILE__, __LINE__);
#endif
		return NULL;
	}
	shm->memoffset += size;
	CHECK(mem = calloc(1, size), "mem OOM");
	return mem;
err:
	return NULL;
}
/* }}} */
static XC_SHM_MEMDESTROY(xc_malloc_memdestroy) /* {{{ */
{
	free(mem);
}
/* }}} */

static xc_allocator_vtable_t xc_allocator_malloc_vtable = XC_ALLOCATOR_VTABLE(allocator_malloc);
#ifndef TEST
static xc_shm_handlers_t xc_shm_malloc_handlers = XC_SHM_HANDLERS(malloc);
#endif
void xc_allocator_malloc_register() /* {{{ */
{
	if (xc_allocator_register("malloc", &xc_allocator_malloc_vtable) == 0) {
#ifndef TEST
		zend_error(E_ERROR, "XCache: failed to register malloc mem_scheme");
#endif
	}
}
/* }}} */

#ifndef TEST
void xc_shm_malloc_register() /* {{{ */
{
	if (xc_shm_scheme_register("malloc", &xc_shm_malloc_handlers) == 0) {
		zend_error(E_ERROR, "XCache: failed to register malloc shm_scheme");
	}
}
/* }}} */
#endif
