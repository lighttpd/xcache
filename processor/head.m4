dnl {{{ === program start ========================================
divert(0)
#include <string.h>
#include <stdio.h>

#include "php.h"
#include "zend_compile.h"
#include "zend_API.h"

#include "xcache.h"
#include "align.h"
#include "const_string.h"
#include "processor.h"
#include "stack.h"
#include "xcache_globals.h"

define(`SIZEOF_zend_uint', `sizeof(zend_uint)')
define(`COUNTOF_zend_uint', `1')
define(`SIZEOF_int', `sizeof(int)')
define(`COUNTOF_int', `1')
define(`SIZEOF_zend_function', `sizeof(zend_function)')
define(`COUNTOF_zend_function', `1')
define(`SIZEOF_zval_ptr', `sizeof(zval_ptr)')
define(`COUNTOF_zval_ptr', `1')
define(`SIZEOF_xc_entry_name_t', `sizeof(xc_entry_name_t)')
define(`COUNTOF_xc_entry_name_t', `1')

ifdef(`XCACHE_ENABLE_TEST', `
#undef NDEBUG
#include <assert.h>
include(structinfo.m4)
')
#ifndef NDEBUG
#	undef inline
#define inline
#endif

typedef zval *zval_ptr;
typedef zend_uchar zval_data_type;

#define MAX_DUP_STR_LEN 256
dnl }}}
/* export: typedef struct _processor_t processor_t; :export {{{ */
struct _processor_t {
	char *p;
	zend_uint size;
	HashTable strings;
	HashTable zvalptrs;
	zend_bool reference; /* enable if to deal with reference */
	const xc_entry_t *xce_src;
	const xc_entry_t *xce_dst;
	const zend_class_entry *cache_ce;
	zend_uint cache_class_num;

	const zend_op          *active_opcodes_src;
	zend_op                *active_opcodes_dst;
	const zend_class_entry *active_class_entry_src;
	zend_class_entry       *active_class_entry_dst;
	zend_uint               active_class_num;

	zend_bool readonly_protection; /* wheather it's present */
IFASSERT(xc_stack_t allocsizes;)
};
/* }}} */
#ifdef XCACHE_HAVE_DPRINT
static void xc_dprint_indent(int indent) /* {{{ */
{
	int i;
	for (i = 0; i < indent; i ++) {
		fprintf(stderr, "  ");
	}
}
#endif
/* }}} */
/* {{{ xc_calc_string_n */
REDEF(`KIND', `calc')
static inline void xc_calc_string_n(processor_t *processor, zend_uchar type, char *str, long size IFASSERT(`, int relayline')) {
	pushdef(`__LINE__', `relayline')
	int realsize = UNISW(size, (type == IS_UNICODE) ? UBYTES(size) : size);

	if (realsize > MAX_DUP_STR_LEN) {
		ALLOC(, char, realsize)
	}
	else if (zend_u_hash_add(&processor->strings, type, str, size, (void*)&str, sizeof(char*), NULL) == SUCCESS) {
		/* new string */
		ALLOC(, char, realsize)
	} 
	IFASSERT(`
		else {
			dnl fprintf(stderr, "dupstr %s\n", str);
		}
	')
	popdef(`__LINE__')
}
/* }}} */
/* {{{ xc_store_string_n */
REDEF(`KIND', `store')
static inline char *xc_store_string_n(processor_t *processor, zend_uchar type, char *str, long size IFASSERT(`, int relayline')) {
	pushdef(`__LINE__', `relayline')
	int realsize = UNISW(size, (type == IS_UNICODE) ? UBYTES(size) : size);
	char *s;

	if (realsize > MAX_DUP_STR_LEN) {
		ALLOC(s, char, realsize)
		memcpy(s, str, realsize);
	}
	else if (zend_u_hash_find(&processor->strings, type, str, size, (void*)&s) != SUCCESS) {
		/* new string */
		ALLOC(s, char, realsize)
		memcpy(s, str, realsize);
		zend_u_hash_add(&processor->strings, type, str, size, (void*)&s, sizeof(char*), NULL);
	}
	else {
		s = *(char**)s;
	}
	return s;
	popdef(`__LINE__')
}
/* }}} */
/* {{{ xc_get_class_num
 * return class_index + 1
 */
static zend_uint xc_get_class_num(processor_t *processor, zend_class_entry *ce) {
	zend_uint i;
	const xc_entry_t *xce = processor->xce_src;
	zend_class_entry *ceptr;

	if (processor->cache_ce == ce) {
		return processor->cache_class_num;
	}
	for (i = 0; i < xce->data.php->classinfo_cnt; i ++) {
		ceptr = CestToCePtr(xce->data.php->classinfos[i].cest);
		if (ZCEP_REFCOUNT_PTR(ceptr) == ZCEP_REFCOUNT_PTR(ce)) {
			processor->cache_ce = ceptr;
			processor->cache_class_num = i + 1;
			return i + 1;
		}
	}
	assert(0);
	return (zend_uint) -1;
}
/* }}} */
/* {{{ xc_get_class */
#ifdef ZEND_ENGINE_2
static zend_class_entry *xc_get_class(processor_t *processor, zend_uint class_num) {
	/* must be parent or currrent class */
	assert(class_num <= processor->active_class_num);
	return CestToCePtr(processor->xce_dst->data.php->classinfos[class_num - 1].cest);
}
#endif
/* }}} */
#ifdef ZEND_ENGINE_2
/* fix method on store */
static void xc_fix_method(processor_t *processor, zend_op_array *dst) /* {{{ */
{
	zend_function *zf = (zend_function *) dst;
	zend_class_entry *ce = processor->active_class_entry_dst;

	/* Fixing up the default functions for objects here since
	 * we need to compare with the newly allocated functions
	 *
	 * caveat: a sub-class method can have the same name as the
	 * parent~s constructor and create problems.
	 */

	if (zf->common.fn_flags & ZEND_ACC_CTOR) {
		if (!ce->constructor) {
			ce->constructor = zf;
		}
	}
	else if (zf->common.fn_flags & ZEND_ACC_DTOR) {
		ce->destructor = zf;
	}
	else if (zf->common.fn_flags & ZEND_ACC_CLONE) {
		ce->clone = zf;
	}
	else {
#define SET_IF_SAME_NAME(member) \
		do { \
			if(!strcasecmp(zf->common.function_name, #member)) { \
				ce->member = zf; \
			} \
		} \
		while(0)
		/* if(ce->member && !strcmp(zf->common.function_name, ce->member->common.function_name)) { \ */

		SET_IF_SAME_NAME(__get);
		SET_IF_SAME_NAME(__set);
		SET_IF_SAME_NAME(__unset);
		SET_IF_SAME_NAME(__isset);
		SET_IF_SAME_NAME(__call);
#if PHP_MAJOR_VERSION >= 6
		SET_IF_SAME_NAME(__tostring);
#endif

#undef SET_IF_SAME_NAME
	}
}
/* }}} */
#endif
dnl ================ export API
/* export: xc_entry_t *xc_processor_store_xc_entry_t(xc_entry_t *src TSRMLS_DC); :export {{{ */
xc_entry_t *xc_processor_store_xc_entry_t(xc_entry_t *src TSRMLS_DC) {
	xc_entry_t *dst;
	processor_t processor;

	memset(&processor, 0, sizeof(processor));
	if (src->type == XC_TYPE_VAR) {
		processor.reference = 1;
	}

	IFASSERT(`xc_stack_init(&processor.allocsizes);')

	/* calc size */ {
		zend_hash_init(&processor.strings, 0, NULL, NULL, 0);
		if (processor.reference) {
			zend_hash_init(&processor.zvalptrs, 0, NULL, NULL, 0);
		}

		processor.size = 0;
		/* allocate */
		processor.size = ALIGN(processor.size + sizeof(src[0]));

		xc_calc_xc_entry_t(&processor, src TSRMLS_CC);
		if (processor.reference) {
			zend_hash_destroy(&processor.zvalptrs);
		}
		zend_hash_destroy(&processor.strings);
	}
	src->size = processor.size;

	IFASSERT(`xc_stack_reverse(&processor.allocsizes);')
	/* store {{{ */
	{
		IFASSERT(`char *oldp;')
		zend_hash_init(&processor.strings, 0, NULL, NULL, 0);
		if (processor.reference) {
			zend_hash_init(&processor.zvalptrs, 0, NULL, NULL, 0);
		}

		/* mem :) */
		processor.p = (char *)xc_mem_malloc(src->cache->mem, processor.size);
		if (processor.p == NULL) {
			dst = NULL;
			goto err_alloc;
			return NULL;
		}
		IFASSERT(`oldp = processor.p;')
		assert(processor.p == (char *) ALIGN(processor.p));

		/* allocate */
		dst = (xc_entry_t *) processor.p;
		processor.p = (char *) ALIGN(processor.p + sizeof(dst[0]));

		xc_store_xc_entry_t(&processor, dst, src TSRMLS_CC);
		IFASSERT(` {
			int real = processor.p - oldp;
			int should = processor.size;
			if (real != processor.size) {
				fprintf(stderr, "real %d - should %d = %d\n", real, should, real - should);
				abort();
			}
		}')
err_alloc:
		if (processor.reference) {
			zend_hash_destroy(&processor.zvalptrs);
		}
		zend_hash_destroy(&processor.strings);
	}
	/* }}} */

	IFASSERT(`xc_stack_destroy(&processor.allocsizes);')

	return dst;
}
/* }}} */
/* export: xc_entry_t *xc_processor_restore_xc_entry_t(xc_entry_t *dst, const xc_entry_t *src, zend_bool readonly_protection TSRMLS_DC); :export {{{ */
xc_entry_t *xc_processor_restore_xc_entry_t(xc_entry_t *dst, const xc_entry_t *src, zend_bool readonly_protection TSRMLS_DC) {
	processor_t processor;

	memset(&processor, 0, sizeof(processor));
	processor.readonly_protection = readonly_protection;

	xc_restore_xc_entry_t(&processor, dst, src TSRMLS_CC);
	return dst;
}
/* }}} */
/* export: zval *xc_processor_restore_zval(zval *dst, const zval *src TSRMLS_DC); :export {{{ */
zval *xc_processor_restore_zval(zval *dst, const zval *src TSRMLS_DC) {
	processor_t processor;

	memset(&processor, 0, sizeof(processor));
	processor.reference = 1;

	zend_hash_init(&processor.zvalptrs, 0, NULL, NULL, 0);
	dnl fprintf(stderr, "mark[%p] = %p\n", src, dst);
	zend_hash_add(&processor.zvalptrs, (char *)src, sizeof(src), (void*)&dst, sizeof(dst), NULL);
	xc_restore_zval(&processor, dst, src TSRMLS_CC);
	zend_hash_destroy(&processor.zvalptrs);

	return dst;
}
/* }}} */
/* export: void xc_dprint(xc_entry_t *src, int indent TSRMLS_DC); :export {{{ */
#ifdef XCACHE_HAVE_DPRINT
void xc_dprint(xc_entry_t *src, int indent TSRMLS_DC) {
	IFDPRINT(`INDENT()`'fprintf(stderr, "xc_entry_t:src");')
	xc_dprint_xc_entry_t(src, indent TSRMLS_CC);
}
#endif
/* }}} */
