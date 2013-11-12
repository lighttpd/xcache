#ifndef XC_SHM_H
#define XC_SHM_H

#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

#ifndef XC_SHM_T_DEFINED
typedef struct _xc_shm_base_t xc_shm_t;
#define XC_SHM_T_DEFINED
#endif

typedef struct _xc_shm_vtable_t xc_shm_vtable_t;

typedef struct _xc_shm_base_t {
	const xc_shm_vtable_t *vtable;
	ptrdiff_t readonlydiff;
} xc_shm_base_t;

typedef size_t xc_shmsize_t;

/* shm */
#define XC_SHM_IS_READWRITE(func) int   func(const xc_shm_t *shm, const void *p)
#define XC_SHM_IS_READONLY(func)  int   func(const xc_shm_t *shm, const void *p)

#define XC_SHM_INIT(func)         xc_shm_t *func(xc_shmsize_t size, int readonly_protection, const void *arg1, const void *arg2)
#define XC_SHM_DESTROY(func)      void func(xc_shm_t *shm)

#define XC_SHM_MEMINIT(func)      void *func(xc_shm_t *shm, xc_shmsize_t size)
#define XC_SHM_MEMDESTROY(func)   void func(void *mem)

#define XC_SHM_VTABLE(name)      { \
	  xc_##name##_init             \
	, xc_##name##_destroy          \
	, xc_##name##_is_readwrite     \
	, xc_##name##_is_readonly      \
\
\
	, xc_##name##_meminit          \
	, xc_##name##_memdestroy       \
}

struct _xc_shm_vtable_t {
	XC_SHM_INIT((*init));
	XC_SHM_DESTROY((*destroy));
	XC_SHM_IS_READWRITE((*is_readwrite));
	XC_SHM_IS_READONLY((*is_readonly));

	XC_SHM_MEMINIT((*meminit));
	XC_SHM_MEMDESTROY((*memdestroy));
};

typedef struct _xc_shm_scheme_t xc_shm_scheme_t;

void xc_shm_init_modules();
int xc_shm_scheme_register(const char *name, const xc_shm_vtable_t *vtable);
const xc_shm_vtable_t *xc_shm_scheme_find(const char *name);
xc_shm_scheme_t *xc_shm_scheme_first();
xc_shm_scheme_t *xc_shm_scheme_next(xc_shm_scheme_t *scheme);
const char *xc_shm_scheme_name(xc_shm_scheme_t *scheme);

xc_shm_t *xc_shm_init(const char *type, xc_shmsize_t size, int readonly_protection, const void *arg1, const void *arg2);
void xc_shm_destroy(xc_shm_t *shm);

int xc_shm_can_readonly(const xc_shm_t *shm);
void *xc_shm_to_readwrite(const xc_shm_t *shm_, void *p);
void *xc_shm_to_readonly(const xc_shm_t *shm_, void *p);

#endif
