#include "php.h"
#include "xcache.h"
#include "utils.h"
#ifdef ZEND_ENGINE_2_1
#include "zend_vm.h"
#endif
#include "opcode_spec.h"
#undef NDEBUG
#include "assert.h"

xc_compile_result_t *xc_compile_result_init(xc_compile_result_t *cr, /* {{{ */
		zend_op_array *op_array,
		HashTable *function_table,
		HashTable *class_table)
{
	if (cr) {
		cr->alloc = 0;
	}
	else {
		cr = emalloc(sizeof(xc_compile_result_t));
		cr->alloc = 1;
	}
	cr->op_array       = op_array;
	cr->function_table = function_table;
	cr->class_table    = class_table;
	return cr;
}
/* }}} */
xc_compile_result_t *xc_compile_result_init_cur(xc_compile_result_t *cr, zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	return xc_compile_result_init(cr, op_array, CG(function_table), CG(class_table));
}
/* }}} */
void xc_compile_result_free(xc_compile_result_t *cr) /* {{{ */
{
	if (cr->alloc) {
		efree(cr);
	}
}
/* }}} */

int xc_apply_function(zend_function *zf, apply_func_t applyer TSRMLS_DC) /* {{{ */
{
	switch (zf->type) {
	case ZEND_USER_FUNCTION:
	case ZEND_EVAL_CODE:
		return applyer(&zf->op_array TSRMLS_CC);
		break;

	case ZEND_INTERNAL_FUNCTION:
	case ZEND_OVERLOADED_FUNCTION:
		break;

	EMPTY_SWITCH_DEFAULT_CASE();
	}
	return 0;
}
/* }}} */
typedef struct {
	apply_func_t applyer;
	zend_class_entry *ce;
} xc_apply_method_info;
int xc_apply_method(zend_function *zf, xc_apply_method_info *mi TSRMLS_DC) /* {{{ */
{
	/* avoid duplicate apply for shadowed method */
#ifdef ZEND_ENGINE_2
	if (mi->ce != zf->common.scope) {
		/* fprintf(stderr, "avoided duplicate %s\n", zf->common.function_name); */
		return 0;
	}
#else
	char *name = zf->common.function_name;
	int name_s = strlen(name) + 1;
	zend_class_entry *ce;
	zend_function *ptr;

	for (ce = mi->ce->parent; ce; ce = ce->parent) {
		if (zend_hash_find(&ce->function_table, name, name_s, (void **) &ptr) == SUCCESS) {
			if (ptr->op_array.refcount == zf->op_array.refcount) {
				return 0;
			}
		}
	}
#endif
	return xc_apply_function(zf, mi->applyer TSRMLS_CC);
}
/* }}} */
#if 0
int xc_apply_class(zend_class_entry *ce, apply_func_t applyer TSRMLS_DC) /* {{{ */
{
	xc_apply_method_info mi;

	mi.applyer = applyer;
	mi.ce      = ce;
	zend_hash_apply_with_argument(&(ce->function_table), (apply_func_arg_t) xc_apply_method, &mi TSRMLS_CC);
	return 0;
}
/* }}} */
#endif
static int xc_apply_cest(xc_cest_t *cest, apply_func_t applyer TSRMLS_DC) /* {{{ */
{
	xc_apply_method_info mi;

	mi.applyer = applyer;
	mi.ce      = CestToCePtr(*cest);
	zend_hash_apply_with_argument(&(CestToCePtr(*cest)->function_table), (apply_func_arg_t) xc_apply_method, &mi TSRMLS_CC);
	return 0;
}
/* }}} */
int xc_apply_op_array(xc_compile_result_t *cr, apply_func_t applyer TSRMLS_DC) /* {{{ */
{
	zend_hash_apply_with_argument(cr->function_table, (apply_func_arg_t) xc_apply_function, applyer TSRMLS_CC);
	zend_hash_apply_with_argument(cr->class_table, (apply_func_arg_t) xc_apply_cest, applyer TSRMLS_CC);

	return applyer(cr->op_array TSRMLS_CC);
}
/* }}} */

int xc_undo_pass_two(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	zend_op *opline, *end;

	if (!op_array->done_pass_two) {
		return 0;
	}

	opline = op_array->opcodes;
	end = opline + op_array->last;
	while (opline < end) {
#ifdef ZEND_ENGINE_2_1
		switch (opline->opcode) {
			case ZEND_JMP:
				opline->op1.u.opline_num = opline->op1.u.jmp_addr - op_array->opcodes;
				assert(opline->op1.u.opline_num < op_array->last);
				break;
			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
				opline->op2.u.opline_num = opline->op2.u.jmp_addr - op_array->opcodes;
				assert(opline->op2.u.opline_num < op_array->last);
				break;
		}
#endif
		opline++;
	}
	op_array->done_pass_two = 0;

	return 0;
}
/* }}} */
int xc_redo_pass_two(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	zend_op *opline, *end;

	if (op_array->done_pass_two) {
		return 0;
	}

	/*
	op_array->opcodes = (zend_op *) erealloc(op_array->opcodes, sizeof(zend_op)*op_array->last);
	op_array->size = op_array->last;
	*/

	opline = op_array->opcodes;
	end = opline + op_array->last;
	while (opline < end) {
		if (opline->op1.op_type == IS_CONST) {
			opline->op1.u.constant.is_ref = 1;
			opline->op1.u.constant.refcount = 2; /* Make sure is_ref won't be reset */
		}
		if (opline->op2.op_type == IS_CONST) {
			opline->op2.u.constant.is_ref = 1;
			opline->op2.u.constant.refcount = 2;
		}
#ifdef ZEND_ENGINE_2_1
		switch (opline->opcode) {
			case ZEND_JMP:
				assert(opline->op1.u.opline_num < op_array->last);
				opline->op1.u.jmp_addr = op_array->opcodes + opline->op1.u.opline_num;
				break;
			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
				assert(opline->op2.u.opline_num < op_array->last);
				opline->op2.u.jmp_addr = op_array->opcodes + opline->op2.u.opline_num;
				break;
		}
		ZEND_VM_SET_OPCODE_HANDLER(opline);
#endif
		opline++;
	}

	op_array->done_pass_two = 1;
	return 0;
}
/* }}} */

#ifdef HAVE_XCACHE_OPCODE_SPEC_DEF
static void xc_fix_opcode_ex_znode(int tofix, xc_op_spec_t spec, znode *znode, int type TSRMLS_DC) /* {{{ */
{
#ifdef ZEND_ENGINE_2
	if ((znode->op_type != IS_UNUSED && (spec == OPSPEC_UCLASS || spec == OPSPEC_CLASS)) ||
			spec == OPSPEC_FETCH) {
		if (tofix) {
			switch (znode->op_type) {
			case IS_VAR:
			case IS_TMP_VAR:
				break;

			default:
				/* TODO: data lost, find a way to keep it */
				/* assert(znode->op_type == IS_CONST); */
				znode->op_type = IS_TMP_VAR;
			}
		}
	}
	switch (znode->op_type) {
	case IS_TMP_VAR:
	case IS_VAR:
		if (tofix) {
			znode->u.var /= sizeof(temp_variable);
		}
		else {
			znode->u.var *= sizeof(temp_variable);
		}
	}
#endif
}
/* }}} */

static void xc_fix_opcode_ex(zend_op_array *op_array, int tofix TSRMLS_DC) /* {{{ */
{
	zend_op *opline;
	zend_uint i;

	opline = op_array->opcodes;
	for (i = 0; i < op_array->last; i ++, opline ++) {
		/* 3rd optimizer may have ... */
		if (opline->opcode < xc_get_opcode_spec_count()) {
			const xc_opcode_spec_t *spec;
			spec = xc_get_opcode_spec(opline->opcode);

			xc_fix_opcode_ex_znode(tofix, spec->op1, &opline->op1, 0 TSRMLS_CC);
			xc_fix_opcode_ex_znode(tofix, spec->op2, &opline->op2, 1 TSRMLS_CC);
			xc_fix_opcode_ex_znode(tofix, spec->res, &opline->result, 2 TSRMLS_CC);
		}
	}
}
/* }}} */
int xc_fix_opcode(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	xc_fix_opcode_ex(op_array, 1 TSRMLS_CC);
	return 0;
}
/* }}} */
int xc_undo_fix_opcode(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	xc_fix_opcode_ex(op_array, 0 TSRMLS_CC);

	return 0;
}
/* }}} */
#endif

#ifdef HAVE_XCACHE_CONSTANT
void xc_install_constant(char *filename, zend_constant *constant, zend_uchar type, zstr key, uint len TSRMLS_DC) /* {{{ */
{
	if (zend_u_hash_add(EG(zend_constants), type, key, len,
				constant, sizeof(zend_constant),
				NULL
				) == FAILURE) {
		CG(in_compilation) = 1;
		CG(compiled_filename) = filename;
		CG(zend_lineno) = 0;
#ifdef IS_UNICODE
		zend_error(E_NOTICE, "Constant %R already defined", type, key);
#else
		zend_error(E_NOTICE, "Constant %s already defined", key);
#endif
		free(ZSTR_V(constant->name));
		if (!(constant->flags & CONST_PERSISTENT)) {
			zval_dtor(&constant->value);
		}
	}
}
/* }}} */
#endif
void xc_install_function(char *filename, zend_function *func, zend_uchar type, zstr key, uint len TSRMLS_DC) /* {{{ */
{
	zend_bool istmpkey;

	if (func->type == ZEND_USER_FUNCTION) {
#ifdef IS_UNICODE
		istmpkey = (type == IS_STRING && ZSTR_S(key)[0] == 0) || ZSTR_U(key)[0] == 0;
#else
		istmpkey = ZSTR_S(key)[0] == 0;
#endif
		if (istmpkey) {
			zend_u_hash_update(CG(function_table), type, key, len,
						func, sizeof(zend_op_array),
						NULL
						);
		}
		else if (zend_u_hash_add(CG(function_table), type, key, len,
					func, sizeof(zend_op_array),
					NULL
					) == FAILURE) {
			CG(in_compilation) = 1;
			CG(compiled_filename) = filename;
			CG(zend_lineno) = ZESW(func->op_array.opcodes[0].lineno, func->op_array.line_start);
#ifdef IS_UNICODE
			zend_error(E_ERROR, "Cannot redeclare %R()", type, key);
#else
			zend_error(E_ERROR, "Cannot redeclare %s()", key);
#endif
		}
	}
}
/* }}} */
ZESW(xc_cest_t *, void) xc_install_class(char *filename, xc_cest_t *cest, zend_uchar type, zstr key, uint len TSRMLS_DC) /* {{{ */
{
	zend_bool istmpkey;
	zend_class_entry *cep = CestToCePtr(*cest);
	ZESW(void *stored_ce_ptr, NOTHING);

#ifdef IS_UNICODE
	istmpkey = (type == IS_STRING && ZSTR_S(key)[0] == 0) || ZSTR_U(key)[0] == 0;
#else
	istmpkey = ZSTR_S(key)[0] == 0;
#endif
	if (istmpkey) {
		zend_u_hash_update(CG(class_table), type, key, len,
					cest, sizeof(xc_cest_t),
					ZESW(&stored_ce_ptr, NULL)
					);
	}
	else if (zend_u_hash_add(CG(class_table), type, key, len,
				cest, sizeof(xc_cest_t),
				ZESW(&stored_ce_ptr, NULL)
				) == FAILURE) {
		CG(in_compilation) = 1;
		CG(compiled_filename) = filename;
		CG(zend_lineno) = ZESW(0, cep->line_start);
#ifdef IS_UNICODE
		zend_error(E_ERROR, "Cannot redeclare class %R", type, cep->name);
#else
		zend_error(E_ERROR, "Cannot redeclare class %s", cep->name);
#endif
	}
	ZESW(return (xc_cest_t *) stored_ce_ptr, NOTHING);
}
/* }}} */

/* sandbox {{{ */
#undef TG
#undef OG
#define TG(x) (sandbox->tmp_##x)
#define OG(x) (sandbox->orig_##x)
/* }}} */
xc_sandbox_t *xc_sandbox_init(xc_sandbox_t *sandbox, char *filename TSRMLS_DC) /* {{{ */
{
	if (sandbox) {
		memset(sandbox, 0, sizeof(sandbox[0]));
	}
	else {
		ECALLOC_ONE(sandbox);
		sandbox->alloc = 1;
	}

	memcpy(&OG(included_files), &EG(included_files), sizeof(EG(included_files)));

#ifdef HAVE_XCACHE_CONSTANT
	OG(zend_constants) = EG(zend_constants);
	EG(zend_constants) = &TG(zend_constants);
#endif

	OG(function_table) = CG(function_table);
	CG(function_table) = &TG(function_table);

	assert(EG(class_table) == CG(class_table));

	OG(class_table) = CG(class_table);
	CG(class_table) = &TG(class_table);
	EG(class_table) = CG(class_table);

	TG(included_files) = &EG(included_files);

	zend_hash_init_ex(TG(included_files), 5, NULL, NULL, 0, 1);
#ifdef HAVE_XCACHE_CONSTANT
	zend_hash_init_ex(&TG(zend_constants), 20, NULL, EG(zend_constants)->pDestructor, 1, 0);
#endif
	zend_hash_init_ex(&TG(function_table), 128, NULL, CG(function_table)->pDestructor, 0, 0);
	zend_hash_init_ex(&TG(class_table), 16, NULL, CG(class_table)->pDestructor, 0, 0);

	sandbox->filename = filename;

	return sandbox;
}
/* }}} */
static void xc_sandbox_install(xc_sandbox_t *sandbox TSRMLS_DC) /* {{{ */
{
	int i;
	Bucket *b;

#ifdef HAVE_XCACHE_CONSTANT
	b = TG(zend_constants).pListHead;
	/* install constants */
	while (b != NULL) {
		zend_constant *c = (zend_constant*) b->pData;
		xc_install_constant(sandbox->filename, c,
				BUCKET_KEY_TYPE(b), ZSTR(BUCKET_KEY(b)), b->nKeyLength TSRMLS_CC);
		b = b->pListNext;
	}
#endif

	b = TG(function_table).pListHead;
	/* install function */
	while (b != NULL) {
		zend_function *func = (zend_function*) b->pData;
		xc_install_function(sandbox->filename, func,
				BUCKET_KEY_TYPE(b), ZSTR(BUCKET_KEY(b)), b->nKeyLength TSRMLS_CC);
		b = b->pListNext;
	}

	b = TG(class_table).pListHead;
	/* install class */
	while (b != NULL) {
		xc_install_class(sandbox->filename, (xc_cest_t*)b->pData,
				BUCKET_KEY_TYPE(b), ZSTR(BUCKET_KEY(b)), b->nKeyLength TSRMLS_CC);
		b = b->pListNext;
	}

	i = 1;
	zend_hash_add(&OG(included_files), sandbox->filename, strlen(sandbox->filename) + 1, (void *)&i, sizeof(int), NULL);
}
/* }}} */
void xc_sandbox_free(xc_sandbox_t *sandbox, int install TSRMLS_DC) /* {{{ */
{
	/* restore first first install function/class */
#ifdef HAVE_XCACHE_CONSTANT
	EG(zend_constants) = OG(zend_constants);
#endif
	CG(function_table) = OG(function_table);
	CG(class_table)    = OG(class_table);
	EG(class_table)    = CG(class_table);

	if (install) {
		xc_sandbox_install(sandbox TSRMLS_CC);

		/* no free as it's installed */
#ifdef HAVE_XCACHE_CONSTANT
		TG(zend_constants).pDestructor = NULL;
#endif
		TG(function_table).pDestructor = NULL;
		TG(class_table).pDestructor = NULL;
	}

	/* destroy all the tmp */
#ifdef HAVE_XCACHE_CONSTANT
	zend_hash_destroy(&TG(zend_constants));
#endif
	zend_hash_destroy(&TG(function_table));
	zend_hash_destroy(&TG(class_table));
	zend_hash_destroy(TG(included_files));

	/* restore orig here, as EG/CG holded tmp before */
	memcpy(&EG(included_files), &OG(included_files), sizeof(EG(included_files)));

	if (sandbox->alloc) {
		efree(sandbox);
	}
}
/* }}} */
