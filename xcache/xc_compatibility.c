#include "xc_compatibility.h"

#ifndef ZEND_ENGINE_2_3
#include "ext/standard/php_string.h"
size_t xc_dirname(char *path, size_t len) /* {{{ */
{
#ifdef ZEND_ENGINE_2
	return php_dirname(path, len);
#else
	php_dirname(path, len);
	return strlen(path);
#endif
}
/* }}} */

long xc_atol(const char *str, int str_len) /* {{{ */
{
	long retval;

	if (!str_len) {
		str_len = strlen(str);
	}

	retval = strtol(str, NULL, 0);
	if (str_len > 0) {
		switch (str[str_len - 1]) {
		case 'g':
		case 'G':
			retval *= 1024;
			/* break intentionally missing */
		case 'm':
		case 'M':
			retval *= 1024;
			/* break intentionally missing */
		case 'k':
		case 'K':
			retval *= 1024;
			break;
		}
	}

	return retval;
}
/* }}} */
#endif

#if defined(ZEND_ENGINE_2) && !defined(ZEND_ENGINE_2_2)
void *xc_object_store_get_object_by_handle(zend_object_handle handle TSRMLS_DC) /* {{{ */
{
	zval zobject;
	Z_OBJ_HANDLE_P(&zobject) = handle;
	return zend_object_store_get_object(&zobject TSRMLS_CC);
}
/* }}} */
void xc_objects_store_add_ref_by_handle(zend_object_handle handle TSRMLS_DC) /* {{{ */
{
	zval zobject;
	Z_OBJ_HANDLE_P(&zobject) = handle;
	zend_objects_store_add_ref(&zobject TSRMLS_CC);
}
/* }}} */
void xc_objects_store_del_ref_by_handle(zend_object_handle handle TSRMLS_DC) /* {{{ */
{
	zval zobject;
	Z_OBJ_HANDLE_P(&zobject) = handle;
	zobject.refcount = 0;
	zend_objects_store_del_ref(&zobject TSRMLS_CC);
}
/* }}} */
#endif
