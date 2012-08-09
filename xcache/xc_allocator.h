#ifndef XC_ALLOCATOR_H_155E773CA8AFC18F3CCCDCF0831EE41D
#define XC_ALLOCATOR_H_155E773CA8AFC18F3CCCDCF0831EE41D

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

#include "xc_shm.h"

typedef struct _xc_allocator_vtable_t xc_allocator_vtable_t;

#ifndef _xc_allocator_t
struct _xc_allocator_t {
	const xc_allocator_vtable_t *vtable;
	xc_shm_t *shm;
};
#endif

typedef struct _xc_allocator_t xc_allocator_t;
typedef struct _xc_allocator_block_t xc_allocator_block_t;
typedef xc_shmsize_t xc_memsize_t;

/* allocator */
#define XC_ALLOCATOR_MALLOC(func)          void *func(xc_allocator_t *allocator, xc_memsize_t size)
#define XC_ALLOCATOR_FREE(func)            xc_memsize_t  func(xc_allocator_t *allocator, const void *p)
#define XC_ALLOCATOR_CALLOC(func)          void *func(xc_allocator_t *allocator, xc_memsize_t memb, xc_memsize_t size)
#define XC_ALLOCATOR_REALLOC(func)         void *func(xc_allocator_t *allocator, const void *p, xc_memsize_t size)
#define XC_ALLOCATOR_AVAIL(func)           xc_memsize_t      func(const xc_allocator_t *allocator)
#define XC_ALLOCATOR_SIZE(func)            xc_memsize_t      func(const xc_allocator_t *allocator)
#define XC_ALLOCATOR_FREEBLOCK_FIRST(func) const xc_allocator_block_t *func(const xc_allocator_t *allocator)
#define XC_ALLOCATOR_FREEBLOCK_NEXT(func)  const xc_allocator_block_t *func(const xc_allocator_block_t *block)
#define XC_ALLOCATOR_BLOCK_SIZE(func)      xc_memsize_t      func(const xc_allocator_block_t *block)
#define XC_ALLOCATOR_BLOCK_OFFSET(func)    xc_memsize_t      func(const xc_allocator_t *allocator, const xc_allocator_block_t *block)

#define XC_ALLOCATOR_INIT(func)            xc_allocator_t *func(xc_shm_t *shm, xc_allocator_t *allocator, xc_memsize_t size)
#define XC_ALLOCATOR_DESTROY(func)         void func(xc_allocator_t *allocator)

#define XC_ALLOCATOR_VTABLE(name)   {  \
	xc_##name##_malloc             \
	, xc_##name##_free             \
	, xc_##name##_calloc           \
	, xc_##name##_realloc          \
	, xc_##name##_avail            \
	, xc_##name##_size             \
	, xc_##name##_freeblock_first  \
	, xc_##name##_freeblock_next   \
	, xc_##name##_block_size       \
	, xc_##name##_block_offset     \
\
	, xc_##name##_init             \
	, xc_##name##_destroy          \
}

struct _xc_allocator_vtable_t {
	XC_ALLOCATOR_MALLOC((*malloc));
	XC_ALLOCATOR_FREE((*free));
	XC_ALLOCATOR_CALLOC((*calloc));
	XC_ALLOCATOR_REALLOC((*realloc));
	XC_ALLOCATOR_AVAIL((*avail));
	XC_ALLOCATOR_SIZE((*size));
	XC_ALLOCATOR_FREEBLOCK_FIRST((*freeblock_first));
	XC_ALLOCATOR_FREEBLOCK_NEXT((*freeblock_next));
	XC_ALLOCATOR_BLOCK_SIZE((*block_size));
	XC_ALLOCATOR_BLOCK_OFFSET((*block_offset));

	XC_ALLOCATOR_INIT((*init));
	XC_ALLOCATOR_DESTROY((*destroy));
};

int xc_allocator_register(const char *name, const xc_allocator_vtable_t *allocator_vtable);
const xc_allocator_vtable_t *xc_allocator_find(const char *name);

#endif /* XC_ALLOCATOR_H_155E773CA8AFC18F3CCCDCF0831EE41D */
