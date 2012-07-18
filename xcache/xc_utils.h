#include "php.h"
#include "xcache.h"

int xc_util_init(int module_number TSRMLS_DC);
void xc_util_destroy();

typedef struct {
	zend_op_array *op_array;
	HashTable *function_table;
	HashTable *class_table;
} xc_compile_result_t;

xc_compile_result_t *xc_compile_result_init(xc_compile_result_t *cr,
		zend_op_array *op_array,
		HashTable *function_table,
		HashTable *class_table);
void xc_compile_result_free(xc_compile_result_t *cr);
xc_compile_result_t *xc_compile_result_init_cur(xc_compile_result_t *cr, zend_op_array *op_array TSRMLS_DC);
/* apply func */
int xc_apply_function(zend_function *zf, apply_func_t applyer TSRMLS_DC);
int xc_apply_class(zend_class_entry *ce, apply_func_t applyer TSRMLS_DC);
int xc_apply_op_array(xc_compile_result_t *cr, apply_func_t applyer TSRMLS_DC);

int xc_undo_pass_two(zend_op_array *op_array TSRMLS_DC);
int xc_redo_pass_two(zend_op_array *op_array TSRMLS_DC);
int xc_fix_opcode(zend_op_array *op_array TSRMLS_DC);
int xc_undo_fix_opcode(zend_op_array *op_array TSRMLS_DC);
zend_uchar xc_get_fixed_opcode(zend_uchar opcode, int line);

int xc_foreach_early_binding_class(zend_op_array *op_array, void (*callback)(zend_op *opline, int oplineno, void *data TSRMLS_DC), void *data TSRMLS_DC);

/* installer */
#ifdef HAVE_XCACHE_CONSTANT
void xc_install_constant(ZEND_24(NOTHING, const) char *filename, zend_constant *constant, zend_uchar type, const24_zstr key, uint len, ulong h TSRMLS_DC);
#endif
void xc_install_function(ZEND_24(NOTHING, const) char *filename, zend_function *func, zend_uchar type, const24_zstr key, uint len, ulong h TSRMLS_DC);
ZESW(xc_cest_t *, void) xc_install_class(ZEND_24(NOTHING, const) char *filename, xc_cest_t *cest, int oplineno, zend_uchar type, const24_zstr key, uint len, ulong h TSRMLS_DC);

#if defined(E_STRICT) || defined(E_DEPRECATED)
#define XCACHE_ERROR_CACHING
#endif

void xc_zend_class_add_ref(zend_class_entry ZESW(*ce, **ce));

typedef zend_bool (*xc_if_func_t)(void *data);

void xc_hash_copy_if(HashTable *target, HashTable *source, copy_ctor_func_t pCopyConstructor, void *tmp, uint size, xc_if_func_t checker);
#ifdef HAVE_XCACHE_CONSTANT
void xc_zend_constant_ctor(zend_constant *c);
void xc_zend_constant_dtor(zend_constant *c);
void xc_copy_internal_zend_constants(HashTable *target, HashTable *source);
#endif

#ifndef ZEND_ENGINE_2_3
size_t xc_dirname(char *path, size_t len);
#define zend_dirname xc_dirname
long xc_atol(const char *str, int len);
#define zend_atol xc_atol
#endif
