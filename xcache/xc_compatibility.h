#ifndef XC_COMPATIBILITY_H_54F26ED90198353558718191D5EE244C
#define XC_COMPATIBILITY_H_54F26ED90198353558718191D5EE244C

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

#include "php.h"

/* Purpose: Privode stuffs for compatibility with different PHP version
 */

#if !defined(ZEND_ENGINE_2_6) && (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 6 || PHP_MAJOR_VERSION > 6)
#	define ZEND_ENGINE_2_6
#endif
#if !defined(ZEND_ENGINE_2_5) && (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION == 5 || defined(ZEND_ENGINE_2_6))
#	define ZEND_ENGINE_2_5
#endif
#if !defined(ZEND_ENGINE_2_4) && (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION == 4 || defined(ZEND_ENGINE_2_5))
#	define ZEND_ENGINE_2_4
#endif
#if !defined(ZEND_ENGINE_2_3) && (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION == 3 || defined(ZEND_ENGINE_2_4))
#	define ZEND_ENGINE_2_3
#endif
#if !defined(ZEND_ENGINE_2_2) && (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION == 2 || defined(ZEND_ENGINE_2_3))
#	define ZEND_ENGINE_2_2
#endif
#if !defined(ZEND_ENGINE_2_1) && (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION == 1 || defined(ZEND_ENGINE_2_2))
#	define ZEND_ENGINE_2_1
#endif

#define NOTHING
/* ZendEngine code Switcher */
#ifndef ZEND_ENGINE_2
#	define ZESW(v1, v2) v1
#else
#	define ZESW(v1, v2) v2
#endif
#ifdef ZEND_ENGINE_2_4
#	define ZEND_24(pre24, v24) v24
#else
#	define ZEND_24(pre24, v24) pre24
#endif

#ifdef do_alloca_with_limit
#	define xc_do_alloca(size, use_heap) do_alloca_with_limit(size, use_heap)
#	define xc_free_alloca(size, use_heap) free_alloca_with_limit(size, use_heap)
#elif defined(ALLOCA_FLAG)
#	define xc_do_alloca(size, use_heap) do_alloca(size, use_heap)
#	define xc_free_alloca(size, use_heap) free_alloca(size, use_heap)
#else
#	define xc_do_alloca(size, use_heap) do_alloca(size)
#	define xc_free_alloca(size, use_heap) free_alloca(size)
#	define ALLOCA_FLAG(x)
#endif
#ifndef Z_ISREF
#	define Z_ISREF(z) (z).is_ref
#endif
#ifndef Z_SET_ISREF
#	define Z_SET_ISREF(z) (z).is_ref = 1
#endif
#ifndef Z_UNSET_ISREF
#	define Z_UNSET_ISREF(z) (z).is_ref = 0
#endif
#ifndef Z_REFCOUNT
#	define Z_REFCOUNT(z) (z).refcount
#endif
#ifndef Z_SET_REFCOUNT
#	define Z_SET_REFCOUNT(z, rc) (z).refcount = rc
#endif
#ifndef IS_CONSTANT_TYPE_MASK
#	define IS_CONSTANT_TYPE_MASK (~IS_CONSTANT_INDEX)
#endif
/* {{{ dirty fix for PHP 6 */
#ifdef add_assoc_long_ex
static inline void xc_add_assoc_long_ex(zval *arg, char *key, uint key_len, long value)
{
	add_assoc_long_ex(arg, key, key_len, value);
}
#	undef add_assoc_long_ex
#	define add_assoc_long_ex xc_add_assoc_long_ex
#endif
#ifdef add_assoc_bool_ex
static inline void xc_add_assoc_bool_ex(zval *arg, char *key, uint key_len, zend_bool value)
{
	add_assoc_bool_ex(arg, key, key_len, value);
}
#	undef add_assoc_bool_ex
#	define add_assoc_bool_ex xc_add_assoc_bool_ex
#endif
#ifdef add_assoc_null_ex
static inline void xc_add_assoc_null_ex(zval *arg, char *key, uint key_len)
{
	add_assoc_null_ex(arg, key, key_len);
}
#	undef add_assoc_null_ex
#	define add_assoc_null_ex xc_add_assoc_null_ex
#endif
/* }}} */

#ifndef ZEND_ENGINE_2_6
typedef void zend_ast;
#endif

#ifdef ZEND_ENGINE_2_4
#	define Z_OP(op) (op)
#	define Z_OP_CONSTANT(op) (op).literal->constant
#	define Z_OP_TYPE(op) op##_##type
#	define Z_OP_TYPEOF_TYPE zend_uchar

#	define Z_CLASS_INFO(className) (className).info.user
#else
#	define Z_OP(op) (op).u
#	define Z_OP_CONSTANT(op) (op).u.constant
#	define Z_OP_TYPE(op) (op).op_type
#	define Z_OP_TYPEOF_TYPE int
typedef znode znode_op;

#	define Z_CLASS_INFO(className) (className)

static inline int php_output_start_default(TSRMLS_D) { return php_start_ob_buffer(NULL, 0, 1 TSRMLS_CC); }
static inline int php_output_get_contents(zval *p TSRMLS_DC) { return php_ob_get_buffer(p TSRMLS_CC); }
static inline int php_output_discard(TSRMLS_D) { php_end_ob_buffer(0, 0 TSRMLS_CC); return SUCCESS; }
#endif

/* unicode */
#ifdef IS_UNICODE
#	define UNISW(text, unicode) unicode
#else
#	define UNISW(text, unicode) text
#endif
#define BUCKET_KEY_SIZE(b) \
		(UNISW( \
			(b)->nKeyLength, \
				((b)->key.type == IS_UNICODE) \
				? UBYTES(b->nKeyLength) \
				: b->nKeyLength \
				))
#define BUCKET_KEY(b)      (UNISW((b)->arKey, (b)->key.arKey))
#define BUCKET_KEY_S(b)    ZSTR_S(BUCKET_KEY(b))
#define BUCKET_KEY_U(b)    ZSTR_U(BUCKET_KEY(b))
#define BUCKET_KEY_TYPE(b) (UNISW(IS_STRING,  (b)->key.type))
#ifdef IS_UNICODE
#	define BUCKET_HEAD_SIZE(b) XtOffsetOf(Bucket, key.arKey)
#else
#	define BUCKET_HEAD_SIZE(b) XtOffsetOf(Bucket, arKey)
#endif

#ifdef ZEND_ENGINE_2_4
#	define BUCKET_SIZE(b) (sizeof(Bucket) + BUCKET_KEY_SIZE(b))
#else
#	define BUCKET_SIZE(b) (BUCKET_HEAD_SIZE(b) + BUCKET_KEY_SIZE(b))
#endif

#ifndef IS_UNICODE
typedef char *zstr;
typedef const char *const_zstr;
#ifdef ZEND_ENGINE_2_4
typedef const char *const24_zstr;
typedef const char *const24_str;
#else
typedef char *const24_zstr;
typedef char *const24_str;
#endif

#	define ZSTR_S(s)     (s)
#	define ZSTR_U(s)     (s)
#	define ZSTR_V(s)     (s)
#	define ZSTR_PS(s)    (s)
#	define ZSTR_PU(s)    (s)
#	define ZSTR_PV(s)    (s)
#else
typedef const zstr const_zstr;
#	define ZSTR_S(zs)    ((zs).s)
#	define ZSTR_U(zs)    ((zs).u)
#	define ZSTR_V(zs)    ((zs).v)
#	define ZSTR_PS(pzs)  ((pzs)->s)
#	define ZSTR_PU(pzs)  ((pzs)->u)
#	define ZSTR_PV(pzs)  ((pzs)->v)
#endif

#ifndef ZSTR
#	define ZSTR(s)      (s)
#endif

#ifndef Z_UNIVAL
#	define Z_UNIVAL(zval) (zval).value.str.val
#	define Z_UNILEN(zval) (zval).value.str.len
#endif

/* {{{ u hash wrapper */
#ifndef IS_UNICODE
#	define zend_u_hash_add(ht, type, arKey, nKeyLength, pData, nDataSize, pDest) \
 	   zend_hash_add(ht, ZEND_24((char *), NOTHING) arKey, nKeyLength, pData, nDataSize, pDest)

#	define zend_u_hash_quick_add(ht, type, arKey, nKeyLength, h, pData, nDataSize, pDest) \
 	   zend_hash_quick_add(ht, ZEND_24((char *), NOTHING) arKey, nKeyLength, h, pData, nDataSize, pDest)

#	define zend_u_hash_update(ht, type, arKey, nKeyLength, pData, nDataSize, pDest) \
 	   zend_hash_update(ht, ZEND_24((char *), NOTHING) arKey, nKeyLength, pData, nDataSize, pDest)

#	define zend_u_hash_quick_update(ht, type, arKey, nKeyLength, h, pData, nDataSize, pDest) \
 	   zend_hash_quick_update(ht, ZEND_24((char *), NOTHING) arKey, nKeyLength, h, pData, nDataSize, pDest)

#	define zend_u_hash_find(ht, type, arKey, nKeyLength, pData) \
 	   zend_hash_find(ht, ZEND_24((char *), NOTHING) arKey, nKeyLength, pData)

#	define zend_u_hash_quick_find(ht, type, arKey, nKeyLength, h, pData) \
 	   zend_hash_quick_find(ht, ZEND_24((char *), NOTHING) arKey, nKeyLength, h, pData)

#	define zend_u_hash_exists(ht, type, arKey, nKeyLength) \
 	   zend_hash_exists(ht, ZEND_24((char *), NOTHING) arKey, nKeyLength)

#	define add_u_assoc_zval_ex(arg, type, key, key_len, value) \
		add_assoc_zval_ex(arg, key, key_len, value)

#	define zend_u_is_auto_global(type, name, name_len) \
		zend_is_auto_global(name, name_len)
#endif
/* }}} */

#ifndef ZVAL_COPY_VALUE
#	define ZVAL_COPY_VALUE(z, v) (z)->value = (v)->value
#endif

#ifndef ZVAL_ZVAL
#	define ZVAL_ZVAL(z, zv, copy, dtor) do {	\
		zval *__z = (z);						\
		zval *__zv = (zv);						\
		ZVAL_COPY_VALUE(__z, __zv);				\
		if (copy) {								\
			zval_copy_ctor(__z);				\
	    }										\
		if (dtor) {								\
			if (!copy) {						\
				ZVAL_NULL(__zv);				\
			}									\
			zval_ptr_dtor(&__zv);				\
	    }										\
	} while (0)
#endif

#ifndef RETVAL_ZVAL
#	define RETVAL_ZVAL(zv, copy, dtor)		ZVAL_ZVAL(return_value, zv, copy, dtor)
#endif

#ifndef RETURN_ZVAL
#	define RETURN_ZVAL(zv, copy, dtor)		{ RETVAL_ZVAL(zv, copy, dtor); return; }
#endif

/* the class entry type to be stored in class_table */
typedef ZESW(zend_class_entry, zend_class_entry*) xc_cest_t;

/* xc_cest_t to (zend_class_entry*) */
#define CestToCePtr(st) (ZESW(\
			&(st), \
			st \
			) )

/* ZCEP=zend class entry ptr */
#define ZCEP_REFCOUNT_PTR(pce) (ZESW( \
			(pce)->refcount, \
			&((pce)->refcount) \
			))

#ifndef ZEND_ENGINE_2_3
size_t xc_dirname(char *path, size_t len);
#define zend_dirname xc_dirname
long xc_atol(const char *str, int len);
#define zend_atol xc_atol
#endif

#ifndef ZEND_MOD_END
#	define ZEND_MOD_END {NULL, NULL, NULL, 0}
#endif

#ifndef PHP_FE_END
#	ifdef ZEND_ENGINE_2
#		define PHP_FE_END {NULL, NULL, NULL, 0, 0}
#	else
#		define PHP_FE_END {NULL, NULL, NULL}
#	endif
#endif

#endif /* XC_COMPATIBILITY_H_54F26ED90198353558718191D5EE244C */
