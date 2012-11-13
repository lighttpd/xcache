#ifndef XC_TRACE_H_709AE2523EDACB72B54D9CB42DDB0FEE
#define XC_TRACE_H_709AE2523EDACB72B54D9CB42DDB0FEE

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

#ifdef ZEND_WIN32
#	ifndef inline
#		ifdef ZEND_WIN32_FORCE_INLINE
#			define inline __forceinline
#		else
#			define inline
#		endif
#	endif
#endif

#ifdef XCACHE_DEBUG
#	define IFDEBUG(x) (x)
int xc_vtrace(const char *fmt, va_list args);
int xc_trace(const char *fmt, ...) ZEND_ATTRIBUTE_PTR_FORMAT(printf, 1, 2);

#	ifdef ZEND_WIN32
static inline int TRACE(const char *fmt, ...) 
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = xc_vtrace(fmt, args);
	va_end(args);
	return ret;
}
#	else
const char *xc_trace_get_basename(const char *path);
#		define TRACE(fmt, ...) \
		xc_trace("%s:%04d: " fmt "\r\n", xc_trace_get_basename(__FILE__), __LINE__, __VA_ARGS__)
#	endif /* ZEND_WIN32 */
#   undef NDEBUG
#   undef inline
#   define inline
#else /* XCACHE_DEBUG */

#	ifdef ZEND_WIN32
static inline int TRACE_DUMMY(const char *fmt, ...)
{
	return 0;
}
#		define TRACE 1 ? 0 : TRACE_DUMMY
#	else
#		define TRACE(fmt, ...) do { } while (0)
#	endif /* ZEND_WIN32 */

#	define IFDEBUG(x) do { } while (0)
#endif /* XCACHE_DEBUG */
#include <assert.h>

#endif /* XC_TRACE_H_709AE2523EDACB72B54D9CB42DDB0FEE */
