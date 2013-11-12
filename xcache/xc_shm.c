#ifdef TEST
#	ifdef HAVE_CONFIG_H
#		include <config.h>
#	endif
#	include <limits.h>
#	include <stdio.h>
#else
#	include "xcache.h"
#endif
#include "xc_shm.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct _xc_shm_scheme_t {
	const char            *name;
	const xc_shm_vtable_t *vtable;
};
static xc_shm_scheme_t xc_shm_schemes[10];

void xc_shm_init_modules() /* {{{ */
{
	extern void xc_allocator_init();
#ifdef HAVE_XCACHE_TEST
	extern void xc_shm_malloc_register();
#endif
	extern void xc_shm_mmap_register();

	memset(xc_shm_schemes, 0, sizeof(xc_shm_schemes));
	xc_allocator_init();
#ifdef HAVE_XCACHE_TEST
	xc_shm_malloc_register();
#endif
	xc_shm_mmap_register();
}
/* }}} */
int xc_shm_scheme_register(const char *name, const xc_shm_vtable_t *vtable) /* {{{ */
{
	int i;
	for (i = 0; i < 10; i ++) {
		if (!xc_shm_schemes[i].name) {
			xc_shm_schemes[i].name = name;
			xc_shm_schemes[i].vtable = vtable;
			return 1;
		}
	}
	return 0;
}
/* }}} */
const xc_shm_vtable_t *xc_shm_scheme_find(const char *name) /* {{{ */
{
	int i;
	for (i = 0; i < 10 && xc_shm_schemes[i].name; i ++) {
		if (strcmp(xc_shm_schemes[i].name, name) == 0) {
			return xc_shm_schemes[i].vtable;
		}
	}
	return NULL;
}
/* }}} */
xc_shm_scheme_t *xc_shm_scheme_first() /* {{{ */
{
	return xc_shm_schemes;
}
/* }}} */
xc_shm_scheme_t *xc_shm_scheme_next(xc_shm_scheme_t *scheme) /* {{{ */
{
	scheme ++;
	return scheme->name ? scheme : NULL;
}
/* }}} */
const char *xc_shm_scheme_name(xc_shm_scheme_t *scheme) /* {{{ */
{
	assert(scheme);
	return scheme->name;
}
/* }}} */
xc_shm_t *xc_shm_init(const char *type, xc_shmsize_t size, int readonly_protection, const void *arg1, const void *arg2) /* {{{ */
{
	const xc_shm_vtable_t *vtable = xc_shm_scheme_find(type);

	if (vtable) {
		xc_shm_t *shm = vtable->init(size, readonly_protection, arg1, arg2);
		if (shm) {
			shm->vtable = vtable;
		}
		return shm;
	}

	return NULL;
}
/* }}} */
void xc_shm_destroy(xc_shm_t *shm) /* {{{ */
{
	shm->vtable->destroy(shm);
}
/* }}} */

int xc_shm_can_readonly(const xc_shm_t *shm) /* {{{ */
{
	return ((xc_shm_base_t *)shm)->readonlydiff != 0;
}
/* }}} */
void *xc_shm_to_readwrite(const xc_shm_t *shm_, void *p) /* {{{ */
{
	const xc_shm_base_t *shm = (const xc_shm_base_t *) shm_;

	if (shm->readonlydiff) {
		assert(shm->vtable->is_readonly(shm_, p));
		p = ((char *) p) - shm->readonlydiff;
	}
	assert(shm->vtable->is_readwrite(shm_, p));
	return p;
}
/* }}} */
void *xc_shm_to_readonly(const xc_shm_t *shm_, void *p) /* {{{ */
{
	const xc_shm_base_t *shm = (const xc_shm_base_t *) shm_;

	assert(shm->vtable->is_readwrite(shm_, p));
	if (shm->readonlydiff) {
		p = ((char *) p) + shm->readonlydiff;
		assert(shm->vtable->is_readonly(shm_, p));
	}
	return p;
}
/* }}} */
