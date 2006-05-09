#include "optimizer.h"
#include "utils.h"

static int xc_optimize_op_array(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	if (op_array->type != ZEND_USER_FUNCTION) {
		return 0;
	}
	//xc_undo_pass_two(op_array TSRMLS_CC);
	//xc_redo_pass_two(op_array TSRMLS_CC);
	//xc_dprint_zend_op_array(op_array, 0);
	return 0;
}
/* }}} */
void xc_optimize(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	xc_compile_result_t cr;

	xc_compile_result_init_cur(&cr, op_array TSRMLS_CC);

	xc_apply_op_array(&cr, (apply_func_t) xc_undo_pass_two TSRMLS_CC);
	xc_apply_op_array(&cr, (apply_func_t) xc_optimize_op_array TSRMLS_CC);
	xc_apply_op_array(&cr, (apply_func_t) xc_redo_pass_two TSRMLS_CC);

	xc_compile_result_free(&cr);
}
/* }}} */
