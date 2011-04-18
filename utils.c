
#include "xcache.h"
#include "stack.h"
#include "xcache_globals.h"
#include "utils.h"
#ifdef ZEND_ENGINE_2_1
#include "zend_vm.h"
#endif
#include "opcode_spec.h"
#undef NDEBUG
#include "assert.h"

#ifndef max
#define max(a, b) ((a) < (b) ? (b) : (a))
#endif

#ifndef ZEND_VM_SET_OPCODE_HANDLER
#	define ZEND_VM_SET_OPCODE_HANDLER(opline) do { } while (0)
#endif

#define OP_ZVAL_DTOR(op) do { \
	Z_UNSET_ISREF(Z_OP_CONSTANT(op)); \
	zval_dtor(&Z_OP_CONSTANT(op)); \
} while(0)
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
	zend_hash_apply_with_argument(cr->function_table, (apply_func_arg_t) xc_apply_function, (void *) applyer TSRMLS_CC);
	zend_hash_apply_with_argument(cr->class_table, (apply_func_arg_t) xc_apply_cest, (void *) applyer TSRMLS_CC);

	return applyer(cr->op_array TSRMLS_CC);
}
/* }}} */

int xc_undo_pass_two(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	zend_op *opline, *end;

#ifndef ZEND_ENGINE_2_4
	if (!op_array->done_pass_two) {
		return 0;
	}
#endif

	opline = op_array->opcodes;
	end = opline + op_array->last;
	while (opline < end) {
#ifdef ZEND_ENGINE_2_1
		switch (opline->opcode) {
#ifdef ZEND_GOTO
			case ZEND_GOTO:
#endif
			case ZEND_JMP:
				assert(Z_OP(opline->op1).jmp_addr - op_array->opcodes < op_array->last);
				Z_OP(opline->op1).opline_num = Z_OP(opline->op1).jmp_addr - op_array->opcodes;
				break;
			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_SET
			case ZEND_JMP_SET:
#endif
				assert(Z_OP(opline->op2).jmp_addr - op_array->opcodes < op_array->last);
				Z_OP(opline->op2).opline_num = Z_OP(opline->op2).jmp_addr - op_array->opcodes;
				break;
		}
#endif
		opline++;
	}
#ifndef ZEND_ENGINE_2_4
	op_array->done_pass_two = 0;
#endif

	return 0;
}
/* }}} */
int xc_redo_pass_two(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	zend_op *opline, *end;

#ifndef ZEND_ENGINE_2_4
	if (op_array->done_pass_two) {
		return 0;
	}
#endif

	/*
	op_array->opcodes = (zend_op *) erealloc(op_array->opcodes, sizeof(zend_op)*op_array->last);
	op_array->size = op_array->last;
	*/

	opline = op_array->opcodes;
	end = opline + op_array->last;
	while (opline < end) {
		if (Z_OP_TYPE(opline->op1) == IS_CONST) {
			Z_SET_ISREF(Z_OP_CONSTANT(opline->op1));
			Z_SET_REFCOUNT(Z_OP_CONSTANT(opline->op1), 2); /* Make sure is_ref won't be reset */

		}
		if (Z_OP_TYPE(opline->op2) == IS_CONST) {
			Z_SET_ISREF(Z_OP_CONSTANT(opline->op2));
			Z_SET_REFCOUNT(Z_OP_CONSTANT(opline->op2), 2);
		}
#ifdef ZEND_ENGINE_2_1
		switch (opline->opcode) {
#ifdef ZEND_GOTO
			case ZEND_GOTO:
#endif
			case ZEND_JMP:
				assert(Z_OP(opline->op1).opline_num < op_array->last);
				Z_OP(opline->op1).jmp_addr = op_array->opcodes + Z_OP(opline->op1).opline_num;
				break;
			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_SET
			case ZEND_JMP_SET:
#endif
				assert(Z_OP(opline->op2).opline_num < op_array->last);
				Z_OP(opline->op2).jmp_addr = op_array->opcodes + Z_OP(opline->op2).opline_num;
				break;
		}
		ZEND_VM_SET_OPCODE_HANDLER(opline);
#endif
		opline++;
	}

#ifndef ZEND_ENGINE_2_4
	op_array->done_pass_two = 1;
#endif
	return 0;
}
/* }}} */

#ifdef HAVE_XCACHE_OPCODE_SPEC_DEF
static void xc_fix_opcode_ex_znode(int tofix, xc_op_spec_t spec, Z_OP_TYPEOF_TYPE *op_type, znode_op *op, int type TSRMLS_DC) /* {{{ */
{
#ifdef ZEND_ENGINE_2
	if ((*op_type != IS_UNUSED && (spec == OPSPEC_UCLASS || spec == OPSPEC_CLASS)) ||
			spec == OPSPEC_FETCH) {
		if (tofix) {
			switch (*op_type) {
			case IS_VAR:
			case IS_TMP_VAR:
				break;

			case IS_CONST:
				if (spec == OPSPEC_UCLASS) {
					break;
				}
				/* fall */

			default:
				/* TODO: data lost, find a way to keep it */
				/* assert(*op_type == IS_CONST); */
				*op_type = IS_TMP_VAR;
			}
		}
	}
	switch (*op_type) {
	case IS_TMP_VAR:
	case IS_VAR:
		if (tofix) {
			Z_OP(*op).var /= sizeof(temp_variable);
		}
		else {
			Z_OP(*op).var *= sizeof(temp_variable);
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

			xc_fix_opcode_ex_znode(tofix, spec->op1, &Z_OP_TYPE(opline->op1),    &opline->op1, 0 TSRMLS_CC);
			xc_fix_opcode_ex_znode(tofix, spec->op2, &Z_OP_TYPE(opline->op2),    &opline->op2, 1 TSRMLS_CC);
			xc_fix_opcode_ex_znode(tofix, spec->res, &Z_OP_TYPE(opline->result), &opline->result, 2 TSRMLS_CC);
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

int xc_foreach_early_binding_class(zend_op_array *op_array, void (*callback)(zend_op *opline, int oplineno, void *data TSRMLS_DC), void *data TSRMLS_DC) /* {{{ */
{
	zend_op *opline, *begin, *end, *next = NULL;

	opline = begin = op_array->opcodes;
	end = opline + op_array->last;
	while (opline < end) {
		switch (opline->opcode) {
#ifdef ZEND_GOTO
			case ZEND_GOTO:
#endif
			case ZEND_JMP:
				next = begin + Z_OP(opline->op1).opline_num;
				break;

			case ZEND_JMPZNZ:
				next = begin + max(Z_OP(opline->op2).opline_num, opline->extended_value);
				break;

			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_SET
			case ZEND_JMP_SET:
#endif
				next = begin + Z_OP(opline->op2).opline_num;
				break;

			case ZEND_RETURN:
				opline = end;
				break;

#ifdef ZEND_ENGINE_2
			case ZEND_DECLARE_INHERITED_CLASS:
				callback(opline, opline - begin, data TSRMLS_CC);
				break;
#else
			case ZEND_DECLARE_FUNCTION_OR_CLASS:
				if (opline->extended_value == ZEND_DECLARE_INHERITED_CLASS) {
					callback(opline, opline - begin, data TSRMLS_CC);
				}
				break;
#endif
		}

		if (opline < next) {
			opline = next;
		}
		else {
			opline ++;
		}
	}
	return SUCCESS;
}
/* }}} */
#ifndef ZEND_COMPILE_DELAYED_BINDING
static int xc_do_early_binding(zend_op_array *op_array, HashTable *class_table, int oplineno TSRMLS_DC) /* {{{ */
{
	zend_op *opline;

	TRACE("binding %d", oplineno);
	assert(oplineno >= 0);

	/* do early binding */
	opline = &(op_array->opcodes[oplineno]);

	switch (opline->opcode) {
#ifdef ZEND_ENGINE_2
	case ZEND_DECLARE_INHERITED_CLASS:
		{
			zval *parent_name;
			zend_class_entry **pce;

			/* don't early-bind classes that implement interfaces */
			if ((opline + 1)->opcode == ZEND_FETCH_CLASS && (opline + 2)->opcode == ZEND_ADD_INTERFACE) {
				return FAILURE;
			}

			parent_name = &(Z_OP_CONSTANT((opline - 1)->op2));
			TRACE("binding with parent %s", Z_STRVAL_P(parent_name));
			if (zend_lookup_class(Z_STRVAL_P(parent_name), Z_STRLEN_P(parent_name), &pce TSRMLS_CC) == FAILURE) {
				return FAILURE;
			}

			if (do_bind_inherited_class(opline, class_table, *pce, 1 TSRMLS_CC) == NULL) {
				return FAILURE;
			}
		}

		/* clear unnecessary ZEND_FETCH_CLASS opcode */
		if (opline > op_array->opcodes
		 && (opline - 1)->opcode == ZEND_FETCH_CLASS) {
			zend_op *fetch_class_opline = opline - 1;

			TRACE("%s %p", Z_STRVAL(Z_OP_CONSTANT(fetch_class_opline->op2)), Z_STRVAL(Z_OP_CONSTANT(fetch_class_opline->op2)));
			OP_ZVAL_DTOR(fetch_class_opline->op2);
			fetch_class_opline->opcode = ZEND_NOP;
			ZEND_VM_SET_OPCODE_HANDLER(fetch_class_opline);
			memset(&fetch_class_opline->op1, 0, sizeof(znode));
			memset(&fetch_class_opline->op2, 0, sizeof(znode));
			SET_UNUSED(fetch_class_opline->op1);
			SET_UNUSED(fetch_class_opline->op2);
			SET_UNUSED(fetch_class_opline->result);
		}

		/* clear unnecessary ZEND_VERIFY_ABSTRACT_CLASS opcode */
		if ((opline + 1)->opcode == ZEND_VERIFY_ABSTRACT_CLASS) {
			zend_op *abstract_op = opline + 1;
			memset(abstract_op, 0, sizeof(abstract_op[0]));
			abstract_op->lineno = 0;
			SET_UNUSED(abstract_op->op1);
			SET_UNUSED(abstract_op->op2);
			SET_UNUSED(abstract_op->result);
			abstract_op->opcode = ZEND_NOP;
			ZEND_VM_SET_OPCODE_HANDLER(abstract_op);
		}
#else
	case ZEND_DECLARE_FUNCTION_OR_CLASS:
		if (do_bind_function_or_class(opline, NULL, class_table, 1) == FAILURE) {
			return FAILURE;
		}
#endif
		break;

	default:
		return FAILURE;
	}

	zend_hash_del(class_table, Z_OP_CONSTANT(opline->op1).value.str.val, Z_OP_CONSTANT(opline->op1).value.str.len);
	OP_ZVAL_DTOR(opline->op1);
	OP_ZVAL_DTOR(opline->op2);
	opline->opcode = ZEND_NOP;
	ZEND_VM_SET_OPCODE_HANDLER(opline);
	memset(&opline->op1, 0, sizeof(znode));
	memset(&opline->op2, 0, sizeof(znode));
	SET_UNUSED(opline->op1);
	SET_UNUSED(opline->op2);
	return SUCCESS;
}
/* }}} */
#endif

#ifdef HAVE_XCACHE_CONSTANT
void xc_install_constant(char *filename, zend_constant *constant, zend_uchar type, zstr key, uint len, ulong h TSRMLS_DC) /* {{{ */
{
	if (zend_u_hash_add(EG(zend_constants), type, key, len,
				constant, sizeof(zend_constant),
				NULL
				) == FAILURE) {
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
void xc_install_function(char *filename, zend_function *func, zend_uchar type, zstr key, uint len, ulong h TSRMLS_DC) /* {{{ */
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
ZESW(xc_cest_t *, void) xc_install_class(char *filename, xc_cest_t *cest, int oplineno, zend_uchar type, zstr key, uint len, ulong h TSRMLS_DC) /* {{{ */
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
		zend_u_hash_quick_update(CG(class_table), type, key, len, h,
					cest, sizeof(xc_cest_t),
					ZESW(&stored_ce_ptr, NULL)
					);
#ifndef ZEND_COMPILE_DELAYED_BINDING
		if (oplineno != -1) {
			xc_do_early_binding(CG(active_op_array), CG(class_table), oplineno TSRMLS_CC);
		}
#endif
	}
	else if (zend_u_hash_quick_add(CG(class_table), type, key, len, h,
				cest, sizeof(xc_cest_t),
				ZESW(&stored_ce_ptr, NULL)
				) == FAILURE) {
		CG(zend_lineno) = ZESW(0, Z_CLASS_INFO(*cep).line_start);
#ifdef IS_UNICODE
		zend_error(E_ERROR, "Cannot redeclare class %R", type, cep->name);
#else
		zend_error(E_ERROR, "Cannot redeclare class %s", cep->name);
#endif
		assert(oplineno == -1);
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
#ifdef XCACHE_ERROR_CACHING
static void xc_sandbox_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args) /* {{{ */
{
	xc_compilererror_t *compilererror;
	xc_sandbox_t *sandbox;
	TSRMLS_FETCH();

	sandbox = (xc_sandbox_t *) XG(sandbox);
	assert(sandbox != NULL);
	switch (type) {
#ifdef E_STRICT
	case E_STRICT:
#endif
#ifdef E_DEPRECATED
	case E_DEPRECATED:
#endif
		if (sandbox->compilererror_cnt <= sandbox->compilererror_size) {
			if (sandbox->compilererror_size) {
				sandbox->compilererror_size += 16;
				sandbox->compilererrors = erealloc(sandbox->compilererrors, sandbox->compilererror_size * sizeof(sandbox->compilererrors));
			}
			else {
				sandbox->compilererror_size = 16;
				sandbox->compilererrors = emalloc(sandbox->compilererror_size * sizeof(sandbox->compilererrors));
			}
		}
		compilererror = &sandbox->compilererrors[sandbox->compilererror_cnt++];
		compilererror->type = type;
		compilererror->lineno = error_lineno;
		compilererror->error_len = vspprintf(&compilererror->error, 0, format, args);
		break;

	default: {
		/* give up, and user handler is not supported in this case */
		zend_uint i;
		zend_uint orig_lineno = CG(zend_lineno);
		zend_error_cb = sandbox->orig_zend_error_cb;

		for (i = 0; i < sandbox->compilererror_cnt; i ++) {
			compilererror = &sandbox->compilererrors[i];
			CG(zend_lineno) = compilererror->lineno;
			zend_error(type, "%s", compilererror->error);
		}
		CG(zend_lineno) = orig_lineno;
		sandbox->compilererror_cnt = 0;

		sandbox->orig_zend_error_cb(type, error_filename, error_lineno, format, args);
		break;
	}
	}
}
/* }}} */
#endif
#ifdef ZEND_ENGINE_2_1
static zend_bool xc_auto_global_callback(char *name, uint name_len TSRMLS_DC) /* {{{ */
{
	return 0;
}
/* }}} */
static int xc_auto_global_arm(zend_auto_global *auto_global TSRMLS_DC) /* {{{ */
{
	if (auto_global->auto_global_callback) {
		auto_global->armed = 1;
		auto_global->auto_global_callback = xc_auto_global_callback;
	}
	else {
		auto_global->armed = 0;
	}
	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */
#endif

void xc_hash_copy_if(HashTable *target, HashTable *source, copy_ctor_func_t pCopyConstructor, void *tmp, uint size, xc_if_func_t checker) /* {{{ */
{
	Bucket *p;
	void *new_entry;
	zend_bool setTargetPointer;

	setTargetPointer = !target->pInternalPointer;
	p = source->pListHead;
	while (p) {
		if (checker(p->pData)) {
			if (setTargetPointer && source->pInternalPointer == p) {
				target->pInternalPointer = NULL;
			}
			if (p->nKeyLength) {
				zend_u_hash_quick_update(target, p->key.type, ZSTR(BUCKET_KEY_S(p)), p->nKeyLength, p->h, p->pData, size, &new_entry);
			} else {
				zend_hash_index_update(target, p->h, p->pData, size, &new_entry);
			}
			if (pCopyConstructor) {
				pCopyConstructor(new_entry);
			}
		}
		p = p->pListNext;
	}
	if (!target->pInternalPointer) {
		target->pInternalPointer = target->pListHead;
	}
}
/* }}} */
#ifdef HAVE_XCACHE_CONSTANT
static zend_bool xc_is_internal_zend_constant(zend_constant *c) /* {{{ */
{
	return (c->flags & CONST_PERSISTENT) ? 1 : 0;
}
/* }}} */
void xc_zend_constant_ctor(zend_constant *c) /* {{{ */
{
	assert((c->flags & CONST_PERSISTENT));
	ZSTR_U(c->name) = UNISW(zend_strndup, zend_ustrndup)(ZSTR_U(c->name), c->name_len - 1);
}
/* }}} */
void xc_zend_constant_dtor(zend_constant *c) /* {{{ */
{
	free(ZSTR_V(c->name));
}
/* }}} */
static void xc_free_zend_constant(zend_constant *c) /* {{{ */
{
	if (!(c->flags & CONST_PERSISTENT)) {
		zval_dtor(&c->value);
	}
	free(ZSTR_V(c->name));
}
/* }}} */
void xc_copy_internal_zend_constants(HashTable *target, HashTable *source) /* {{{ */
{
	zend_constant tmp_const;
	xc_hash_copy_if(target, source, (copy_ctor_func_t) xc_zend_constant_ctor, (void *) &tmp_const, sizeof(zend_constant), (xc_if_func_t) xc_is_internal_zend_constant);
}
/* }}} */
#endif
xc_sandbox_t *xc_sandbox_init(xc_sandbox_t *sandbox, char *filename TSRMLS_DC) /* {{{ */
{
	HashTable *h;

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

	OG(class_table) = CG(class_table);
	CG(class_table) = &TG(class_table);
	EG(class_table) = CG(class_table);

#ifdef ZEND_ENGINE_2_1
	OG(auto_globals) = CG(auto_globals);
	CG(auto_globals) = &TG(auto_globals);
#endif

	TG(included_files) = &EG(included_files);

	zend_hash_init_ex(TG(included_files), 5, NULL, NULL, 0, 1);
#ifdef HAVE_XCACHE_CONSTANT
	h = OG(zend_constants);
	zend_hash_init_ex(&TG(zend_constants),  20, NULL, (dtor_func_t) xc_free_zend_constant, h->persistent, h->bApplyProtection);
	xc_copy_internal_zend_constants(&TG(zend_constants), &XG(internal_constant_table));
	TG(internal_constant_tail) = TG(zend_constants).pListTail;
#endif
	h = OG(function_table);
	zend_hash_init_ex(&TG(function_table), 128, NULL, ZEND_FUNCTION_DTOR, h->persistent, h->bApplyProtection);
	{
		zend_function tmp_func;
		zend_hash_copy(&TG(function_table), &XG(internal_function_table), NULL, (void *) &tmp_func, sizeof(tmp_func));
	}
	TG(internal_function_tail) = TG(function_table).pListTail;

	h = OG(class_table);
	zend_hash_init_ex(&TG(class_table),     16, NULL, ZEND_CLASS_DTOR, h->persistent, h->bApplyProtection);
#if 0 && TODO
	{
		xc_cest_t tmp_cest;
		zend_hash_copy(&TG(class_table), &XG(internal_class_table), NULL, (void *) &tmp_cest, sizeof(tmp_cest));
	}
#endif
	TG(internal_class_tail) = TG(class_table).pListTail;

#ifdef ZEND_ENGINE_2_1
	/* shallow copy, don't destruct */
	h = OG(auto_globals);
	zend_hash_init_ex(&TG(auto_globals),     8, NULL,           NULL, h->persistent, h->bApplyProtection);
	{
		zend_auto_global tmp_autoglobal;

		zend_hash_copy(&TG(auto_globals), OG(auto_globals), NULL, (void *) &tmp_autoglobal, sizeof(tmp_autoglobal));
		zend_hash_apply(&TG(auto_globals), (apply_func_t) xc_auto_global_arm TSRMLS_CC);
	}
#endif

	sandbox->filename = filename;

#ifdef XCACHE_ERROR_CACHING
	sandbox->orig_user_error_handler_error_reporting = EG(user_error_handler_error_reporting);
	EG(user_error_handler_error_reporting) = 0;

	sandbox->compilererror_cnt  = 0;
	sandbox->compilererror_size = 0;
	sandbox->orig_zend_error_cb = zend_error_cb;
	zend_error_cb = xc_sandbox_error_cb;
#endif

#ifdef ZEND_COMPILE_IGNORE_INTERNAL_CLASSES
	sandbox->orig_compiler_options = CG(compiler_options);
	/* Using ZEND_COMPILE_IGNORE_INTERNAL_CLASSES for ZEND_FETCH_CLASS_RT_NS_CHECK
	 */
	CG(compiler_options) |= ZEND_COMPILE_IGNORE_INTERNAL_CLASSES | ZEND_COMPILE_NO_CONSTANT_SUBSTITUTION | ZEND_COMPILE_DELAYED_BINDING;
#endif

	XG(sandbox) = (void *) sandbox;
	return sandbox;
}
/* }}} */
#ifndef ZEND_COMPILE_DELAYED_BINDING
static void xc_early_binding_cb(zend_op *opline, int oplineno, void *data TSRMLS_DC) /* {{{ */
{
	xc_sandbox_t *sandbox = (xc_sandbox_t *) data;
	xc_do_early_binding(CG(active_op_array), OG(class_table), oplineno TSRMLS_CC);
}
/* }}} */
#endif
static void xc_sandbox_install(xc_sandbox_t *sandbox, xc_install_action_t install TSRMLS_DC) /* {{{ */
{
	zend_uint i;
	Bucket *b;

#ifdef HAVE_XCACHE_CONSTANT
	for (b = TG(zend_constants).pListHead; b != NULL && b != TG(internal_constant_tail); b = b->pListNext) {
		zend_constant *c = (zend_constant*) b->pData;
		xc_free_zend_constant(c);
	}

	b = TG(internal_constant_tail) ? TG(internal_constant_tail)->pListNext : TG(zend_constants).pListHead;
	/* install constants */
	while (b != NULL) {
		zend_constant *c = (zend_constant*) b->pData;
		xc_install_constant(sandbox->filename, c,
				BUCKET_KEY_TYPE(b), ZSTR(BUCKET_KEY_S(b)), b->nKeyLength, b->h TSRMLS_CC);
		b = b->pListNext;
	}
#endif

	b = TG(internal_function_tail) ? TG(internal_function_tail)->pListNext : TG(function_table).pListHead;
	/* install function */
	while (b != NULL) {
		zend_function *func = (zend_function*) b->pData;
		xc_install_function(sandbox->filename, func,
				BUCKET_KEY_TYPE(b), ZSTR(BUCKET_KEY_S(b)), b->nKeyLength, b->h TSRMLS_CC);
		b = b->pListNext;
	}

	b = TG(internal_class_tail) ? TG(internal_class_tail)->pListNext : TG(class_table).pListHead;
	/* install class */
	while (b != NULL) {
		xc_install_class(sandbox->filename, (xc_cest_t*) b->pData, -1,
				BUCKET_KEY_TYPE(b), ZSTR(BUCKET_KEY_S(b)), b->nKeyLength, b->h TSRMLS_CC);
		b = b->pListNext;
	}

#ifdef ZEND_ENGINE_2_1
	/* trigger auto_globals jit */
	for (b = TG(auto_globals).pListHead; b != NULL; b = b->pListNext) {
		zend_auto_global *auto_global = (zend_auto_global *) b->pData;
		/* check if actived */
		if (auto_global->auto_global_callback && !auto_global->armed) {
			zend_u_is_auto_global(BUCKET_KEY_TYPE(b), ZSTR(BUCKET_KEY_S(b)), auto_global->name_len TSRMLS_CC);
		}
	}
#endif

	if (install != XC_InstallNoBinding) {
#ifdef ZEND_COMPILE_DELAYED_BINDING
		zend_do_delayed_early_binding(CG(active_op_array) TSRMLS_CC);
#else
		xc_undo_pass_two(CG(active_op_array) TSRMLS_CC);
		xc_foreach_early_binding_class(CG(active_op_array), xc_early_binding_cb, (void *) sandbox TSRMLS_CC);
		xc_redo_pass_two(CG(active_op_array) TSRMLS_CC);
#endif
	}

#ifdef XCACHE_ERROR_CACHING
	/* restore trigger errors */
	for (i = 0; i < sandbox->compilererror_cnt; i ++) {
		xc_compilererror_t *error = &sandbox->compilererrors[i];
		CG(zend_lineno) = error->lineno;
		zend_error(error->type, "%s", error->error);
	}
	CG(zend_lineno) = 0;
#endif

	i = 1;
	zend_hash_add(&OG(included_files), sandbox->filename, strlen(sandbox->filename) + 1, (void *)&i, sizeof(int), NULL);
}
/* }}} */
void xc_sandbox_free(xc_sandbox_t *sandbox, xc_install_action_t install TSRMLS_DC) /* {{{ */
{
	XG(sandbox) = NULL;
#ifdef XCACHE_ERROR_CACHING
	EG(user_error_handler_error_reporting) = sandbox->orig_user_error_handler_error_reporting;
	zend_error_cb = sandbox->orig_zend_error_cb;
#endif

	/* restore first first install function/class */
#ifdef HAVE_XCACHE_CONSTANT
	EG(zend_constants) = OG(zend_constants);
#endif
	CG(function_table) = OG(function_table);
	CG(class_table)    = OG(class_table);
	EG(class_table)    = CG(class_table);
#ifdef ZEND_ENGINE_2_1
	CG(auto_globals)   = OG(auto_globals);
#endif

	if (install != XC_NoInstall) {
		CG(in_compilation)    = 1;
		CG(compiled_filename) = sandbox->filename;
		CG(zend_lineno)       = 0;
		xc_sandbox_install(sandbox, install TSRMLS_CC);
		CG(in_compilation)    = 0;
		CG(compiled_filename) = NULL;

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
#ifdef ZEND_ENGINE_2_1
	zend_hash_destroy(&TG(auto_globals));
#endif
	zend_hash_destroy(TG(included_files));

	/* restore orig here, as EG/CG holded tmp before */
	memcpy(&EG(included_files), &OG(included_files), sizeof(EG(included_files)));

#ifdef XCACHE_ERROR_CACHING
	if (sandbox->compilererrors) {
		zend_uint i;
		for (i = 0; i < sandbox->compilererror_cnt; i ++) {
			efree(sandbox->compilererrors[i].error);
		}
		efree(sandbox->compilererrors);
	}
#endif

#ifdef ZEND_COMPILE_IGNORE_INTERNAL_CLASSES
	CG(compiler_options) = sandbox->orig_compiler_options;
#endif

	if (sandbox->alloc) {
		efree(sandbox);
	}
}
/* }}} */
int xc_vtrace(const char *fmt, va_list args) /* {{{ */
{
	return vfprintf(stderr, fmt, args);
}
/* }}} */
int xc_trace(const char *fmt, ...) /* {{{ */
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = xc_vtrace(fmt, args);
	va_end(args);
	return ret;
}
/* }}} */

#ifndef ZEND_ENGINE_2_3
size_t zend_dirname(char *path, size_t len) /* {{{ */
{
	php_dirname(path, len);
	return strlen(path);
}
/* }}} */

long zend_atol(const char *str, int str_len) /* {{{ */
{
	long retval;

	if (!str_len) {
		str_len = strlen(str);
	}

	retval = strtol(str, NULL, 0);
	if (str_len > 0) {
		switch (str[str_len - 1]) {
		case 'g':
		case 'G':
			retval *= 1024;
			/* break intentionally missing */
		case 'm':
		case 'M':
			retval *= 1024;
			/* break intentionally missing */
		case 'k':
		case 'K':
			retval *= 1024;
			break;
		}
	}

	return retval;
}
/* }}} */

#endif
