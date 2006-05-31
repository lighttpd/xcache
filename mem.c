#ifdef TEST
#include <limits.h>
#include <stdio.h>
#else
#include <php.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"
#include "align.h"

#ifdef TEST
#	define ALLOC_DEBUG
#endif
#ifdef ALLOC_DEBUG
#	define ALLOC_DEBUG_BLOCK_CHECK
#endif

#if 0
#undef ALLOC_DEBUG_BLOCK_CHECK
#endif

#define CHAR_PTR(p) ((char *) (p))
#define PADD(p, a) (CHAR_PTR(p) + a)
#define PSUB(p1, p2) (CHAR_PTR(p1) - CHAR_PTR(p2))

// {{{ mem
struct _xc_block_t {
#ifdef ALLOC_DEBUG_BLOCK_CHECK
	unsigned int magic;
#endif
	xc_memsize_t size; /* reserved even after alloc */
	xc_block_t *next;  /* not used after alloc */
};

struct _xc_mem_t {
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


void *xc_mem_malloc(xc_mem_t *mem, xc_memsize_t size) /* {{{ */
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

#ifdef ALLOC_DEBUG
	fprintf(stderr, "avail: %d (%dKB). Allocate size: %d realsize: %d (%dKB)"
			, mem->avail, mem->avail / 1024
			, size
			, realsize, realsize / 1024
			);
#endif
	do {
		p = NULL;
		if (mem->avail < realsize) {
#ifdef ALLOC_DEBUG
			fprintf(stderr, " oom\n");
#endif
			break;
		}

		b = NULL;
		minsize = INT_MAX;

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
#ifdef ALLOC_DEBUG
			fprintf(stderr, " no fit chunk\n");
#endif
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
#ifdef ALLOC_DEBUG
			fprintf(stderr, " perfect fit. Got: %p\n", p);
#endif
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

#ifdef ALLOC_DEBUG
		fprintf(stderr, " -> avail: %d (%dKB). new next: %p offset: %d %dKB. Got: %p\n"
				, mem->avail, mem->avail / 1024
				, newb
				, PSUB(newb, mem), PSUB(newb, mem) / 1024
				, p
				);
#endif
		prev->next = newb;
		/* prev|cur|newb|next
		 *    `-----^
		 */

	} while (0);

	return p;
}
/* }}} */
int xc_mem_free(xc_mem_t *mem, const void *p) /* {{{ return block size freed */
{
	xc_block_t *cur, *b;
	int size;

	cur = (xc_block_t *) (CHAR_PTR(p) - BLOCK_HEADER_SIZE());
#ifdef ALLOC_DEBUG
	fprintf(stderr, "freeing: %p", p);
	fprintf(stderr, ", size=%d", cur->size);
#endif
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

#ifdef ALLOC_DEBUG
	fprintf(stderr, ", avail %d (%dKB)", mem->avail, mem->avail / 1024);
#endif
	mem->avail += size;

	/* combine prev|cur */
	if (PADD(b, b->size) == (char *)cur) {
		b->size += cur->size;
		b->next = cur->next;
		cur = b;
#ifdef ALLOC_DEBUG
		fprintf(stderr, ", combine prev");
#endif
	}

	/* combine cur|next */
	b = cur->next;
	if (PADD(cur, cur->size) == (char *)b) {
		cur->size += b->size;
		cur->next = b->next;
#ifdef ALLOC_DEBUG
		fprintf(stderr, ", combine next");
#endif
	}
#ifdef ALLOC_DEBUG
	fprintf(stderr, " -> avail %d (%dKB)\n", mem->avail, mem->avail / 1024);
#endif
	return size;
}
/* }}} */
void *xc_mem_calloc(xc_mem_t *mem, xc_memsize_t memb, xc_memsize_t size) /* {{{ */
{
	xc_memsize_t realsize = memb * size;
	void *p = xc_mem_malloc(mem, realsize);

	memset(p, 0, realsize);
	return p;
}
/* }}} */
void *xc_mem_realloc(xc_mem_t *mem, const void *p, xc_memsize_t size) /* {{{ */
{
	void *newp = xc_mem_malloc(mem, size);
	memcpy(newp, p, size);
	xc_mem_free(mem, p);
	return newp;
}
/* }}} */
char *xc_mem_strndup(xc_mem_t *mem, const char *str, xc_memsize_t len) /* {{{ */
{
	void *p = xc_mem_malloc(mem, len + 1);
	memcpy(p, str, len + 1);
	return p;
}
/* }}} */
char *xc_mem_strdup(xc_mem_t *mem, const char *str) /* {{{ */
{
	return xc_mem_strndup(mem, str, strlen(str));
}
/* }}} */

xc_memsize_t xc_mem_avail(xc_mem_t *mem) /* {{{ */
{
	return mem->avail;
}
/* }}} */
xc_memsize_t xc_mem_size(xc_mem_t *mem) /* {{{ */
{
	return mem->size;
}
/* }}} */

const xc_block_t *xc_mem_freeblock_first(xc_mem_t *mem) /* {{{ */
{
	return mem->headblock->next;
}
/* }}} */
const xc_block_t *xc_mem_freeblock_next(const xc_block_t *block) /* {{{ */
{
	return block->next;
}
/* }}} */
xc_memsize_t xc_mem_block_size(const xc_block_t *block) /* {{{ */
{
	return block->size;
}
/* }}} */
xc_memsize_t xc_mem_block_offset(const xc_mem_t *mem, const xc_block_t *block) /* {{{ */
{
	return ((char *) block) - ((char *) mem);
}
/* }}} */

xc_mem_t *xc_mem_init(void *ptr, xc_memsize_t size) /* {{{ */
{
	xc_mem_t   *mem;
	xc_block_t *b;

#define MINSIZE (ALIGN(sizeof(xc_mem_t)) + sizeof(xc_block_t))
	/* requires at least the header and 1 tail block */
	if (size < MINSIZE) {
		fprintf(stderr, "xc_mem_init requires %d bytes at least\n", MINSIZE);
		return NULL;
	}
	mem = (xc_mem_t *) ptr;
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
void xc_mem_destroy(xc_mem_t *mem) /* {{{ */
{
}
/* }}} */

#ifdef TEST
/* {{{ */
#undef CHECK
#define CHECK(a, msg) do { if ((a) == NULL) { puts(msg); return -1; } } while (0)
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
	size = 100;
#endif
	CHECK(memory = malloc(size), "OOM");
	CHECK(ptrs   = malloc(size * sizeof(void*)), "OOM");
	CHECK(mem    = xc_mem_init(memory, size), "Failed init memory allocator");

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
