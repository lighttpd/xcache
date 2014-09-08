#include "xc_disassembler.h"
#include "xcache.h"
#include "xcache/xc_ini.h"
#include "xcache/xc_utils.h"
#include "xcache/xc_sandbox.h"
#include "xcache/xc_compatibility.h"
#include "xc_processor.h"

#include "ext/standard/info.h"

static void xc_dasm(zval *output, zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	const Bucket *b;
	zval *zv, *list;
	xc_compile_result_t cr;
	int bufsize = 2;
	char *buf;
	xc_dasm_t dasm;

	xc_compile_result_init_cur(&cr, op_array TSRMLS_CC);

	xc_apply_op_array(&cr, (apply_func_t) xc_undo_pass_two TSRMLS_CC);
	xc_apply_op_array(&cr, (apply_func_t) xc_fix_opcode TSRMLS_CC);

	/* go */
	array_init(output);

	ALLOC_INIT_ZVAL(zv);
	array_init(zv);
	xc_dasm_zend_op_array(&dasm, zv, op_array TSRMLS_CC);
	add_assoc_zval_ex(output, XCACHE_STRS("op_array"), zv);

	buf = emalloc(bufsize);

	ALLOC_INIT_ZVAL(list);
	array_init(list);
	for (b = xc_sandbox_user_function_begin(TSRMLS_C); b; b = b->pListNext) {
		int keysize, keyLength;

		ALLOC_INIT_ZVAL(zv);
		array_init(zv);
		xc_dasm_zend_function(&dasm, zv, b->pData TSRMLS_CC);

		keysize = BUCKET_KEY_SIZE(b) + 2;
		if (keysize > bufsize) {
			do {
				bufsize *= 2;
			} while (keysize > bufsize);
			buf = erealloc(buf, bufsize);
		}
		memcpy(buf, BUCKET_KEY_S(b), keysize);
		buf[keysize - 2] = buf[keysize - 1] = ""[0];
		keyLength = b->nKeyLength;
#ifdef IS_UNICODE
		if (BUCKET_KEY_TYPE(b) == IS_UNICODE) {
			if (buf[0] == ""[0] && buf[1] == ""[0]) {
				keyLength ++;
			}
		} else
#endif
		{
			if (buf[0] == ""[0]) {
				keyLength ++;
			}
		}

		add_u_assoc_zval_ex(list, BUCKET_KEY_TYPE(b), ZSTR(buf), keyLength, zv);
	}
	add_assoc_zval_ex(output, XCACHE_STRS("function_table"), list);
	
	ALLOC_INIT_ZVAL(list);
	array_init(list);
	for (b = xc_sandbox_user_class_begin(TSRMLS_C); b; b = b->pListNext) {
		int keysize, keyLength;

		ALLOC_INIT_ZVAL(zv);
		array_init(zv);
		xc_dasm_zend_class_entry(&dasm, zv, CestToCePtr(*(xc_cest_t *)b->pData) TSRMLS_CC);

		keysize = BUCKET_KEY_SIZE(b) + 2;
		if (keysize > bufsize) {
			do {
				bufsize *= 2;
			} while (keysize > bufsize);
			buf = erealloc(buf, bufsize);
		}
		memcpy(buf, BUCKET_KEY_S(b), keysize);
		buf[keysize - 2] = buf[keysize - 1] = ""[0];
		keyLength = b->nKeyLength;
#ifdef IS_UNICODE
		if (BUCKET_KEY_TYPE(b) == IS_UNICODE) {
			if (buf[0] == ""[0] && buf[1] == ""[0]) {
				keyLength ++;
			}
		} else
#endif
		{
			if (buf[0] == ""[0]) {
				keyLength ++;
			}
		}
		add_u_assoc_zval_ex(list, BUCKET_KEY_TYPE(b), ZSTR(buf), keyLength, zv);
	}
	efree(buf);
	add_assoc_zval_ex(output, XCACHE_STRS("class_table"), list);

	/*xc_apply_op_array(&cr, (apply_func_t) xc_redo_pass_two TSRMLS_CC);*/
	xc_compile_result_free(&cr);
}
/* }}} */
typedef struct xc_dasm_sandboxed_t { /* {{{ */
	enum Type {
		xc_dasm_file_t
		, xc_dasm_string_t
	} type;
	union {
		zval *zfilename;
		struct {
			zval *source;
			char *eval_name;
		} compile_string;
	} input;

	zval *output;
} xc_dasm_sandboxed_t; /* }}} */
zend_op_array *xc_dasm_sandboxed(void *data TSRMLS_DC) /* {{{ */
{
	zend_bool catched = 0;
	zend_op_array *op_array = NULL;
	xc_dasm_sandboxed_t *sandboxed_dasm = (xc_dasm_sandboxed_t *) data;

	zend_try {
		if (sandboxed_dasm->type == xc_dasm_file_t) {
			op_array = compile_filename(ZEND_REQUIRE, sandboxed_dasm->input.zfilename TSRMLS_CC);
		}
		else {
			op_array = compile_string(sandboxed_dasm->input.compile_string.source, sandboxed_dasm->input.compile_string.eval_name TSRMLS_CC);
		}
	} zend_catch {
		catched = 1;
	} zend_end_try();

	if (catched || !op_array) {
#define return_value sandboxed_dasm->output
		RETVAL_FALSE;
#undef return_value
		return NULL;
	}

	xc_dasm(sandboxed_dasm->output, op_array TSRMLS_CC);

	/* free */
#ifdef ZEND_ENGINE_2
	destroy_op_array(op_array TSRMLS_CC);
#else
	destroy_op_array(op_array);
#endif
	efree(op_array);

	return NULL;
} /* }}} */
void xc_dasm_string(zval *output, zval *source TSRMLS_DC) /* {{{ */
{
	xc_dasm_sandboxed_t sandboxed_dasm;
	char *eval_name = zend_make_compiled_string_description("runtime-created function" TSRMLS_CC);

	sandboxed_dasm.output = output;
	sandboxed_dasm.type = xc_dasm_string_t;
	sandboxed_dasm.input.compile_string.source = source;
	sandboxed_dasm.input.compile_string.eval_name = eval_name;
	xc_sandbox(&xc_dasm_sandboxed, (void *) &sandboxed_dasm, eval_name TSRMLS_CC);
	efree(eval_name);
}
/* }}} */
void xc_dasm_file(zval *output, const char *filename TSRMLS_DC) /* {{{ */
{
	zval *zfilename;
	xc_dasm_sandboxed_t sandboxed_dasm;

	MAKE_STD_ZVAL(zfilename);
	zfilename->value.str.val = estrdup(filename);
	zfilename->value.str.len = strlen(filename);
	zfilename->type = IS_STRING;

	sandboxed_dasm.output = output;
	sandboxed_dasm.type = xc_dasm_file_t;
	sandboxed_dasm.input.zfilename = zfilename;
	xc_sandbox(&xc_dasm_sandboxed, (void *) &sandboxed_dasm, zfilename->value.str.val TSRMLS_CC);

	zval_dtor(zfilename);
	FREE_ZVAL(zfilename);
}
/* }}} */

/* {{{ proto array xcache_dasm_file(string filename)
   Disassemble file into opcode array by filename */
PHP_FUNCTION(xcache_dasm_file)
{
	char *filename;
	int filename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, &filename_len) == FAILURE) {
		return;
	}
	if (!filename_len) RETURN_FALSE;

	xc_dasm_file(return_value, filename TSRMLS_CC);
}
/* }}} */
/* {{{ proto array xcache_dasm_string(string code)
   Disassemble php code into opcode array */
PHP_FUNCTION(xcache_dasm_string)
{
	zval *code;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &code) == FAILURE) {
		return;
	}
	xc_dasm_string(return_value, code TSRMLS_CC);
}
/* }}} */

#ifdef IS_CONSTANT_AST
/* {{{ proto array xcache_dasm_ast(mixed ast)
   Disassemble zend_ast data into array */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_dasm_ast, 0, 0, 1)
	ZEND_ARG_INFO(0, ast)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_dasm_ast[] = { 1, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_dasm_ast)
{
	zval *ast;
	xc_dasm_t dasm;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &ast) == FAILURE) {
		return;
	}
	if ((Z_TYPE_P(ast) & IS_CONSTANT_TYPE_MASK) != IS_CONSTANT_AST) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Data type is not zend_ast");
		return;
	}
	array_init(return_value);
	xc_dasm_zend_ast(&dasm, return_value, ast->value.ast TSRMLS_CC);
}
/* }}} */
#endif

/* {{{ PHP_MINFO_FUNCTION(xcache_disassembler) */
static PHP_MINFO_FUNCTION(xcache_disassembler)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "XCache Disassembler Module", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */
static zend_function_entry xcache_disassembler_functions[] = /* {{{ */
{
	PHP_FE(xcache_dasm_file,         NULL)
	PHP_FE(xcache_dasm_string,       NULL)
#ifdef IS_CONSTANT_AST
	PHP_FE(xcache_dasm_ast,          arginfo_xcache_dasm_ast)
#endif
	PHP_FE_END
};
/* }}} */
static zend_module_entry xcache_disassembler_module_entry = { /* {{{ */
	STANDARD_MODULE_HEADER,
	XCACHE_NAME " Disassembler",
	xcache_disassembler_functions,
	NULL,
	NULL,
	NULL,
	NULL,
	PHP_MINFO(xcache_disassembler),
	XCACHE_VERSION,
#ifdef PHP_GINIT
	NO_MODULE_GLOBALS,
#endif
#ifdef ZEND_ENGINE_2
	NULL,
#else
	NULL,
	NULL,
#endif
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */
int xc_disassembler_startup_module() /* {{{ */
{
	return zend_startup_module(&xcache_disassembler_module_entry);
}
/* }}} */
