#ifndef XC_VECTOR_H_0957AC4E1A44E838C7B8DBECFF9C4B3B
#define XC_VECTOR_H_0957AC4E1A44E838C7B8DBECFF9C4B3B

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

typedef struct {
	zend_uint size;
	zend_uint cnt;
	void *data;
} xc_vector_t;

#define xc_vector_init(type, vector) do { \
	(vector)->cnt = 0;     \
	(vector)->size = 0;    \
	(vector)->data = NULL; \
} while (0)

#define xc_vector_add(type, vector, value) do { \
	if ((vector)->cnt == (vector)->size) { \
		if ((vector)->size) { \
			(vector)->size <<= 1; \
			(vector)->data = erealloc((vector)->data, sizeof(type) * (vector)->size); \
		} \
		else { \
			(vector)->size = 8; \
			(vector)->data = emalloc(sizeof(type) * (vector)->size); \
		} \
	} \
	((type *) (vector)->data)[(vector)->cnt++] = value; \
} while (0)

static inline void *xc_vector_detach_impl(xc_vector_t *vector)
{
	void *data = vector->data;
	vector->data = NULL;
	vector->size = 0;
	vector->cnt = 0;
	return data;
}

#define xc_vector_detach(type, vector) ((type *) xc_vector_detach_impl(vector))

static inline void xc_vector_free_impl(xc_vector_t *vector TSRMLS_DC)
{
	if (vector->data) {
		efree(vector->data);
	}
	vector->size = 0;
	vector->cnt = 0;
}

#define xc_vector_free(type, vector) xc_vector_free_impl(vector TSRMLS_CC)

#endif /* XC_VECTOR_H_0957AC4E1A44E838C7B8DBECFF9C4B3B */
