#ifdef TEST
#	define PHP_DIR_SEPARATOR '/'
#else
#	include "php.h"
#endif
#include "xc_trace.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

const char *xc_trace_get_basename(const char *path) /* {{{ */
{
	const char *last_separator = strrchr(path, PHP_DIR_SEPARATOR);
	return last_separator ? last_separator + 1 : path;
}
/* }}} */
int xc_vtrace(const char *fmt, va_list args) /* {{{ */
{
	return vfprintf(stderr, fmt, args);
}
/* }}} */
int xc_trace(const char *fmt, ...) /* {{{ */
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = xc_vtrace(fmt, args);
	va_end(args);
	return ret;
}
/* }}} */

