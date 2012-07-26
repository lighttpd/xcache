#ifndef XC_SANDBOX_H_3AFE4094B1D005188B909F9B6538599C
#define XC_SANDBOX_H_3AFE4094B1D005188B909F9B6538599C

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

/* Purpose: run specified function in compiler sandbox, restore everything to previous state after it returns
 */

#include "xc_compatibility.h"

int xc_sandbox_module_init(int module_number TSRMLS_DC);
void xc_sandbox_module_shutdown();

/* return op_array to install */
typedef zend_op_array *(*xc_sandboxed_func_t)(void *data TSRMLS_DC);
zend_op_array *xc_sandbox(xc_sandboxed_func_t sandboxed_func, void *data, ZEND_24(NOTHING, const) char *filename TSRMLS_DC);
const Bucket *xc_sandbox_user_function_begin(TSRMLS_D);
const Bucket *xc_sandbox_user_class_begin(TSRMLS_D);
zend_uint xc_sandbox_compilererror_cnt(TSRMLS_D);
#ifdef XCACHE_ERROR_CACHING
struct _xc_compilererror_t;
struct _xc_compilererror_t *xc_sandbox_compilererrors(TSRMLS_D);
zend_uint xc_sandbox_compilererror_cnt(TSRMLS_D);
#endif

#endif /* XC_SANDBOX_H_3AFE4094B1D005188B909F9B6538599C */
