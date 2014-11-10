#ifndef XC_VECTOR_H_0957AC4E1A44E838C7B8DBECFF9C4B3B
#define XC_VECTOR_H_0957AC4E1A44E838C7B8DBECFF9C4B3B

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>

typedef struct {
	size_t size;
	void *data;

	size_t capacity;
	size_t data_size;
	int persistent;
} xc_vector_t;

#define xc_vector_initializer(type, persistent_) { \
	0, \
	NULL, \
\
	0, \
	sizeof(type), \
	persistent_, \
}

#define xc_vector_init(type, vector, persistent_) do { \
	(vector)->size = 0; \
	(vector)->data = NULL; \
\
	(vector)->capacity = 0; \
	(vector)->data_size = sizeof(type); \
	(vector)->persistent = persistent_; \
} while (0)

static inline void xc_vector_destroy_impl(xc_vector_t *vector TSRMLS_DC)
{
	vector->size = 0;
	if (vector->data) {
		pefree(vector->data, vector->persistent);
		vector->data = NULL;
	}
	vector->capacity = 0;
	vector->data_size = 0;
}

#define xc_vector_destroy(vector) xc_vector_destroy_impl(vector TSRMLS_CC)

#define xc_vector_size(vector) ((vector)->size)
#define xc_vector_initialized(vector) ((vector)->data_size != 0)
#define xc_vector_element_ptr_(vector, index) ( \
	(void *) ( \
		((char *) (vector)->data) + (index) * (vector)->data_size \
	) \
)

static inline xc_vector_t *xc_vector_check_type_(xc_vector_t *vector, size_t data_size)
{
	assert(vector->data_size = data_size);
	return vector;
}

#define xc_vector_data(type, vector) ((type *) xc_vector_check_type_(vector, sizeof(type))->data)

static inline void xc_vector_check_reserve_(xc_vector_t *vector TSRMLS_DC)
{
	if (vector->size == vector->capacity) {
		if (vector->capacity) {
			vector->capacity <<= 1;
			vector->data = perealloc(vector->data, vector->data_size * vector->capacity, vector->persistent);
		}
		else {
			vector->capacity = 8;
			vector->data = pemalloc(vector->data_size * vector->capacity, vector->persistent);
		}
	}
}

#define xc_vector_push_back(vector, value_ptr) do { \
	xc_vector_check_reserve_(vector TSRMLS_CC); \
	memcpy(xc_vector_element_ptr_(vector, (vector)->size++), value_ptr, (vector)->data_size); \
} while (0)

static inline void *xc_vector_detach_impl(xc_vector_t *vector)
{
	void *data = vector->data;

	vector->data = NULL;
	vector->capacity = 0;
	vector->size = 0;
	return data;
}

#define xc_vector_detach(type, vector) ((type *) xc_vector_detach_impl(xc_vector_check_type_(vector, sizeof(type))))

static inline xc_vector_t *xc_vector_pop_back_check_(xc_vector_t *vector, size_t data_size)
{
	assert(vector);
	assert(vector->data_size == data_size);
	assert(vector->capacity > 0);
	return vector;
}

#define xc_vector_pop_back(type, vector) xc_vector_data(type, \
	xc_vector_pop_back_check_(vector, sizeof(type)) \
)[--(vector)->size]

static inline void xc_vector_reverse(xc_vector_t *vector)
{
	char *left, *right;
	void *tmp;

	assert(vector);
	tmp = alloca(vector->data_size);
	for (left = vector->data, right = xc_vector_element_ptr_(vector, vector->size - 1); left < right; left += vector->data_size, right -= vector->data_size) {
		memcpy(tmp, left, vector->data_size);
		memcpy(left, right, vector->data_size);
		memcpy(right, tmp, vector->data_size);
	}
}

#endif /* XC_VECTOR_H_0957AC4E1A44E838C7B8DBECFF9C4B3B */
