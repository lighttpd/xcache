#ifndef __XCACHE_H
#define __XCACHE_H
#define XCACHE_NAME       "XCache"
#ifndef XCACHE_VERSION
#	define XCACHE_VERSION "3.2.0"
#endif
#define XCACHE_AUTHOR     "mOo"
#define XCACHE_COPYRIGHT  "Copyright (c) 2005-2014"
#define XCACHE_URL        "http://xcache.lighttpd.net"
#define XCACHE_WIKI_URL   XCACHE_URL "/wiki"

#include "php.h"

#if defined(E_STRICT) || defined(E_DEPRECATED)
#define XCACHE_ERROR_CACHING
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "xcache/xc_shm.h"
#include "xcache/xc_mutex.h"
#include "xcache/xc_compatibility.h"

extern zend_module_entry xcache_module_entry;
#define phpext_xcache_ptr &xcache_module_entry

extern zend_bool xc_test;

#endif /* __XCACHE_H */
