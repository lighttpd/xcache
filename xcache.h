#ifndef __XCACHE_H
#define __XCACHE_H
#define XCACHE_NAME       "XCache"
#define XCACHE_VERSION    "2.0.0-dev"
#define XCACHE_AUTHOR     "mOo"
#define XCACHE_COPYRIGHT  "Copyright (c) 2005-2007"
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
#include "xc_shm.h"
#include "lock.h"

#define HAVE_INODE
#if !defined(ZEND_ENGINE_2_1) && (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 1 || PHP_MAJOR_VERSION > 5)
#	define ZEND_ENGINE_2_1
#endif
#if !defined(ZEND_ENGINE_2_2) && (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 2 || PHP_MAJOR_VERSION > 5)
#	define ZEND_ENGINE_2_2
#endif
#if !defined(ZEND_ENGINE_2_3) && (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3 || PHP_MAJOR_VERSION > 5)
#	define ZEND_ENGINE_2_3
#endif

#define NOTHING
/* ZendEngine code Switcher */
#ifndef ZEND_ENGINE_2
#	define ZESW(v1, v2) v1
#else
#	define ZESW(v1, v2) v2
#endif

#ifdef ALLOCA_FLAG
#	define my_do_alloca(size, use_heap) do_alloca(size, use_heap)
#	define my_free_alloca(size, use_heap) free_alloca(size, use_heap)
#else
#	define my_do_alloca(size, use_heap) do_alloca(size)
#	define my_free_alloca(size, use_heap) free_alloca(size)
#	define ALLOCA_FLAG(x)
#endif
#ifndef Z_SET_ISREF
#	define Z_SET_ISREF(z) (z).is_ref = 1;
#endif
#ifndef Z_UNSET_ISREF
#	define Z_UNSET_ISREF(z) (z).is_ref = 0;
#endif
#ifndef Z_SET_REFCOUNT
#	define Z_SET_REFCOUNT(z, rc) (z).refcount = rc;
#endif
#ifndef IS_CONSTANT_TYPE_MASK
#	define IS_CONSTANT_TYPE_MASK 0xf
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
#define BUCKET_KEY_S(b)    (UNISW((b)->arKey, (b)->key.arKey.s))
#define BUCKET_KEY_U(b)    (UNISW((b)->arKey, (b)->key.arKey.u))
#define BUCKET_KEY_TYPE(b) (UNISW(IS_STRING,  (b)->key.type))
#ifdef IS_UNICODE
#	define BUCKET_HEAD_SIZE(b) XtOffsetOf(Bucket, key.arKey)
#else
#	define BUCKET_HEAD_SIZE(b) XtOffsetOf(Bucket, arKey)
#endif
#define BUCKET_SIZE(b) (BUCKET_HEAD_SIZE(b) + BUCKET_KEY_SIZE(b))

#ifndef IS_UNICODE
typedef char *zstr;
#	define ZSTR_S(s)     (s)
#	define ZSTR_U(s)     (s)
#	define ZSTR_V(s)     (s)
#	define ZSTR_PS(s)    (s)
#	define ZSTR_PU(s)    (s)
#	define ZSTR_PV(s)    (s)
#else
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
 	   zend_hash_add(ht, arKey, nKeyLength, pData, nDataSize, pDest)

#	define zend_u_hash_quick_add(ht, type, arKey, nKeyLength, h, pData, nDataSize, pDest) \
 	   zend_hash_quick_add(ht, arKey, nKeyLength, h, pData, nDataSize, pDest)

#	define zend_u_hash_update(ht, type, arKey, nKeyLength, pData, nDataSize, pDest) \
 	   zend_hash_update(ht, arKey, nKeyLength, pData, nDataSize, pDest)

#	define zend_u_hash_quick_update(ht, type, arKey, nKeyLength, h, pData, nDataSize, pDest) \
 	   zend_hash_quick_update(ht, arKey, nKeyLength, h, pData, nDataSize, pDest)

#	define zend_u_hash_find(ht, type, arKey, nKeyLength, pData) \
 	   zend_hash_find(ht, arKey, nKeyLength, pData)

#	define zend_u_hash_quick_find(ht, type, arKey, nKeyLength, h, pData) \
 	   zend_hash_quick_find(ht, arKey, nKeyLength, h, pData)

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
	int bits;
	int size;
	int mask;
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
	zend_ulong misses;
	zend_ulong hits;
	zend_ulong clogs;
	zend_ulong ooms;
	zend_ulong errors;
	xc_lock_t  *lck;
	xc_shm_t   *shm; /* to which shm contains us */
	xc_mem_t   *mem; /* to which mem contains us */

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
/* {{{ xc_classinfo_t */
typedef struct {
#ifdef IS_UNICODE
	zend_uchar type;
#endif
	zstr      key;
	zend_uint key_size;
	ulong     h;
	xc_cest_t cest;
#ifndef ZEND_COMPILE_DELAYED_BINDING
	int       oplineno;
#endif
} xc_classinfo_t;
/* }}} */
#ifdef HAVE_XCACHE_CONSTANT
/* {{{ xc_constinfo_t */
typedef struct {
#ifdef IS_UNICODE
	zend_uchar type;
#endif
	zstr      key;
	zend_uint key_size;
	ulong     h;
	zend_constant constant;
} xc_constinfo_t;
/* }}} */
#endif
/* {{{ xc_funcinfo_t */
typedef struct {
#ifdef IS_UNICODE
	zend_uchar type;
#endif
	zstr      key;
	zend_uint key_size;
	ulong     h;
	zend_function func;
} xc_funcinfo_t;
/* }}} */
#ifdef ZEND_ENGINE_2_1
/* {{{ xc_autoglobal_t */
typedef struct {
#ifdef IS_UNICODE
	zend_uchar type;
#endif
	zstr       key;
	zend_uint  key_len;
	ulong      h;
} xc_autoglobal_t;
/* }}} */
#endif
typedef enum { XC_TYPE_PHP, XC_TYPE_VAR } xc_entry_type_t;
typedef char xc_md5sum_t[16];
/* {{{ xc_compilererror_t */
typedef struct {
	uint lineno;
	int error_len;
	char *error;
} xc_compilererror_t;
/* }}} */
/* {{{ xc_entry_data_php_t */
struct _xc_entry_data_php_t {
	xc_hash_value_t hvalue; /* hash of md5 */
	xc_entry_data_php_t *next;
	xc_cache_t *cache;      /* which cache it's on */

	xc_md5sum_t md5;        /* md5sum of the source */
	zend_ulong  refcount;   /* count of entries referencing to this data */

	size_t sourcesize;
	zend_ulong hits;        /* hits of this php */
	size_t     size;

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
/* {{{ xc_entry_data_var_t */
typedef struct {
	zval   *value;

	zend_bool  have_references;
} xc_entry_data_var_t;
/* }}} */
typedef zvalue_value xc_entry_name_t;
/* {{{ xc_entry_t */
struct _xc_entry_t {
	xc_entry_type_t type;
	xc_hash_value_t hvalue;
	xc_entry_t *next;
	xc_cache_t *cache;      /* which cache it's on */

	size_t     size;
	zend_ulong refcount;    /* count of instances holding this entry */
	zend_ulong hits;
	time_t     ctime;           /* the ctime of this entry */
	time_t     atime;           /* the atime of this entry */
	time_t     dtime;           /* the deletion time of this entry */
	long       ttl;             /* ttl of time entry, var only */

#ifdef IS_UNICODE
	zend_uchar name_type;
#endif
	xc_entry_name_t name;

	union {
		xc_entry_data_php_t *php;
		xc_entry_data_var_t *var;
	} data;

	time_t mtime;           /* the mtime of origin source file */
#ifdef HAVE_INODE
	int device;             /* the filesystem device */
	int inode;              /* the filesystem inode */
#endif
};
/* }}} */

extern zend_module_entry xcache_module_entry;
#define phpext_xcache_ptr &xcache_module_entry

int xc_is_rw(const void *p);
int xc_is_ro(const void *p);
int xc_is_shm(const void *p);
void xc_gc_add_op_array(zend_op_array *op_array TSRMLS_DC);

#endif /* __XCACHE_H */
