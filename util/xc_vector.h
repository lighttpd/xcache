#ifndef XC_VECTOR_H_0957AC4E1A44E838C7B8DBECFF9C4B3B
#define XC_VECTOR_H_0957AC4E1A44E838C7B8DBECFF9C4B3B

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>

typedef struct {
	void *data_;
	size_t size_;
	size_t capacity_;

	size_t data_size_;
	int persistent_;
} xc_vector_t;

#define xc_vector_initializer(type, persistent) { \
	NULL, \
	0, \
	0, \
\
	sizeof(type), \
	persistent, \
}

#define xc_vector_init_ex(type, vector, data, size, persistent) do { \
	(vector)->data_ = data; \
	(vector)->size_ = (vector)->capacity_ = size; \
	(vector)->data_size_ = sizeof(type); \
	(vector)->persistent_ = persistent; \
} while (0)

#define xc_vector_init(type, vector) xc_vector_init_ex(type, vector, NULL, 0, 0)
#define xc_vector_init_persistent(type, vector) xc_vector_init_ex(type, vector, NULL, 0, 1)

static inline void xc_vector_destroy_impl(xc_vector_t *vector TSRMLS_DC)
{
	vector->size_ = 0;
	if (vector->data_) {
		pefree(vector->data_, vector->persistent_);
		vector->data_ = NULL;
	}
	vector->capacity_ = 0;
	vector->data_size_ = 0;
}

#define xc_vector_destroy(vector) xc_vector_destroy_impl(vector TSRMLS_CC)
#define xc_vector_clear(vector) do { (vector)->size_ = 0; } while (0)

#define xc_vector_size(vector) ((vector)->size_)
#define xc_vector_initialized(vector) ((vector)->data_size_ != 0)
#define xc_vector_element_ptr_(vector, index) ( \
	(void *) ( \
		((char *) (vector)->data_) + (index) * (vector)->data_size_ \
	) \
)

static inline xc_vector_t *xc_vector_check_type_(xc_vector_t *vector, size_t data_size)
{
	assert(vector->data_size_ = data_size);
	return vector;
}

#define xc_vector_data(type, vector) ((type *) xc_vector_check_type_(vector, sizeof(type))->data_)

static void xc_vector_reserve_impl(xc_vector_t *vector, size_t capacity TSRMLS_DC)
{
	assert(capacity);
	if (!vector->capacity_) {
		vector->capacity_ = 8;
	}
	while (vector->capacity_ <= capacity) {
		vector->capacity_ <<= 1;
	}
	vector->data_ = perealloc(vector->data_, vector->data_size_ * vector->capacity_, vector->persistent_);
}
#define xc_vector_reserve(vector, capacity) xc_vector_reserve_impl(vector, capacity TSRMLS_CC)

static void xc_vector_resize_impl(xc_vector_t *vector, size_t size TSRMLS_DC)
{
	assert(size);
	xc_vector_reserve(vector, size);
	vector->size_ = size;
}
#define xc_vector_resize(vector, size) xc_vector_resize_impl(vector, size TSRMLS_CC)

static inline void xc_vector_check_reserve_(xc_vector_t *vector TSRMLS_DC)
{
	if (vector->size_ == vector->capacity_) {
		if (vector->capacity_) {
			vector->capacity_ <<= 1;
		}
		else {
			vector->capacity_ = 8;
		}
		vector->data_ = perealloc(vector->data_, vector->data_size_ * vector->capacity_, vector->persistent_);
	}
}

#define xc_vector_push_back(vector, value_ptr) do { \
	xc_vector_check_reserve_(vector TSRMLS_CC); \
	memcpy(xc_vector_element_ptr_(vector, (vector)->size_++), value_ptr, (vector)->data_size_); \
} while (0)

static inline void *xc_vector_detach_impl(xc_vector_t *vector)
{
	void *data = vector->data_;

	vector->data_ = NULL;
	vector->capacity_ = 0;
	vector->size_ = 0;
	return data;
}

#define xc_vector_detach(type, vector) ((type *) xc_vector_detach_impl(xc_vector_check_type_(vector, sizeof(type))))

static inline xc_vector_t *xc_vector_pop_back_check_(xc_vector_t *vector, size_t data_size)
{
	assert(vector);
	assert(vector->data_size_ == data_size);
	assert(vector->capacity_ > 0);
	return vector;
}

#define xc_vector_pop_back(type, vector) xc_vector_data(type, \
	xc_vector_pop_back_check_(vector, sizeof(type)) \
)[--(vector)->size_]

static inline void xc_vector_reverse(xc_vector_t *vector)
{
	char *left, *right;
	void *tmp;

	assert(vector);
	assert(vector->data_size_);
	tmp = alloca(vector->data_size_);
	for (left = vector->data_, right = xc_vector_element_ptr_(vector, vector->size_ - 1); left < right; left += vector->data_size_, right -= vector->data_size_) {
		memcpy(tmp, left, vector->data_size_);
		memcpy(left, right, vector->data_size_);
		memcpy(right, tmp, vector->data_size_);
	}
}

#endif /* XC_VECTOR_H_0957AC4E1A44E838C7B8DBECFF9C4B3B */
