#include "xc_trace.h"
#include <stdio.h>
#include <stdarg.h>

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

