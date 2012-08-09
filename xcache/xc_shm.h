#ifndef XC_SHM_H
#define XC_SHM_H

#include <stdlib.h>

typedef struct _xc_shm_handlers_t xc_shm_handlers_t;

#ifndef XC_SHM_IMPL
struct _xc_shm_t {
	const xc_shm_handlers_t *handlers;
};
#define XC_SHM_IMPL _xc_shm_t
#endif

typedef struct XC_SHM_IMPL xc_shm_t;
typedef size_t xc_shmsize_t;

/* shm */
#define XC_SHM_CAN_READONLY(func) int   func(xc_shm_t *shm)
#define XC_SHM_IS_READWRITE(func) int   func(xc_shm_t *shm, const void *p)
#define XC_SHM_IS_READONLY(func)  int   func(xc_shm_t *shm, const void *p)
#define XC_SHM_TO_READWRITE(func) void *func(xc_shm_t *shm, void *p)
#define XC_SHM_TO_READONLY(func)  void *func(xc_shm_t *shm, void *p)

#define XC_SHM_INIT(func)         xc_shm_t *func(xc_shmsize_t size, int readonly_protection, const void *arg1, const void *arg2)
#define XC_SHM_DESTROY(func)      void func(xc_shm_t *shm)

#define XC_SHM_MEMINIT(func)      void *func(xc_shm_t *shm, xc_shmsize_t size)
#define XC_SHM_MEMDESTROY(func)   void func(void *mem)

#define XC_SHM_HANDLERS(name)    { \
	xc_##name##_can_readonly       \
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

struct _xc_shm_handlers_t {
	XC_SHM_CAN_READONLY((*can_readonly));
	XC_SHM_IS_READWRITE((*is_readwrite));
	XC_SHM_IS_READONLY((*is_readonly));
	XC_SHM_TO_READWRITE((*to_readwrite));
	XC_SHM_TO_READONLY((*to_readonly));
	XC_SHM_INIT((*init));
	XC_SHM_DESTROY((*destroy));

	XC_SHM_MEMINIT((*meminit));
	XC_SHM_MEMDESTROY((*memdestroy));
};

typedef struct _xc_shm_scheme_t xc_shm_scheme_t;

void xc_shm_init_modules();
int xc_shm_scheme_register(const char *name, const xc_shm_handlers_t *handlers);
const xc_shm_handlers_t *xc_shm_scheme_find(const char *name);
xc_shm_scheme_t *xc_shm_scheme_first();
xc_shm_scheme_t *xc_shm_scheme_next(xc_shm_scheme_t *scheme);
const char *xc_shm_scheme_name(xc_shm_scheme_t *scheme);

xc_shm_t *xc_shm_init(const char *type, xc_shmsize_t size, int readonly_protection, const void *arg1, const void *arg2);
void xc_shm_destroy(xc_shm_t *shm);
#endif
