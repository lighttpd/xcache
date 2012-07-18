#ifndef _22077CFAC35518969B4416944ACBA159
#define _22077CFAC35518969B4416944ACBA159

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/* return op_array to install */
typedef zend_op_array *(*xc_sandboxed_func_t)(void *data TSRMLS_DC);
zend_op_array *xc_sandbox(xc_sandboxed_func_t sandboxed_func, void *data, ZEND_24(NOTHING, const) char *filename TSRMLS_DC);
const Bucket *xc_sandbox_user_function_begin(TSRMLS_D);
const Bucket *xc_sandbox_user_class_begin(TSRMLS_D);
zend_uint xc_sandbox_compilererror_cnt(TSRMLS_D);
#ifdef XCACHE_ERROR_CACHING
xc_compilererror_t *xc_sandbox_compilererrors(TSRMLS_D);
zend_uint xc_sandbox_compilererror_cnt(TSRMLS_D);
#endif

#endif /* _22077CFAC35518969B4416944ACBA159 */
