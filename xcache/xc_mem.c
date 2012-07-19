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
#define XC_MEMBLOCK_IMPL _xc_mem_block_t
#define XC_MEM_IMPL _xc_mem_mem_t
#include "xc_shm.h"
// #include "xc_utils.h"
#include "util/xc_align.h"
#include "util/xc_trace.h"

#if 0
#undef ALLOC_DEBUG_BLOCK_CHECK
#endif

#define CHAR_PTR(p) ((char *) (p))
#define PADD(p, a) (CHAR_PTR(p) + a)
#define PSUB(p1, p2) (CHAR_PTR(p1) - CHAR_PTR(p2))

/* {{{ mem */
struct _xc_mem_block_t {
#ifdef ALLOC_DEBUG_BLOCK_CHECK
	unsigned int magic;
#endif
	xc_memsize_t size; /* reserved even after alloc */
	xc_block_t *next;  /* not used after alloc */
};

struct _xc_mem_mem_t {
	const xc_mem_handlers_t *handlers;
	xc_shm_t                *shm;
	xc_memsize_t size;
	xc_memsize_t avail;       /* total free */
	xc_block_t headblock[1];  /* just as a pointer to first block*/
};

#ifndef XtOffsetOf
#	include <linux/stddef.h>
#	define XtOffsetOf(s_type, field) offsetof(s_type, field)
#endif

#define SizeOf(type, field) sizeof( ((type *) 0)->field )
#define BLOCK_HEADER_SIZE() (ALIGN( XtOffsetOf(xc_block_t, size) + SizeOf(xc_block_t, size) ))

#define BLOCK_MAGIC ((unsigned int) 0x87655678)

/* }}} */
static inline void xc_block_setup(xc_block_t *b, xc_memsize_t size, xc_block_t *next) /* {{{ */
{
#ifdef ALLOC_DEBUG_BLOCK_CHECK
	b->magic = BLOCK_MAGIC;
#endif
	b->size = size;
	b->next = next;
}
/* }}} */
#ifdef ALLOC_DEBUG_BLOCK_CHECK
static void xc_block_check(xc_block_t *b) /* {{{ */
{
	if (b->magic != BLOCK_MAGIC) {
		fprintf(stderr, "0x%X != 0x%X magic wrong \n", b->magic, BLOCK_MAGIC);
	}
}
/* }}} */
#else
#	define xc_block_check(b) do { } while(0)
#endif


static XC_MEM_MALLOC(xc_mem_malloc) /* {{{ */
{
	xc_block_t *prev, *cur;
	xc_block_t *newb, *b;
	xc_memsize_t realsize;
	xc_memsize_t minsize;
	void *p;
	/* [xc_block_t:size|size] */
	realsize = BLOCK_HEADER_SIZE() + size;
	/* realsize is ALIGNed so next block start at ALIGNed address */
	realsize = ALIGN(realsize);

	TRACE("avail: %lu (%luKB). Allocate size: %lu realsize: %lu (%luKB)"
			, mem->avail, mem->avail / 1024
			, size
			, realsize, realsize / 1024
			);
	do {
		p = NULL;
		if (mem->avail < realsize) {
			TRACE("%s", " oom");
			break;
		}

		b = NULL;
		minsize = ULONG_MAX;

		/* prev|cur */

		for (prev = mem->headblock; prev->next; prev = cur) {
			/* while (prev->next != 0) { */
			cur = prev->next;
			xc_block_check(cur);
			if (cur->size == realsize) {
				/* found a perfect fit, stop searching */
				b = prev;
				break;
			}
			/* make sure we can split on the block */
			else if (cur->size > (sizeof(xc_block_t) + realsize) &&
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
		mem->avail -= realsize;

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

		newb = (xc_block_t *)PADD(cur, realsize);
		xc_block_setup(newb, cur->size - realsize, b);
		cur->size = realsize;
		/* prev|cur|newb|next
		 *            `--^
		 */

		TRACE(" -> avail: %lu (%luKB). new next: %p offset: %lu %luKB. Got: %p"
				, mem->avail, mem->avail / 1024
				, newb
				, PSUB(newb, mem), PSUB(newb, mem) / 1024
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
static XC_MEM_FREE(xc_mem_free) /* {{{ return block size freed */
{
	xc_block_t *cur, *b;
	int size;

	cur = (xc_block_t *) (CHAR_PTR(p) - BLOCK_HEADER_SIZE());
	TRACE("freeing: %p, size=%lu", p, cur->size);
	xc_block_check(cur);
	assert((char*)mem < (char*)cur && (char*)cur < (char*)mem + mem->size);

	/* find free block right before the p */
	b = mem->headblock;
	while (b->next != 0 && b->next < cur) {
		b = b->next;
	}

	/* restore block */
	cur->next = b->next;
	b->next = cur;
	size = cur->size;

	TRACE(" avail %lu (%luKB)", mem->avail, mem->avail / 1024);
	mem->avail += size;

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
	TRACE(" -> avail %lu (%luKB)", mem->avail, mem->avail / 1024);
	return size;
}
/* }}} */
static XC_MEM_CALLOC(xc_mem_calloc) /* {{{ */
{
	xc_memsize_t realsize = memb * size;
	void *p = xc_mem_malloc(mem, realsize);

	if (p) {
		memset(p, 0, realsize);
	}
	return p;
}
/* }}} */
static XC_MEM_REALLOC(xc_mem_realloc) /* {{{ */
{
	void *newp = xc_mem_malloc(mem, size);
	if (p && newp) {
		memcpy(newp, p, size);
		xc_mem_free(mem, p);
	}
	return newp;
}
/* }}} */
static XC_MEM_STRNDUP(xc_mem_strndup) /* {{{ */
{
	void *p = xc_mem_malloc(mem, len + 1);
	if (p) {
		memcpy(p, str, len + 1);
	}
	return p;
}
/* }}} */
static XC_MEM_STRDUP(xc_mem_strdup) /* {{{ */
{
	return xc_mem_strndup(mem, str, strlen(str));
}
/* }}} */

static XC_MEM_AVAIL(xc_mem_avail) /* {{{ */
{
	return mem->avail;
}
/* }}} */
static XC_MEM_SIZE(xc_mem_size) /* {{{ */
{
	return mem->size;
}
/* }}} */

static XC_MEM_FREEBLOCK_FIRST(xc_mem_freeblock_first) /* {{{ */
{
	return mem->headblock->next;
}
/* }}} */
XC_MEM_FREEBLOCK_NEXT(xc_mem_freeblock_next) /* {{{ */
{
	return block->next;
}
/* }}} */
XC_MEM_BLOCK_SIZE(xc_mem_block_size) /* {{{ */
{
	return block->size;
}
/* }}} */
XC_MEM_BLOCK_OFFSET(xc_mem_block_offset) /* {{{ */
{
	return ((char *) block) - ((char *) mem);
}
/* }}} */

static XC_MEM_INIT(xc_mem_init) /* {{{ */
{
	xc_block_t *b;
#define MINSIZE (ALIGN(sizeof(xc_mem_t)) + sizeof(xc_block_t))
	/* requires at least the header and 1 tail block */
	if (size < MINSIZE) {
		fprintf(stderr, "xc_mem_init requires %lu bytes at least\n", (unsigned long) MINSIZE);
		return NULL;
	}
	TRACE("size=%lu", size);
	mem->shm = shm;
	mem->size = size;
	mem->avail = size - MINSIZE;

	/* pointer to first block, right after ALIGNed header */
	b = mem->headblock;
	xc_block_setup(b, 0, (xc_block_t *) PADD(mem, ALIGN(sizeof(xc_mem_t))));

	/* first block*/
	b = b->next;
	xc_block_setup(b, mem->avail, 0);
#undef MINSIZE

	return mem;
}
/* }}} */
static XC_MEM_DESTROY(xc_mem_destroy) /* {{{ */
{
}
/* }}} */

#ifdef TEST
/* {{{ testing */
#undef CHECK
#define CHECK(a, msg) do { \
	if (!(a)) { \
		fprintf(stderr, "%s\n", msg); return -1; \
	} \
} while (0)

#include <time.h>

int main()
{
	int count = 0;
	void *p;
	void *memory;
	xc_mem_t *mem;
	void **ptrs;
	int size, i;

#if 0
	fprintf(stderr, "%s", "Input test size: ");
	scanf("%d", &size);
#else
	size = 1024;
#endif
	CHECK(memory = malloc(size), "OOM");
	CHECK(ptrs   = malloc(size * sizeof(void *)), "OOM");
	mem = (xc_mem_t *) memory;
	CHECK(mem    = xc_mem_init(NULL, mem, size), "Failed init memory allocator");

	while ((p = xc_mem_malloc(mem, 1))) {
		ptrs[count ++] = p;
	}
	fprintf(stderr, "count=%d, random freeing\n", count);
	srandom(time(NULL));
	while (count) {
		i = (random() % count);
		fprintf(stderr, "freeing %d: ", i);
		xc_mem_free(mem, ptrs[i]);
		ptrs[i] = ptrs[count - 1];
		count --;
	}

	free(ptrs);
	free(memory);
	return 0;
}
/* }}} */
#endif

typedef struct {
	const char              *name;
	const xc_mem_handlers_t *handlers;
} xc_mem_scheme_t;
static xc_mem_scheme_t xc_mem_schemes[10];

int xc_mem_scheme_register(const char *name, const xc_mem_handlers_t *handlers) /* {{{ */
{
	int i;
	for (i = 0; i < 10; i ++) {
		if (!xc_mem_schemes[i].name) {
			xc_mem_schemes[i].name = name;
			xc_mem_schemes[i].handlers = handlers;
			return 1;
		}
	}
	return 0;
}
/* }}} */
const xc_mem_handlers_t *xc_mem_scheme_find(const char *name) /* {{{ */
{
	int i;
	for (i = 0; i < 10 && xc_mem_schemes[i].name; i ++) {
		if (strcmp(xc_mem_schemes[i].name, name) == 0) {
			return xc_mem_schemes[i].handlers;
		}
	}
	return NULL;
}
/* }}} */

static xc_mem_handlers_t xc_mem_mem_handlers = XC_MEM_HANDLERS(mem);
void xc_shm_mem_init() /* {{{ */
{
	memset(xc_mem_schemes, 0, sizeof(xc_mem_schemes));

	if (xc_mem_scheme_register("mem", &xc_mem_mem_handlers) == 0) {
		zend_error(E_ERROR, "XCache: failed to register mem mem_scheme");
	}
}
/* }}} */
