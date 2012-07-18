#ifndef __XCACHE_H
#define __XCACHE_H
#define XCACHE_NAME       "XCache"
#ifndef XCACHE_VERSION
#	define XCACHE_VERSION "2.1.0"
#endif
#define XCACHE_AUTHOR     "mOo"
#define XCACHE_COPYRIGHT  "Copyright (c) 2005-2012"
#define XCACHE_URL        "http://xcache.lighttpd.net"
#define XCACHE_WIKI_URL   XCACHE_URL "/wiki"

#include <php.h>
#include <zend_compile.h>
#include <zend_API.h>
#include <zend.h>
#include "php_ini.h"
#include "zend_hash.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "xcache/xc_shm.h"
#include "xcache/xc_lock.h"

#if !defined(ZEND_ENGINE_2_4) && (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 4 || PHP_MAJOR_VERSION > 5)
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
#	define my_do_alloca(size, use_heap) do_alloca_with_limit(size, use_heap)
#	define my_free_alloca(size, use_heap) free_alloca_with_limit(size, use_heap)
#elif defined(ALLOCA_FLAG)
#	define my_do_alloca(size, use_heap) do_alloca(size, use_heap)
#	define my_free_alloca(size, use_heap) free_alloca(size, use_heap)
#else
#	define my_do_alloca(size, use_heap) do_alloca(size)
#	define my_free_alloca(size, use_heap) free_alloca(size)
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
static inline void my_add_assoc_long_ex(zval *arg, char *key, uint key_len, long value)
{
	add_assoc_long_ex(arg, key, key_len, value);
}
#	undef add_assoc_long_ex
#	define add_assoc_long_ex my_add_assoc_long_ex
#endif
#ifdef add_assoc_bool_ex
static inline void my_add_assoc_bool_ex(zval *arg, char *key, uint key_len, zend_bool value)
{
	add_assoc_bool_ex(arg, key, key_len, value);
}
#	undef add_assoc_bool_ex
#	define add_assoc_bool_ex my_add_assoc_bool_ex
#endif
#ifdef add_assoc_null_ex
static inline void my_add_assoc_null_ex(zval *arg, char *key, uint key_len)
{
	add_assoc_null_ex(arg, key, key_len);
}
#	undef add_assoc_null_ex
#	define add_assoc_null_ex my_add_assoc_null_ex
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
#endif

/* }}} */

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


#define ECALLOC_N(x, n) ((x) = ecalloc(n, sizeof((x)[0])))
#define ECALLOC_ONE(x) ECALLOC_N(x, 1)



typedef ulong xc_hash_value_t;
typedef struct {
	size_t bits;
	size_t size;
	xc_hash_value_t mask;
} xc_hash_t;

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

#define ZCE_REFCOUNT_PTR(ce) ZCE_REFCOUNT_PTR(&ce)

typedef zend_op_array *(zend_compile_file_t)(zend_file_handle *h, int type TSRMLS_DC);

typedef struct _xc_entry_t xc_entry_t;
typedef struct _xc_entry_data_php_t xc_entry_data_php_t;
/* {{{ xc_cache_t */
typedef struct {
	int cacheid;
	xc_hash_t  *hcache; /* hash to cacheid */

	time_t     compiling;
	zend_ulong updates;
	zend_ulong hits;
	zend_ulong clogs;
	zend_ulong ooms;
	zend_ulong errors;
	xc_lock_t  *lck;
	xc_shm_t   *shm; /* which shm contains us */
	xc_mem_t   *mem; /* which mem contains us */

	xc_entry_t **entries;
	int entries_count;
	xc_entry_data_php_t **phps;
	int phps_count;
	xc_entry_t *deletes;
	int deletes_count;
	xc_hash_t  *hentry; /* hash settings to entry */
	xc_hash_t  *hphp;   /* hash settings to php */

	time_t     last_gc_deletes;
	time_t     last_gc_expires;

	time_t     hits_by_hour_cur_time;
	zend_uint  hits_by_hour_cur_slot;
	zend_ulong hits_by_hour[24];
	time_t     hits_by_second_cur_time;
	zend_uint  hits_by_second_cur_slot;
	zend_ulong hits_by_second[5];
} xc_cache_t;
/* }}} */
/* {{{ xc_op_array_info_detail_t */
typedef struct {
	zend_uint index;
	zend_uint info;
} xc_op_array_info_detail_t;
/* }}} */
/* {{{ xc_op_array_info_t */
typedef struct {
#ifdef ZEND_ENGINE_2_4
	zend_uint literalinfo_cnt;
	xc_op_array_info_detail_t *literalinfos;
#else
	zend_uint oplineinfo_cnt;
	xc_op_array_info_detail_t *oplineinfos;
#endif
} xc_op_array_info_t;
/* }}} */
/* {{{ xc_classinfo_t */
typedef struct {
#ifdef IS_UNICODE
	zend_uchar   type;
#endif
	const24_zstr key;
	zend_uint    key_size;
	ulong        h;
	zend_uint  methodinfo_cnt;
	xc_op_array_info_t *methodinfos;
	xc_cest_t    cest;
#ifndef ZEND_COMPILE_DELAYED_BINDING
	int          oplineno;
#endif
} xc_classinfo_t;
/* }}} */
#ifdef HAVE_XCACHE_CONSTANT
/* {{{ xc_constinfo_t */
typedef struct {
#ifdef IS_UNICODE
	zend_uchar    type;
#endif
	const24_zstr  key;
	zend_uint     key_size;
	ulong         h;
	zend_constant constant;
} xc_constinfo_t;
/* }}} */
#endif
/* {{{ xc_funcinfo_t */
typedef struct {
#ifdef IS_UNICODE
	zend_uchar     type;
#endif
	const24_zstr   key;
	zend_uint      key_size;
	ulong          h;
	xc_op_array_info_t op_array_info;
	zend_function func;
} xc_funcinfo_t;
/* }}} */
#ifdef ZEND_ENGINE_2_1
/* {{{ xc_autoglobal_t */
typedef struct {
#ifdef IS_UNICODE
	zend_uchar   type;
#endif
	const24_zstr key;
	zend_uint    key_len;
	ulong        h;
} xc_autoglobal_t;
/* }}} */
#endif
typedef struct {
	char digest[16];
} xc_md5sum_t;
/* {{{ xc_compilererror_t */
typedef struct {
	int type;
	uint lineno;
	int error_len;
	char *error;
} xc_compilererror_t;
/* }}} */
/* {{{ xc_entry_data_php_t */
struct _xc_entry_data_php_t {
	xc_entry_data_php_t *next;
	xc_hash_value_t      hvalue;

	xc_md5sum_t md5;        /* md5sum of the source */
	zend_ulong  refcount;   /* count of entries referencing to this data */

	zend_ulong hits;        /* hits of this php */
	size_t     size;

	xc_op_array_info_t op_array_info;
	zend_op_array *op_array;

#ifdef HAVE_XCACHE_CONSTANT
	zend_uint constinfo_cnt;
	xc_constinfo_t *constinfos;
#endif

	zend_uint funcinfo_cnt;
	xc_funcinfo_t *funcinfos;

	zend_uint classinfo_cnt;
	xc_classinfo_t *classinfos;
#ifndef ZEND_COMPILE_DELAYED_BINDING
	zend_bool have_early_binding;
#endif

#ifdef ZEND_ENGINE_2_1
	zend_uint autoglobal_cnt;
	xc_autoglobal_t *autoglobals;
#endif

#ifdef E_STRICT
	zend_uint compilererror_cnt;
	xc_compilererror_t *compilererrors;
#endif

	zend_bool  have_references;
};
/* }}} */
typedef zvalue_value xc_entry_name_t;
/* {{{ xc_entry_t */
struct _xc_entry_t {
	xc_entry_t *next;

	size_t     size;
	time_t     ctime;           /* creation ctime of this entry */
	time_t     atime;           /*   access atime of this entry */
	time_t     dtime;           /*  deletion time of this entry */
	zend_ulong hits;
	zend_ulong ttl;

	xc_entry_name_t name;
};

typedef struct {
	xc_entry_t entry;
	xc_entry_data_php_t *php;

	zend_ulong refcount;    /* count of php instances holding this entry */
	time_t file_mtime;
	size_t file_size;
	int file_device;
	int file_inode;

	int    filepath_len;
	ZEND_24(NOTHING, const) char *filepath;
	int    dirpath_len;
	char  *dirpath;
#ifdef IS_UNICODE
	int    ufilepath_len;
	UChar *ufilepath;
	int    udirpath_len;
	UChar *udirpath;
#endif
} xc_entry_php_t;

typedef struct {
	xc_entry_t entry;
#ifdef IS_UNICODE
	zend_uchar name_type;
#endif
	zval      *value;
	zend_bool  have_references;
} xc_entry_var_t;
/* }}} */
typedef struct xc_entry_hash_t { /* {{{ */
	xc_hash_value_t cacheid;
	xc_hash_value_t entryslotid;
} xc_entry_hash_t;
/* }}} */

extern zend_module_entry xcache_module_entry;
#define phpext_xcache_ptr &xcache_module_entry

int xc_is_rw(const void *p);
int xc_is_ro(const void *p);
int xc_is_shm(const void *p);
/* {{{ xc_gc_op_array_t */
typedef struct {
#ifdef ZEND_ENGINE_2
	zend_uint num_args;
	zend_arg_info *arg_info;
#endif
	zend_op *opcodes;
} xc_gc_op_array_t;
/* }}} */
void xc_gc_add_op_array(xc_gc_op_array_t *gc_op_array TSRMLS_DC);
void xc_fix_op_array_info(const xc_entry_php_t *xce, const xc_entry_data_php_t *php, zend_op_array *op_array, int shallow_copy, const xc_op_array_info_t *op_array_info TSRMLS_DC);

#endif /* __XCACHE_H */
