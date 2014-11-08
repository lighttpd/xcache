
#define TSRMLS_DC
#define TSRMLS_CC
#define pemalloc(size, p) malloc(size)
#define perealloc(ptr, size, p) realloc(ptr, size)
#define pefree(ptr, p) free(ptr)

#include <stdio.h>
#include "xc_vector.h"

#undef CHECK
#define CHECK(a, msg) do { \
	if (!(a)) { \
		fprintf(stderr, "%s\n", msg); return -1; \
	} \
} while (0)

int main() /* {{{ */
{
	xc_vector_t vector = xc_vector_initializer(int, 0);
	int t;

	t = 1; xc_vector_push_back(&vector, &t);
	t = 2; xc_vector_push_back(&vector, &t);
	t = 3; xc_vector_push_back(&vector, &t);
	xc_vector_reverse(&vector);
	t = xc_vector_pop_back(int, &vector);
	CHECK(t == 1, "not 1");
	t = xc_vector_pop_back(int, &vector);
	CHECK(t == 2, "not 2");
	t = xc_vector_pop_back(int, &vector);
	CHECK(t == 3, "not 3");

	xc_vector_destroy(&vector);

	return 0;
}
/* }}} */
