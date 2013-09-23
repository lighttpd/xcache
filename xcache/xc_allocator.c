#ifdef TEST
#	ifdef HAVE_CONFIG_H
#		include <config.h>
#	endif
#else
#	include "xcache.h"
#endif
#include "xc_allocator.h"
#include <string.h>
#include <stdio.h>

typedef struct {
	const char *name;
	const xc_allocator_vtable_t *allocator_vtable;
} xc_allocator_info_t;
static xc_allocator_info_t xc_allocator_infos[10];

int xc_allocator_register(const char *name, const xc_allocator_vtable_t *allocator_vtable) /* {{{ */
{
	size_t i;
	for (i = 0; i < sizeof(xc_allocator_infos) / sizeof(xc_allocator_infos[0]); i ++) {
		if (!xc_allocator_infos[i].name) {
			xc_allocator_infos[i].name = name;
			xc_allocator_infos[i].allocator_vtable = allocator_vtable;
			return 1;
		}
	}
	return 0;
}
/* }}} */
const xc_allocator_vtable_t *xc_allocator_find(const char *name) /* {{{ */
{
	size_t i;
	for (i = 0; i < sizeof(xc_allocator_infos) / sizeof(xc_allocator_infos[0]) && xc_allocator_infos[i].name; i ++) {
		if (strcmp(xc_allocator_infos[i].name, name) == 0) {
			return xc_allocator_infos[i].allocator_vtable;
		}
	}
	return NULL;
}
/* }}} */
void xc_allocator_init() /* {{{ */
{
	extern void xc_allocator_bestfit_register();
#ifdef HAVE_XCACHE_TEST
	extern void xc_allocator_malloc_register();
#endif

	memset(xc_allocator_infos, 0, sizeof(xc_allocator_infos));
	xc_allocator_bestfit_register();
#ifdef HAVE_XCACHE_TEST
	xc_allocator_malloc_register();
#endif
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

int testAllocator(const xc_allocator_vtable_t *allocator_vtable)
{
	int count = 0;
	void *p;
	xc_allocator_t *allocator;
	void *memory;
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
	allocator = (xc_allocator_t *) memory;
	allocator->vtable = allocator_vtable;
	CHECK(allocator = allocator->vtable->init(NULL, allocator, size), "Failed init memory allocator");

	while ((p = allocator->vtable->malloc(allocator, 1))) {
		ptrs[count ++] = p;
	}
	fprintf(stderr, "count=%d, random freeing\n", count);
	srandom(time(NULL));
	while (count) {
		i = (random() % count);
		fprintf(stderr, "freeing %d: ", i);
		allocator->vtable->free(allocator, ptrs[i]);
		ptrs[i] = ptrs[count - 1];
		count --;
	}

	free(ptrs);
	free(memory);
	return 0;
}
/* }}} */
int main() /* {{{ */
{
	int i;

	xc_allocator_init();

	for (i = 0; i < sizeof(xc_allocator_infos) / sizeof(xc_allocator_infos[0]) && xc_allocator_infos[i].name; i ++) {
		fprintf(stderr, "testing %s...\n", xc_allocator_infos[i].name);
		testAllocator(xc_allocator_infos[i].allocator_vtable);
	}
	return 0;
}
/* }}} */
#endif
