typedef struct _xc_shm_t xc_shm_t;
typedef size_t xc_shmsize_t;

#include "mem.h"

/* shm */
#define XC_SHM_CAN_READONLY(func) int   func(xc_shm_t *shm)
#define XC_SHM_IS_READWRITE(func) int   func(xc_shm_t *shm, const void *p)
#define XC_SHM_IS_READONLY(func)  int   func(xc_shm_t *shm, const void *p)
#define XC_SHM_TO_READWRITE(func) void *func(xc_shm_t *shm, void *p)
#define XC_SHM_TO_READONLY(func)  void *func(xc_shm_t *shm, void *p)

#define XC_SHM_INIT(func)         xc_shm_t *func(xc_shmsize_t size, int readonly_protection, const void *arg1, const void *arg2)
#define XC_SHM_DESTROY(func)      void func(xc_shm_t *shm)

#define XC_SHM_MEMINIT(func)      xc_mem_t *func(xc_shm_t *shm, xc_memsize_t size)
#define XC_SHM_MEMDESTROY(func)   void func(xc_mem_t *mem)

#define XC_SHM_HANDLERS(name)    { \
	NULL                           \
	, xc_##name##_can_readonly     \
	, xc_##name##_is_readwrite     \
	, xc_##name##_is_readonly      \
	, xc_##name##_to_readwrite     \
	, xc_##name##_to_readonly      \
\
	, xc_##name##_init             \
	, xc_##name##_destroy          \
\
	, xc_##name##_meminit          \
	, xc_##name##_memdestroy       \
}

typedef struct {
	const xc_mem_handlers_t *memhandlers;
	XC_SHM_CAN_READONLY((*can_readonly));
	XC_SHM_IS_READWRITE((*is_readwrite));
	XC_SHM_IS_READONLY((*is_readonly));
	XC_SHM_TO_READWRITE((*to_readwrite));
	XC_SHM_TO_READONLY((*to_readonly));
	XC_SHM_INIT((*init));
	XC_SHM_DESTROY((*destroy));

	XC_SHM_MEMINIT((*meminit));
	XC_SHM_MEMDESTROY((*memdestroy));
} xc_shm_handlers_t;


#ifndef XC_SHM_IMPL
struct _xc_shm_t {
	const xc_shm_handlers_t *handlers;
};
#endif

void xc_shm_init_modules();
int xc_shm_scheme_register(const char *name, const xc_shm_handlers_t *handlers);
const xc_shm_handlers_t *xc_shm_scheme_find(const char *name);

xc_shm_t *xc_shm_init(const char *type, xc_shmsize_t size, int readonly_protection, const void *arg1, const void *arg2);
void xc_shm_destroy(xc_shm_t *shm);
