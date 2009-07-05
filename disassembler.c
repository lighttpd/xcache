#include "disassembler.h"
#include "xcache.h"
#include "utils.h"
#include "processor.h"

#define return_value dst

#ifndef HAVE_XCACHE_OPCODE_SPEC_DEF
#error disassembler cannot be built without xcache/opcode_spec_def.h
#endif
static void xc_dasm(zval *dst, zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	Bucket *b;
	zval *zv, *list;
	xc_compile_result_t cr;
	int bufsize = 2;
	char *buf;
	int keysize;

	xc_compile_result_init_cur(&cr, op_array TSRMLS_CC);

	xc_apply_op_array(&cr, (apply_func_t) xc_undo_pass_two TSRMLS_CC);
	xc_apply_op_array(&cr, (apply_func_t) xc_fix_opcode TSRMLS_CC);

	/* go */
	array_init(dst);

	ALLOC_INIT_ZVAL(zv);
	array_init(zv);
	xc_dasm_zend_op_array(zv, op_array TSRMLS_CC);
	add_assoc_zval_ex(dst, ZEND_STRS("op_array"), zv);

	ALLOC_INIT_ZVAL(list);
	array_init(list);
	xc_dasm_HashTable_zend_function(list, CG(function_table) TSRMLS_CC);
	add_assoc_zval_ex(dst, ZEND_STRS("function_table"), list);
	
	buf = emalloc(bufsize);
	ALLOC_INIT_ZVAL(list);
	array_init(list);
	for (b = CG(class_table)->pListHead; b; b = b->pListNext) {
		ALLOC_INIT_ZVAL(zv);
		array_init(zv);
		xc_dasm_zend_class_entry(zv, CestToCePtr(*(xc_cest_t *)b->pData) TSRMLS_CC);

		keysize = BUCKET_KEY_SIZE(b) + 2;
		if (keysize > bufsize) {
			do {
				bufsize *= 2;
			} while (keysize > bufsize);
			buf = erealloc(buf, bufsize);
		}
		memcpy(buf, BUCKET_KEY_S(b), keysize);
		buf[keysize - 2] = buf[keysize - 1] = ""[0];
		keysize = b->nKeyLength;
#ifdef IS_UNICODE
		if (BUCKET_KEY_TYPE(b) == IS_UNICODE) {
			if (buf[0] == ""[0] && buf[1] == ""[0]) {
				keysize ++;
			}
		} else
#endif
		{
			if (buf[0] == ""[0]) {
				keysize ++;
			}
		}
		add_u_assoc_zval_ex(list, BUCKET_KEY_TYPE(b), ZSTR(buf), b->nKeyLength, zv);
	}
	efree(buf);
	add_assoc_zval_ex(dst, ZEND_STRS("class_table"), list);

	/*xc_apply_op_array(&cr, (apply_func_t) xc_redo_pass_two TSRMLS_CC);*/
	xc_compile_result_free(&cr);

	return;
}
/* }}} */
void xc_dasm_string(zval *dst, zval *source TSRMLS_DC) /* {{{ */
{
	int catched;
	zend_op_array *op_array = NULL;
	xc_sandbox_t sandbox;
	char *eval_name = zend_make_compiled_string_description("runtime-created function" TSRMLS_CC);

	xc_sandbox_init(&sandbox, eval_name TSRMLS_CC);

	catched = 0;
	zend_try {
		op_array = compile_string(source, eval_name TSRMLS_CC);
	} zend_catch {
		catched = 1;
	} zend_end_try();

	if (catched || !op_array) {
		goto err_compile;
	}

	xc_dasm(dst, op_array TSRMLS_CC);

	/* free */
	efree(eval_name);
#ifdef ZEND_ENGINE_2
	destroy_op_array(op_array TSRMLS_CC);
#else
	destroy_op_array(op_array);
#endif
	efree(op_array);
	xc_sandbox_free(&sandbox, 0 TSRMLS_CC);
	return;

err_compile:
	efree(eval_name);
	xc_sandbox_free(&sandbox, 0 TSRMLS_CC);

	RETURN_FALSE;
}
/* }}} */
void xc_dasm_file(zval *dst, const char *filename TSRMLS_DC) /* {{{ */
{
	int catched;
	zend_op_array *op_array = NULL;
	xc_sandbox_t sandbox;
	zval *zfilename;

	MAKE_STD_ZVAL(zfilename);
	zfilename->value.str.val = estrdup(filename);
	zfilename->value.str.len = strlen(filename);
	zfilename->type = IS_STRING;

	xc_sandbox_init(&sandbox, zfilename->value.str.val TSRMLS_CC);

	catched = 0;
	zend_try {
		op_array = compile_filename(ZEND_REQUIRE, zfilename TSRMLS_CC);
	} zend_catch {
		catched = 1;
	} zend_end_try();

	if (catched || !op_array) {
		goto err_compile;
	}

	xc_dasm(dst, op_array TSRMLS_CC);

	/* free */
#ifdef ZEND_ENGINE_2
	destroy_op_array(op_array TSRMLS_CC);
#else
	destroy_op_array(op_array);
#endif
	efree(op_array);
	xc_sandbox_free(&sandbox, 0 TSRMLS_CC);
	zval_dtor(zfilename);
	FREE_ZVAL(zfilename);
	return;

err_compile:
	xc_sandbox_free(&sandbox, 0 TSRMLS_CC);

	zval_dtor(zfilename);
	FREE_ZVAL(zfilename);
	RETURN_FALSE;
}
/* }}} */
