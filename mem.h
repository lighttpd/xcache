#include "xc_shm.h"

typedef struct _xc_mem_handlers_t xc_mem_handlers_t;

#ifndef XC_MEM_IMPL
struct _xc_mem_t {
	const xc_mem_handlers_t *handlers;
	xc_shm_t                *shm;
};
#   define XC_MEM_IMPL _xc_mem_t
#endif

#ifndef XC_MEMBLOCK_IMPL
#   define XC_MEMBLOCK_IMPL _xc_block_t
#endif
typedef struct XC_MEM_IMPL xc_mem_t;
typedef struct XC_MEMBLOCK_IMPL xc_block_t;
typedef xc_shmsize_t xc_memsize_t;

/* shm::mem */
#define XC_MEM_MALLOC(func)          void *func(xc_mem_t *mem, xc_memsize_t size)
#define XC_MEM_FREE(func)            xc_memsize_t  func(xc_mem_t *mem, const void *p)
#define XC_MEM_CALLOC(func)          void *func(xc_mem_t *mem, xc_memsize_t memb, xc_memsize_t size)
#define XC_MEM_REALLOC(func)         void *func(xc_mem_t *mem, const void *p, xc_memsize_t size)
#define XC_MEM_STRNDUP(func)         char *func(xc_mem_t *mem, const char *str, xc_memsize_t len)
#define XC_MEM_STRDUP(func)          char *func(xc_mem_t *mem, const char *str)
#define XC_MEM_AVAIL(func)           xc_memsize_t      func(xc_mem_t *mem)
#define XC_MEM_SIZE(func)            xc_memsize_t      func(xc_mem_t *mem)
#define XC_MEM_FREEBLOCK_FIRST(func) const xc_block_t *func(xc_mem_t *mem)
#define XC_MEM_FREEBLOCK_NEXT(func)  const xc_block_t *func(const xc_block_t *block)
#define XC_MEM_BLOCK_SIZE(func)      xc_memsize_t      func(const xc_block_t *block)
#define XC_MEM_BLOCK_OFFSET(func)    xc_memsize_t      func(const xc_mem_t *mem, const xc_block_t *block)

#define XC_MEM_INIT(func)            xc_mem_t *func(xc_shm_t *shm, xc_mem_t *mem, xc_memsize_t size)
#define XC_MEM_DESTROY(func)         void func(xc_mem_t *mem)

#define XC_MEM_HANDLERS(name)   {  \
	xc_##name##_malloc             \
	, xc_##name##_free             \
	, xc_##name##_calloc           \
	, xc_##name##_realloc          \
	, xc_##name##_strndup          \
	, xc_##name##_strdup           \
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

struct _xc_mem_handlers_t {
	XC_MEM_MALLOC((*malloc));
	XC_MEM_FREE((*free));
	XC_MEM_CALLOC((*calloc));
	XC_MEM_REALLOC((*realloc));
	XC_MEM_STRNDUP((*strndup));
	XC_MEM_STRDUP((*strdup));
	XC_MEM_AVAIL((*avail));
	XC_MEM_SIZE((*size));
	XC_MEM_FREEBLOCK_FIRST((*freeblock_first));
	XC_MEM_FREEBLOCK_NEXT((*freeblock_next));
	XC_MEM_BLOCK_SIZE((*block_size));
	XC_MEM_BLOCK_OFFSET((*block_offset));

	XC_MEM_INIT((*init));
	XC_MEM_DESTROY((*destroy));
};

int xc_mem_scheme_register(const char *name, const xc_mem_handlers_t *handlers);
const xc_mem_handlers_t *xc_mem_scheme_find(const char *name);
