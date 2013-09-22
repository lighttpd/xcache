
#include "xcache.h"
#include "xcache_globals.h"
#include "xc_utils.h"
#ifdef ZEND_ENGINE_2_1
#include "zend_vm.h"
#endif
#include "xc_opcode_spec.h"
#include "util/xc_trace.h"

#ifndef max
#define max(a, b) ((a) < (b) ? (b) : (a))
#endif

#ifndef ZEND_VM_SET_OPCODE_HANDLER
#	define ZEND_VM_SET_OPCODE_HANDLER(opline) do { } while (0)
#endif

#ifdef ZEND_ENGINE_2_4
#	define OP_ZVAL_DTOR(op) do { } while(0)
#else
#	define OP_ZVAL_DTOR(op) do { \
		Z_UNSET_ISREF(Z_OP_CONSTANT(op)); \
		zval_dtor(&Z_OP_CONSTANT(op)); \
	} while(0)
#endif

/* }}} */

xc_compile_result_t *xc_compile_result_init(xc_compile_result_t *cr, /* {{{ */
		zend_op_array *op_array,
		HashTable *function_table,
		HashTable *class_table)
{
	assert(cr);
	cr->op_array       = op_array;
	cr->function_table = function_table;
	cr->class_table    = class_table;
	return cr;
}
/* }}} */
xc_compile_result_t *xc_compile_result_init_cur(xc_compile_result_t *cr, zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	assert(cr);
	return xc_compile_result_init(cr, op_array, CG(function_table), CG(class_table));
}
/* }}} */
void xc_compile_result_free(xc_compile_result_t *cr) /* {{{ */
{
}
/* }}} */

typedef struct {
	apply_func_t applyer;
} xc_apply_func_info;
static int xc_apply_function(zend_function *zf, xc_apply_func_info *fi TSRMLS_DC) /* {{{ */
{
	switch (zf->type) {
	case ZEND_USER_FUNCTION:
	case ZEND_EVAL_CODE:
		return fi->applyer(&zf->op_array TSRMLS_CC);
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
	xc_apply_func_info fi;
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
	return xc_apply_function(zf, &mi->fi TSRMLS_CC);
}
/* }}} */
static int xc_apply_cest(xc_cest_t *cest, xc_apply_func_info *fi TSRMLS_DC) /* {{{ */
{
	xc_apply_method_info mi;

	mi.fi = *fi;
	mi.ce = CestToCePtr(*cest);
	zend_hash_apply_with_argument(&(CestToCePtr(*cest)->function_table), (apply_func_arg_t) xc_apply_method, &mi TSRMLS_CC);
	return 0;
}
/* }}} */
int xc_apply_op_array(xc_compile_result_t *cr, apply_func_t applyer TSRMLS_DC) /* {{{ */
{
	xc_apply_func_info fi;
	fi.applyer = applyer;
	zend_hash_apply_with_argument(cr->function_table, (apply_func_arg_t) xc_apply_function, &fi TSRMLS_CC);
	zend_hash_apply_with_argument(cr->class_table, (apply_func_arg_t) xc_apply_cest, &fi TSRMLS_CC);

	return applyer(cr->op_array TSRMLS_CC);
}
/* }}} */
int xc_undo_pass_two(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
#ifdef ZEND_ENGINE_2
	zend_op *opline, *opline_end;
#endif

#ifdef ZEND_ENGINE_2_4
	if (!(op_array->fn_flags & ZEND_ACC_DONE_PASS_TWO)) {
		return 0;
	}
#else
	if (!op_array->done_pass_two) {
		return 0;
	}
#endif

#ifdef ZEND_ENGINE_2
	opline = op_array->opcodes;
	opline_end = opline + op_array->last;
	while (opline < opline_end) {
#	ifdef ZEND_ENGINE_2_4
		if (opline->op1_type == IS_CONST) {
			opline->op1.constant = opline->op1.literal - op_array->literals;
		}
		if (opline->op2_type == IS_CONST) {
			opline->op2.constant = opline->op2.literal - op_array->literals;
		}
#	endif

		switch (opline->opcode) {
#	ifdef ZEND_GOTO
			case ZEND_GOTO:
#	endif
			case ZEND_JMP:
#	ifdef ZEND_FAST_CALL
			case ZEND_FAST_CALL:
#	endif
				assert(Z_OP(opline->op1).jmp_addr >= op_array->opcodes);
				assert((zend_uint) (Z_OP(opline->op1).jmp_addr - op_array->opcodes) < op_array->last);
				Z_OP(opline->op1).opline_num = Z_OP(opline->op1).jmp_addr - op_array->opcodes;
				break;
			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
#	ifdef ZEND_JMP_SET
			case ZEND_JMP_SET:
#	endif
#	ifdef ZEND_JMP_SET_VAR
			case ZEND_JMP_SET_VAR:
#	endif
				assert(Z_OP(opline->op2).jmp_addr >= op_array->opcodes);
				assert((zend_uint) (Z_OP(opline->op2).jmp_addr - op_array->opcodes) < op_array->last);
				Z_OP(opline->op2).opline_num = Z_OP(opline->op2).jmp_addr - op_array->opcodes;
				break;
		}
		opline++;
	}
#endif /* ZEND_ENGINE_2 */

#ifdef ZEND_ENGINE_2_4
	op_array->fn_flags &= ~ZEND_ACC_DONE_PASS_TWO;
#else
	op_array->done_pass_two = 0;
#endif

	return 0;
}
/* }}} */
int xc_redo_pass_two(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
#ifdef ZEND_ENGINE_2
	zend_op *opline, *opline_end;
#endif
#ifdef ZEND_ENGINE_2_4
	zend_literal *literal = op_array->literals;
#endif

#ifdef ZEND_ENGINE_2_4
	if ((op_array->fn_flags & ZEND_ACC_DONE_PASS_TWO)) {
		return 0;
	}
#else
	if (op_array->done_pass_two) {
		return 0;
	}
#endif

	/*
	op_array->opcodes = (zend_op *) erealloc(op_array->opcodes, sizeof(zend_op)*op_array->last);
	op_array->size = op_array->last;
	*/
#ifdef ZEND_ENGINE_2_4
	if (literal) {
		zend_literal *literal_end = literal + op_array->last_literal;
		while (literal < literal_end) {
			Z_SET_ISREF(literal->constant);
			Z_SET_REFCOUNT(literal->constant, 2); /* Make sure is_ref won't be reset */
			literal++;
		}
	}
#endif

#ifdef ZEND_ENGINE_2
	opline = op_array->opcodes;
	opline_end = opline + op_array->last;
	while (opline < opline_end) {
#	ifdef ZEND_ENGINE_2_4
		if (opline->op1_type == IS_CONST) {
			opline->op1.literal = op_array->literals + opline->op1.constant;
		}
		if (opline->op2_type == IS_CONST) {
			opline->op2.literal = op_array->literals + opline->op2.constant;
		}
#	else
		if (Z_OP_TYPE(opline->op1) == IS_CONST) {
			Z_SET_ISREF(Z_OP_CONSTANT(opline->op1));
			Z_SET_REFCOUNT(Z_OP_CONSTANT(opline->op1), 2); /* Make sure is_ref won't be reset */
		}
		if (Z_OP_TYPE(opline->op2) == IS_CONST) {
			Z_SET_ISREF(Z_OP_CONSTANT(opline->op2));
			Z_SET_REFCOUNT(Z_OP_CONSTANT(opline->op2), 2);
		}
#	endif
		switch (opline->opcode) {
#	ifdef ZEND_GOTO
			case ZEND_GOTO:
#	endif
			case ZEND_JMP:
#	ifdef ZEND_FAST_CALL
			case ZEND_FAST_CALL:
#	endif
				assert(Z_OP(opline->op1).opline_num < op_array->last);
				Z_OP(opline->op1).jmp_addr = op_array->opcodes + Z_OP(opline->op1).opline_num;
				break;
			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
#	ifdef ZEND_JMP_SET
			case ZEND_JMP_SET:
#	endif
#	ifdef ZEND_JMP_SET_VAR
			case ZEND_JMP_SET_VAR:
#	endif
				assert(Z_OP(opline->op2).opline_num < op_array->last);
				Z_OP(opline->op2).jmp_addr = op_array->opcodes + Z_OP(opline->op2).opline_num;
				break;
		}
		/* ZEND_VM_SET_OPCODE_HANDLER(opline); this is not undone, don't redo. only do this for loader */
		opline++;
	}
#endif /* ZEND_ENGINE_2 */

#ifdef ZEND_ENGINE_2_4
	op_array->fn_flags |= ZEND_ACC_DONE_PASS_TWO;
#else
	op_array->done_pass_two = 1;
#endif
	return 0;
}
/* }}} */

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

#ifndef ZEND_COMPILE_DELAYED_BINDING
int xc_foreach_early_binding_class(zend_op_array *op_array, xc_foreach_early_binding_class_cb callback, void *data TSRMLS_DC) /* {{{ */
{
	zend_op *opline, *begin, *opline_end, *next = NULL;

	opline = begin = op_array->opcodes;
	opline_end = opline + op_array->last;
	while (opline < opline_end) {
		switch (opline->opcode) {
#ifdef ZEND_GOTO
			case ZEND_GOTO:
#endif
			case ZEND_JMP:
#ifdef ZEND_FAST_CALL
			case ZEND_FAST_CALL:
#endif
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
#ifdef ZEND_JMP_SET_VAR
			case ZEND_JMP_SET_VAR:
#endif
				next = begin + Z_OP(opline->op2).opline_num;
				break;

			case ZEND_RETURN:
				opline = opline_end;
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
int xc_do_early_binding(zend_op_array *op_array, HashTable *class_table, int oplineno TSRMLS_DC) /* {{{ */
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
void xc_install_constant(ZEND_24(NOTHING, const) char *filename, zend_constant *constant, zend_uchar type, const24_zstr key, uint len, ulong h TSRMLS_DC) /* {{{ */
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
void xc_install_function(ZEND_24(NOTHING, const) char *filename, zend_function *func, zend_uchar type, const24_zstr key, uint len, ulong h TSRMLS_DC) /* {{{ */
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
ZESW(xc_cest_t *, void) xc_install_class(ZEND_24(NOTHING, const) char *filename, xc_cest_t *cest, int oplineno, zend_uchar type, const24_zstr key, uint len, ulong h TSRMLS_DC) /* {{{ */
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

void xc_hash_copy_if(HashTable *target, HashTable *source, copy_ctor_func_t pCopyConstructor, void *tmp, uint size, xc_if_func_t checker) /* {{{ */
{
	Bucket *p;
	void *new_entry;
	zend_bool setTargetPointer;

	setTargetPointer = !target->pInternalPointer;
	p = source->pListHead;
	while (p) {
		if (checker(p->pData)) {
			if (p->nKeyLength) {
				zend_u_hash_quick_update(target, p->key.type, ZSTR(BUCKET_KEY_S(p)), p->nKeyLength, p->h, p->pData, size, &new_entry);
			} else {
				zend_hash_index_update(target, p->h, p->pData, size, &new_entry);
			}
			if (pCopyConstructor) {
				pCopyConstructor(new_entry);
			}
			if (setTargetPointer && source->pInternalPointer == p) {
				target->pInternalPointer = new_entry;
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
void xc_copy_internal_zend_constants(HashTable *target, HashTable *source) /* {{{ */
{
	zend_constant tmp_const;
	xc_hash_copy_if(target, source, (copy_ctor_func_t) xc_zend_constant_ctor, (void *) &tmp_const, sizeof(zend_constant), (xc_if_func_t) xc_is_internal_zend_constant);
}
/* }}} */
#endif
