dnl === program start ========================================
divert(0)
ifdef(`XCACHE_ENABLE_TEST', `
m4_errprint(`AUTOCHECK INFO: runtime autocheck Enabled (debug build)')
', `
m4_errprint(`AUTOCHECK INFO: runtime autocheck Disabled (optimized build)')
')

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
dnl

#ifdef DEBUG_SIZE
static int xc_totalsize = 0;
#endif

#include "processor/debug.h"
#include "processor/types.h"
include(__dir__`/types.m4')

/* {{{ call op_array ctor handler */
extern zend_bool xc_have_op_array_ctor;
static void xc_zend_extension_op_array_ctor_handler(zend_extension *extension, zend_op_array *op_array TSRMLS_DC)
{
	if (extension->op_array_ctor) {
		extension->op_array_ctor(op_array);
	}
}
/* }}} */
#include "processor/processor-t.h"

#include "processor/string-helper.h"
include(__dir__`/string-helper.m4')
include(__dir__`/class-helper.m4')
