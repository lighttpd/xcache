#include "php.h"

typedef struct {
	int alloc;
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

/* installer */
#ifdef HAVE_XCACHE_CONSTANT
void xc_install_constant(char *filename, zend_constant *constant, zend_uchar type, zstr key, uint len TSRMLS_DC);
#endif
void xc_install_function(char *filename, zend_function *func, zend_uchar type, zstr key, uint len TSRMLS_DC);
ZESW(xc_cest_t *, void) xc_install_class(char *filename, xc_cest_t *cest, zend_uchar type, zstr key, uint len TSRMLS_DC);

/* sandbox */
typedef struct {
	int alloc;
	int orig_user_error_handler_error_reporting;
	char *filename;

	HashTable orig_included_files;
	HashTable *tmp_included_files;

#ifdef HAVE_XCACHE_CONSTANT
	HashTable *orig_zend_constants;
	HashTable tmp_zend_constants;
#endif
	HashTable *orig_function_table;
	HashTable *orig_class_table;
	HashTable tmp_function_table;
	HashTable tmp_class_table;
} xc_sandbox_t;

xc_sandbox_t *xc_sandbox_init(xc_sandbox_t *sandbox, char *filename TSRMLS_DC);
void xc_sandbox_free(xc_sandbox_t *sandbox, int install TSRMLS_DC);
