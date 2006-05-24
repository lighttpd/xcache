#ifndef __XCACHE_H
#define __XCACHE_H
#define XCACHE_NAME       "XCache"
#define XCACHE_VERSION    "1.0"
#define XCACHE_AUTHOR     "mOo"
#define XCACHE_COPYRIGHT  "Copyright (c) 2005-2006"
#define XCACHE_URL        "http://xcache.lighttpd.net"

#include <php.h>
#include <zend_compile.h>
#include <zend_API.h>
#include "php_ini.h"
#include "zend_hash.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "myshm.h"
#include "mem.h"
#include "lock.h"

#ifndef ZEND_WIN32
/* UnDefine if your filesystem doesn't support inodes */
#	define HAVE_INODE
#endif
#if !defined(ZEND_ENGINE_2_1) && (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 1 || PHP_MAJOR_VERSION > 5)
#	define ZEND_ENGINE_2_1
#endif

/* ZendEngine code Switcher */
#ifndef ZEND_ENGINE_2
#	define ZESW(v1, v2) v1
#else
#	define ZESW(v1, v2) v2
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
#define BUCKET_KEY(b) (UNISW((b)->arKey, (b)->key.u.string))
#define BUCKET_UKEY(b) (UNISW((b)->arKey, (b)->key.u.unicode))
#define BUCKET_KEY_TYPE(b) (UNISW(0, (b)->key.type))
#ifdef IS_UNICODE
#	define BUCKET_HEAD_SIZE(b) XtOffsetOf(Bucket, key)
#else
#	define BUCKET_HEAD_SIZE(b) XtOffsetOf(Bucket, arKey)
#endif
#define BUCKET_SIZE(b) (BUCKET_HEAD_SIZE(b) + BUCKET_KEY_SIZE(b))

#ifndef IS_UNICODE
typedef char *zstr;
#	define ZSTR_S(s) (s)
#	define ZSTR_U(s) (s)
#	define ZSTR_V(s) (s)
#else
#	define ZSTR_S(s) ((s)->s)
#	define ZSTR_U(s) ((s)->u)
#	define ZSTR_V(s) ((s)->v)
#endif

/* {{{ u hash wrapper */
#ifndef IS_UNICODE
#	define zend_u_hash_add(ht, type, arKey, nKeyLength, pData, nDataSize, pDest) \
 	   zend_hash_add(ht, arKey, nKeyLength, pData, nDataSize, pDest)

#	define zend_u_hash_find(ht, type, arKey, nKeyLength, pData) \
 	   zend_hash_find(ht, arKey, nKeyLength, pData)

#	define add_u_assoc_zval_ex(arg, type, key, key_len, value) \
		add_assoc_zval_ex(arg, key, key_len, value)

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

/* {{{ xc_cache_t */
typedef struct _xc_entry_t xc_entry_t;
typedef struct {
	int cacheid;
	xc_hash_t  *hcache; /* hash to cacheid */

	time_t     compiling;
	zend_ulong misses;
	zend_ulong hits;
	zend_ulong clogs;
	zend_ulong ooms;
	xc_lock_t  *lck;
	xc_shm_t   *shm; /* to which shm contains us */
	xc_mem_t   *mem; /* to which mem contains us */

	xc_entry_t **entries;
	xc_entry_t *deletes;
	xc_hash_t  *hentry; /* hash to entry */
} xc_cache_t;
/* }}} */
/* {{{ xc_classinfo_t */
typedef struct {
	UNISW(,zend_uchar type;)
	char *key;
	zend_uint key_size;
	xc_cest_t cest;
} xc_classinfo_t;
/* }}} */
/* {{{ xc_funcinfo_t */
typedef struct {
	UNISW(,zend_uchar type;)
	char *key;
	zend_uint key_size;
	zend_function func;
} xc_funcinfo_t;
/* }}} */
typedef enum { XC_TYPE_PHP, XC_TYPE_VAR } xc_entry_type_t;
/* {{{ xc_entry_data_php_t */
typedef struct {
	size_t sourcesize;
#ifdef HAVE_INODE
	int device;             /* the filesystem device */
	int inode;              /* the filesystem inode */
#endif
	time_t mtime;           /* the mtime of origin source file */

	zend_op_array *op_array;

	zend_uint funcinfo_cnt;
	xc_funcinfo_t *funcinfos;

	zend_uint classinfo_cnt;
	xc_classinfo_t *classinfos;
} xc_entry_data_php_t;
/* }}} */
/* {{{ xc_entry_data_var_t */
typedef struct {
	zval   *value;
	time_t etime;
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
	zend_ulong refcount;
	zend_ulong hits;
	time_t     ctime;           /* the ctime of this entry */
	time_t     atime;           /* the atime of this entry */
	time_t     dtime;           /* the deletion time of this entry */

#ifdef IS_UNICODE
	zend_uchar name_type;
#endif
	xc_entry_name_t name;

	union {
		xc_entry_data_php_t *php;
		xc_entry_data_var_t *var;
	} data;
};
/* }}} */

extern zend_module_entry xcache_module_entry;
#define phpext_xcache_ptr &xcache_module_entry

int xc_is_rw(const void *p);
int xc_is_ro(const void *p);
int xc_is_shm(const void *p);

#endif /* __XCACHE_H */
