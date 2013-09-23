#define _xc_allocator_t _xc_allocator_bestfit_t
#define _xc_allocator_block_t _xc_allocator_bestfit_block_t
#include "xc_allocator.h"
#undef _xc_allocator_t
#undef _xc_allocator_block_t

#ifdef TEST
#	include <limits.h>
#	include <stdio.h>
#	define XCACHE_DEBUG
typedef int zend_bool;
#	define ZEND_ATTRIBUTE_PTR_FORMAT(a, b, c)
#	define zend_error(type, error) fprintf(stderr, "%s", error)
#else
#	include <php.h>
#endif

#ifdef XCACHE_DEBUG
#	define ALLOC_DEBUG_BLOCK_CHECK
#endif


#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "xc_shm.h"
#include "util/xc_align.h"
#include "util/xc_trace.h"

#if 0
#undef ALLOC_DEBUG_BLOCK_CHECK
#endif

#define CHAR_PTR(p) ((char *) (p))
#define PADD(p, a) (CHAR_PTR(p) + a)
#define PSUB(p1, p2) (CHAR_PTR(p1) - CHAR_PTR(p2))

/* {{{ allocator */
typedef struct _xc_allocator_bestfit_block_t xc_allocator_bestfit_block_t;
struct _xc_allocator_bestfit_block_t {
#ifdef ALLOC_DEBUG_BLOCK_CHECK
	unsigned int magic;
#endif
	xc_memsize_t size; /* reserved even after alloc */
	xc_allocator_bestfit_block_t *next;  /* not used after alloc */
};

typedef struct _xc_allocator_bestfit_t {
	const xc_allocator_vtable_t *vtable;
	xc_shm_t *shm;
	xc_memsize_t size;
	xc_memsize_t avail;       /* total free */
	xc_allocator_bestfit_block_t headblock[1];  /* just as a pointer to first block*/
} xc_allocator_bestfit_t;

#include <stddef.h>

#define SizeOf(type, field) sizeof( ((type *) 0)->field )
#define BLOCK_HEADER_SIZE() (ALIGN( offsetof(xc_allocator_bestfit_block_t, size) + SizeOf(xc_allocator_bestfit_block_t, size) ))

#define BLOCK_MAGIC ((unsigned int) 0x87655678)

/* }}} */
static inline void xc_block_setup(xc_allocator_bestfit_block_t *b, xc_memsize_t size, xc_allocator_bestfit_block_t *next) /* {{{ */
{
#ifdef ALLOC_DEBUG_BLOCK_CHECK
	b->magic = BLOCK_MAGIC;
#endif
	b->size = size;
	b->next = next;
}
/* }}} */
#ifdef ALLOC_DEBUG_BLOCK_CHECK
static void xc_block_check(xc_allocator_bestfit_block_t *b) /* {{{ */
{
	if (b->magic != BLOCK_MAGIC) {
		fprintf(stderr, "0x%X != 0x%X magic wrong \n", b->magic, BLOCK_MAGIC);
	}
}
/* }}} */
#else
#	define xc_block_check(b) do { } while(0)
#endif


static XC_ALLOCATOR_MALLOC(xc_allocator_bestfit_malloc) /* {{{ */
{
	xc_allocator_bestfit_block_t *prev, *cur;
	xc_allocator_bestfit_block_t *newb, *b;
	xc_memsize_t realsize;
	xc_memsize_t minsize;
	void *p;
	/* [xc_allocator_bestfit_block_t:size|size] */
	realsize = BLOCK_HEADER_SIZE() + size;
	/* realsize is ALIGNed so next block start at ALIGNed address */
	realsize = ALIGN(realsize);

	TRACE("avail: %lu (%luKB). Allocate size: %lu realsize: %lu (%luKB)"
			, allocator->avail, allocator->avail / 1024
			, size
			, realsize, realsize / 1024
			);
	do {
		p = NULL;
		if (allocator->avail < realsize) {
			TRACE("%s", " oom");
			break;
		}

		b = NULL;
		minsize = ULONG_MAX;

		/* prev|cur */

		for (prev = allocator->headblock; prev->next; prev = cur) {
			/* while (prev->next != 0) { */
			cur = prev->next;
			xc_block_check(cur);
			if (cur->size == realsize) {
				/* found a perfect fit, stop searching */
				b = prev;
				break;
			}
			/* make sure we can split on the block */
			else if (cur->size > (sizeof(xc_allocator_bestfit_block_t) + realsize) &&
					cur->size < minsize) {
				/* cur is acceptable and memller */
				b = prev;
				minsize = cur->size;
			}
			prev = cur;
		}

		if (b == NULL) {
			TRACE("%s", " no fit chunk");
			break;
		}

		prev = b;

		cur = prev->next;
		p = PADD(cur, BLOCK_HEADER_SIZE());

		/* update the block header */
		allocator->avail -= realsize;

		/* perfect fit, just unlink */
		if (cur->size == realsize) {
			prev->next = cur->next;
			TRACE(" perfect fit. Got: %p", p);
			break;
		}

		/* make new free block after alloced space */

		/* save, as it might be overwrited by newb (cur->size is ok) */
		b = cur->next;

		/* prev|cur     |next=b */

		newb = (xc_allocator_bestfit_block_t *)PADD(cur, realsize);
		xc_block_setup(newb, cur->size - realsize, b);
		cur->size = realsize;
		/* prev|cur|newb|next
		 *            `--^
		 */

		TRACE(" -> avail: %lu (%luKB). new next: %p offset: %lu %luKB. Got: %p"
				, allocator->avail, allocator->avail / 1024
				, newb
				, PSUB(newb, allocator), PSUB(newb, allocator) / 1024
				, p
				);
		prev->next = newb;
		/* prev|cur|newb|next
		 *    `-----^
		 */

	} while (0);

	return p;
}
/* }}} */
static XC_ALLOCATOR_FREE(xc_allocator_bestfit_free) /* {{{ return block size freed */
{
	xc_allocator_bestfit_block_t *cur, *b;
	int size;

	cur = (xc_allocator_bestfit_block_t *) (CHAR_PTR(p) - BLOCK_HEADER_SIZE());
	TRACE("freeing: %p, size=%lu", p, cur->size);
	xc_block_check(cur);
	assert((char*)allocator < (char*)cur);
	assert((char*)cur < (char*)allocator + allocator->size);

	/* find free block right before the p */
	b = allocator->headblock;
	while (b->next != 0 && b->next < cur) {
		b = b->next;
	}

	/* restore block */
	cur->next = b->next;
	b->next = cur;
	size = cur->size;

	TRACE(" avail %lu (%luKB)", allocator->avail, allocator->avail / 1024);
	allocator->avail += size;

	/* combine prev|cur */
	if (PADD(b, b->size) == (char *)cur) {
		b->size += cur->size;
		b->next = cur->next;
		cur = b;
		TRACE("%s", " combine prev");
	}

	/* combine cur|next */
	b = cur->next;
	if (PADD(cur, cur->size) == (char *)b) {
		cur->size += b->size;
		cur->next = b->next;
		TRACE("%s", " combine next");
	}
	TRACE(" -> avail %lu (%luKB)", allocator->avail, allocator->avail / 1024);
	return size;
}
/* }}} */
static XC_ALLOCATOR_CALLOC(xc_allocator_bestfit_calloc) /* {{{ */
{
	xc_memsize_t realsize = memb * size;
	void *p = xc_allocator_bestfit_malloc(allocator, realsize);

	if (p) {
		memset(p, 0, realsize);
	}
	return p;
}
/* }}} */
static XC_ALLOCATOR_REALLOC(xc_allocator_bestfit_realloc) /* {{{ */
{
	void *newp = xc_allocator_bestfit_malloc(allocator, size);
	if (p && newp) {
		memcpy(newp, p, size);
		xc_allocator_bestfit_free(allocator, p);
	}
	return newp;
}
/* }}} */

static XC_ALLOCATOR_AVAIL(xc_allocator_bestfit_avail) /* {{{ */
{
	return allocator->avail;
}
/* }}} */
static XC_ALLOCATOR_SIZE(xc_allocator_bestfit_size) /* {{{ */
{
	return allocator->size;
}
/* }}} */

static XC_ALLOCATOR_FREEBLOCK_FIRST(xc_allocator_bestfit_freeblock_first) /* {{{ */
{
	return allocator->headblock->next;
}
/* }}} */
static XC_ALLOCATOR_FREEBLOCK_NEXT(xc_allocator_bestfit_freeblock_next) /* {{{ */
{
	return block->next;
}
/* }}} */
static XC_ALLOCATOR_BLOCK_SIZE(xc_allocator_bestfit_block_size) /* {{{ */
{
	return block->size;
}
/* }}} */
static XC_ALLOCATOR_BLOCK_OFFSET(xc_allocator_bestfit_block_offset) /* {{{ */
{
	return ((char *) block) - ((char *) allocator);
}
/* }}} */

static XC_ALLOCATOR_INIT(xc_allocator_bestfit_init) /* {{{ */
{
	xc_allocator_bestfit_block_t *b;
#define MINSIZE (ALIGN(sizeof(xc_allocator_bestfit_t)) + sizeof(xc_allocator_bestfit_block_t))
	/* requires at least the header and 1 tail block */
	if (size < MINSIZE) {
		fprintf(stderr, "xc_allocator_bestfit_init requires %lu bytes at least\n", (unsigned long) MINSIZE);
		return NULL;
	}
	TRACE("size=%lu", size);
	allocator->shm = shm;
	allocator->size = size;
	allocator->avail = size - MINSIZE;

	/* pointer to first block, right after ALIGNed header */
	b = allocator->headblock;
	xc_block_setup(b, 0, (xc_allocator_bestfit_block_t *) PADD(allocator, ALIGN(sizeof(xc_allocator_bestfit_t))));

	/* first block*/
	b = b->next;
	xc_block_setup(b, allocator->avail, 0);
#undef MINSIZE

	return allocator;
}
/* }}} */
static XC_ALLOCATOR_DESTROY(xc_allocator_bestfit_destroy) /* {{{ */
{
}
/* }}} */

static xc_allocator_vtable_t xc_allocator_bestfit = XC_ALLOCATOR_VTABLE(allocator_bestfit);
void xc_allocator_bestfit_register() /* {{{ */
{
	if (xc_allocator_register("bestfit", &xc_allocator_bestfit) == 0) {
		zend_error(E_ERROR, "XCache: failed to register allocator 'bestfit'");
	}
}
/* }}} */
