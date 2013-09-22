#include "util/xc_stack.h"

ZEND_BEGIN_MODULE_GLOBALS(xcache)
	zend_bool initial_compile_file_called; /* true if origin_compile_file is called */
	zend_bool cacher;      /* true if enabled */
	zend_bool stat;
	zend_bool experimental;
#ifdef HAVE_XCACHE_OPTIMIZER
	zend_bool optimizer;   /* true if enabled */
#endif
#ifdef HAVE_XCACHE_COVERAGER
	zend_bool coverager;
	zend_bool coverager_autostart;
	zend_bool coverager_started;
	HashTable *coverages;  /* coverages[file][line] = times */
#endif
#ifndef ZEND_WIN32
	pid_t holds_pid;
#endif
	xc_stack_t *php_holds;
	zend_uint php_holds_size;
	xc_stack_t *var_holds;
	zend_uint var_holds_size;
	time_t request_time;
	long   var_ttl;
#ifdef IS_UNCODE
	zval uvar_namespace_hard;
	zval uvar_namespace_soft;
#endif
	zval var_namespace_hard;
	zval var_namespace_soft;

	zend_llist gc_op_arrays;
#ifdef ZEND_ACC_ALIAS
	zend_llist gc_class_entries;
#endif

#ifdef HAVE_XCACHE_CONSTANT
	HashTable internal_constant_table;
#endif
	HashTable internal_function_table;
	HashTable internal_class_table;
	zend_bool internal_table_copied;

	void *sandbox;
	zend_uint op_array_dummy_refcount_holder;
ZEND_END_MODULE_GLOBALS(xcache)

ZEND_EXTERN_MODULE_GLOBALS(xcache)

#ifdef ZTS
# define XG(v) TSRMG(xcache_globals_id, zend_xcache_globals *, v)
#else
# define XG(v) (xcache_globals.v)
#endif
