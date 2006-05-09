#include <php.h>
#include <stdlib.h>
#include "mem.h"
#include "align.h"
#define PADD(p, a) (((char*)p) + a)

// {{{ mem
struct _xc_block_t {
#ifdef ALLOC_DEBUG
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

#ifndef offsetof
#	define offsetof(type, field) ((int) ((char *) &((type *) 0)->field))
#endif
#define SizeOf(type, field) sizeof( ((type *) 0)->field )
#define BLOCK_HEADER_SIZE() (ALIGN( offsetof(xc_block_t, size) + SizeOf(xc_block_t, size) ))

#define BLOCK_MAGIC ((unsigned int) 0x87655678)

/* }}} */
static inline void xc_block_setup(xc_block_t *b, xc_memsize_t size, xc_block_t *next) /* {{{ */
{
#ifdef ALLOC_DEBUG
	b->magic = BLOCK_MAGIC;
#endif
	b->size = size;
	b->next = next;
}
/* }}} */
#ifdef ALLOC_DEBUG
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

	do {
		p = NULL;
		if (mem->avail < realsize) {
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
			break;
		}

		prev = b;

		cur = prev->next;
		p = PADD(cur, BLOCK_HEADER_SIZE());

		/* update the block header */
		mem->avail -= realsize;

		/*perfect fit, just unlink */
		if (cur->size == realsize) {
			prev->next = cur->next;
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
		fprintf(stderr, "avail: %d (%dKB). size: %d %d (%dKB) new next: %p offset: %d %dKB\n"
				, mem->avail, mem->avail/1024
				, size
				, realsize, realsize/1024
				, newb
				, ((char*)newb - mem->rw), ((char*)newb - mem->rw) / 1024
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

	cur = (xc_block_t *) (((char *) p) - BLOCK_HEADER_SIZE());
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

	mem->avail += size;

	/* combine prev|cur */
	if (PADD(b, b->size) == (char *)cur) {
		b->size += cur->size;
		b->next = cur->next;
		cur = b;
	}

	/* combine cur|next */
	b = cur->next;
	if (PADD(cur, cur->size) == (char *)b) {
		cur->size += b->size;
		cur->next = b->next;
	}
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
	xc_mem_t *mem = (xc_mem_t *) ptr;
	xc_block_t  *b;

	mem->size = size;
	mem->avail = size - ALIGN(sizeof(xc_mem_t));

	/* pointer to first block */
	b = mem->headblock;
	xc_block_setup(b, 0, (xc_block_t *)(mem + ALIGN(sizeof(xc_mem_t))));

	/* first block*/
	b = b->next;
	xc_block_setup(b, mem->avail, 0);

	return mem;
}
/* }}} */
void xc_mem_destroy(xc_mem_t *mem) /* {{{ */
{
}
/* }}} */
