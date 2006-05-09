
ZEND_BEGIN_MODULE_GLOBALS(xcache)
	zend_bool cacher;      /* true if enabled */
#ifdef HAVE_XCACHE_OPTIMIZER
	zend_bool optimizer;   /* true if enabled */
#endif
#ifdef HAVE_XCACHE_COVERAGE
	zend_bool coveragedumper;
	HashTable *coverages;  /* coverages[file][line] = times */
#endif
	xc_stack_t *php_holds;
	xc_stack_t *var_holds;
	time_t request_time;
ZEND_END_MODULE_GLOBALS(xcache)

ZEND_EXTERN_MODULE_GLOBALS(xcache)

#ifdef ZTS
# define XG(v) TSRMG(xcache_globals_id, zend_xcache_globals *, v)
#else
# define XG(v) (xcache_globals.v)
#endif
