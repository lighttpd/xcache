#ifndef __XCACHE_H
#define __XCACHE_H
#define XCACHE_NAME       "XCache"
#ifndef XCACHE_VERSION
#	define XCACHE_VERSION "3.0.0-dev"
#endif
#define XCACHE_AUTHOR     "mOo"
#define XCACHE_COPYRIGHT  "Copyright (c) 2005-2012"
#define XCACHE_URL        "http://xcache.lighttpd.net"
#define XCACHE_WIKI_URL   XCACHE_URL "/wiki"

#include <php.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "xcache/xc_shm.h"
#include "xcache/xc_lock.h"
#include "xcache/xc_compatibility.h"

#define ECALLOC_N(x, n) ((x) = ecalloc(n, sizeof((x)[0])))
#define ECALLOC_ONE(x) ECALLOC_N(x, 1)

typedef ulong xc_hash_value_t;
typedef struct {
	size_t bits;
	size_t size;
	xc_hash_value_t mask;
} xc_hash_t;

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
