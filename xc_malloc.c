#define XC_SHM_IMPL _xc_malloc_shm_t
#define XC_MEM_IMPL _xc_malloc_mem_t
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xc_shm.h"
#include "php.h"
#include "align.h"
#include "utils.h"

struct _xc_malloc_mem_t {
	const xc_mem_handlers_t *handlers;
	xc_shm_t                *shm;
	xc_memsize_t size;
	xc_memsize_t avail;       /* total free */
};

/* {{{ _xc_malloc_shm_t */
struct _xc_malloc_shm_t {
	xc_shm_handlers_t *handlers;
	xc_shmsize_t       size;
	xc_shmsize_t       memoffset;
#ifdef HAVE_XCACHE_TEST
	HashTable blocks;
#endif
};
/* }}} */

#define CHECK(x, e) do { if ((x) == NULL) { zend_error(E_ERROR, "XCache: " e); goto err; } } while (0)

static void *xc_add_to_blocks(xc_mem_t *mem, void *p, size_t size) /* {{{ */
{
#ifdef HAVE_XCACHE_TEST
	if (p) {
		zend_hash_add(&mem->shm->blocks, (void *) &p, sizeof(p), (void *) &size, sizeof(size), NULL);
	}
#endif
	return p;
}
/* }}} */
static void xc_del_from_blocks(xc_mem_t *mem, void *p) /* {{{ */
{
#ifdef HAVE_XCACHE_TEST
	zend_hash_del(&mem->shm->blocks, (void *) &p, sizeof(p));
#endif
}
/* }}} */

static XC_MEM_MALLOC(xc_malloc_malloc) /* {{{ */
{
	return xc_add_to_blocks(mem, malloc(size), size);
}
/* }}} */
static XC_MEM_FREE(xc_malloc_free) /* {{{ return block size freed */
{
	xc_del_from_blocks(mem, (void *) p);
	free((void *) p);
	return 0;
}
/* }}} */
static XC_MEM_CALLOC(xc_malloc_calloc) /* {{{ */
{
	return xc_add_to_blocks(mem, calloc(memb, size), size);
}
/* }}} */
static XC_MEM_REALLOC(xc_malloc_realloc) /* {{{ */
{
	return xc_add_to_blocks(mem, realloc((void *) p, size), size);
}
/* }}} */
static XC_MEM_STRNDUP(xc_malloc_strndup) /* {{{ */
{
	char *p = xc_add_to_blocks(mem, malloc(len), len);
	if (!p) {
		return NULL;
	}
	return memcpy(p, str, len);
}
/* }}} */
static XC_MEM_STRDUP(xc_malloc_strdup) /* {{{ */
{
	return xc_malloc_strndup(mem, str, strlen(str) + 1);
}
/* }}} */

static XC_MEM_AVAIL(xc_malloc_avail) /* {{{ */
{
	return mem->avail;
}
/* }}} */
static XC_MEM_SIZE(xc_malloc_size) /* {{{ */
{
	return mem->size;
}
/* }}} */

static XC_MEM_FREEBLOCK_FIRST(xc_malloc_freeblock_first) /* {{{ */
{
	return (void *) -1;
}
/* }}} */
XC_MEM_FREEBLOCK_NEXT(xc_malloc_freeblock_next) /* {{{ */
{
	return NULL;
}
/* }}} */
XC_MEM_BLOCK_SIZE(xc_malloc_block_size) /* {{{ */
{
	return 0;
}
/* }}} */
XC_MEM_BLOCK_OFFSET(xc_malloc_block_offset) /* {{{ */
{
	return 0;
}
/* }}} */

static XC_MEM_INIT(xc_mem_malloc_init) /* {{{ */
{
#define MINSIZE (ALIGN(sizeof(xc_mem_t)))
	/* requires at least the header and 1 tail block */
	if (size < MINSIZE) {
		fprintf(stderr, "xc_mem_malloc_init requires %d bytes at least\n", MINSIZE);
		return NULL;
	}
	mem->shm = shm;
	mem->size = size;
	mem->avail = size - MINSIZE;
#undef MINSIZE

	return mem;
}
/* }}} */
static XC_MEM_DESTROY(xc_mem_malloc_destroy) /* {{{ */
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
	free(shm);
#ifdef HAVE_XCACHE_TEST
	zend_hash_destroy(&shm->blocks);
#endif
	return;
}
/* }}} */
static XC_SHM_INIT(xc_malloc_init) /* {{{ */
{
	xc_shm_t *shm;
	CHECK(shm = calloc(1, sizeof(xc_shm_t)), "shm OOM");
	shm->size = size;

#ifdef HAVE_XCACHE_TEST
	zend_hash_init(&shm->blocks, 64, NULL, NULL, 1);
#endif
	return shm;
err:
	return NULL;
}
/* }}} */

static XC_SHM_MEMINIT(xc_malloc_meminit) /* {{{ */
{
	xc_mem_t *mem;
	if (shm->memoffset + size > shm->size) {
		zend_error(E_ERROR, "XCache: internal error at %s#%d", __FILE__, __LINE__);
		return NULL;
	}
	shm->memoffset += size;
	CHECK(mem = calloc(1, sizeof(xc_mem_t)), "mem OOM");
	mem->handlers = shm->handlers->memhandlers;
	mem->handlers->init(shm, mem, size);
	return mem;
err:
	return NULL;
}
/* }}} */
static XC_SHM_MEMDESTROY(xc_malloc_memdestroy) /* {{{ */
{
	mem->handlers->destroy(mem);
	free(mem);
}
/* }}} */

#define xc_malloc_destroy xc_mem_malloc_destroy
#define xc_malloc_init xc_mem_malloc_init
static xc_mem_handlers_t xc_mem_malloc_handlers = XC_MEM_HANDLERS(malloc);
#undef xc_malloc_init
#undef xc_malloc_destroy
static xc_shm_handlers_t xc_shm_malloc_handlers = XC_SHM_HANDLERS(malloc);
void xc_shm_malloc_register() /* {{{ */
{
	if (xc_mem_scheme_register("malloc", &xc_mem_malloc_handlers) == 0) {
		zend_error(E_ERROR, "XCache: failed to register malloc mem_scheme");
	}

	CHECK(xc_shm_malloc_handlers.memhandlers = xc_mem_scheme_find("malloc"), "cannot find malloc handlers");
	if (xc_shm_scheme_register("malloc", &xc_shm_malloc_handlers) == 0) {
		zend_error(E_ERROR, "XCache: failed to register malloc shm_scheme");
	}
err:
	return;
}
/* }}} */
