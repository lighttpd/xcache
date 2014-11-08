dnl {{{ === program start ========================================
divert(0)
#include <string.h>
#include <stdio.h>

#include "php.h"
#include "zend_extensions.h"
#include "zend_compile.h"
#include "zend_API.h"
#include "zend_ini.h"

EXPORT(`#include <stddef.h>')
EXPORT(`#include "xcache.h"')
EXPORT(`#include "mod_cacher/xc_cache.h"')
EXPORT(`#include "xcache/xc_shm.h"')
EXPORT(`#include "xcache/xc_allocator.h"')
#include "xc_processor.h"
#include "xcache/xc_const_string.h"
#include "xcache/xc_utils.h"
#include "util/xc_align.h"
#include "util/xc_trace.h"
#include "util/xc_util.h"
#include "xcache_globals.h"

#if defined(HARDENING_PATCH_HASH_PROTECT) && HARDENING_PATCH_HASH_PROTECT
extern unsigned int zend_hash_canary;
#endif
dnl }}}

include(__dir__`/debug-helper.m4')
include(__dir__`/type-helper.m4')
include(__dir__`/string-helper.m4')

/* {{{ call op_array ctor handler */
extern zend_bool xc_have_op_array_ctor;
static void xc_zend_extension_op_array_ctor_handler(zend_extension *extension, zend_op_array *op_array TSRMLS_DC)
{
	if (extension->op_array_ctor) {
		extension->op_array_ctor(op_array);
	}
}
/* }}} */
/* {{{ memsetptr */
IFAUTOCHECK(`dnl
static void *memsetptr(void *mem, void *content, size_t n)
{
	void **p = (void **) mem;
	void **end = (void **) ((char *) mem + n);
	while (p < end - sizeof(content)) {
		*p = content;
		p += sizeof(content);
	}
	if (p < end) {
		memset(p, -1, end - p);
	}
	return mem;
}
')
/* }}} */
dnl {{{ _xc_processor_t
typedef struct _xc_processor_t {
	char *p;
	size_t size;
	HashTable zvalptrs;
	zend_bool handle_reference; /* enable if to deal with reference */
	zend_bool have_references;
	ptrdiff_t relocatediff;

	const xc_entry_php_t *entry_php_src;
	const xc_entry_php_t *entry_php_dst;
	const xc_entry_data_php_t *php_src;
	const xc_entry_data_php_t *php_dst;
	const zend_class_entry *cache_ce;
	zend_ulong cache_class_index;

	const zend_op_array    *active_op_array_src;
	zend_op_array          *active_op_array_dst;
	const zend_class_entry *active_class_entry_src;
	zend_class_entry       *active_class_entry_dst;
	zend_uint                 active_class_index;
	zend_uint                 active_op_array_index;
	const xc_op_array_info_t *active_op_array_infos_src;

	zend_bool readonly_protection; /* wheather it's present */

	STRING_HELPER_T

	IFAUTOCHECK(xc_vector_t allocsizes;)
} xc_processor_t;
dnl }}}
STRING_HELPERS()

include(__dir__`/class-helper.m4')
