#if 1
#define DEBUG
#endif

#include "utils.h"
#include "optimizer.h"
/* the "vector" stack */
#include "stack.h"

#ifdef DEBUG
#	include "processor.h"
#endif

typedef int bbid_t;
enum {
	BBID_INVALID = -1,
};
/* {{{ basic block */
typedef struct _bb_t {
	bbid_t     id;
	zend_bool  used;

	zend_bool  alloc;
	zend_op   *opcodes;
	int        count;
	int        size;

	bbid_t     jmpout_op1;
	bbid_t     jmpout_op2;
	bbid_t     jmpout_ext;
	bbid_t     follow;
} bb_t;
/* }}} */

/* basic blocks */
typedef xc_stack_t bbs_t;

/* oparray help functions */
static int op_array_convert_switch(zend_op_array *op_array) /* {{{ */
{
	int i;

	if (op_array->brk_cont_array == NULL) {
		return SUCCESS;
	}

	for (i = 0; i < op_array->last; i ++) {
		zend_op *opline = &op_array->opcodes[i];
		zend_brk_cont_element *jmp_to;
		int array_offset, nest_levels, original_nest_levels;

		if (opline->opcode != ZEND_BRK && opline->opcode != ZEND_CONT) {
			continue;
		}
		if (opline->op2.op_type != IS_CONST
		 || opline->op2.u.constant.type != IS_LONG) {
			return FAILURE;
		}

		nest_levels = opline->op2.u.constant.value.lval;
		original_nest_levels = nest_levels;

		array_offset = opline->op1.u.opline_num;
		do {
			if (array_offset == -1) {
				/* this is a runtime error in ZE
				zend_error(E_ERROR, "Cannot break/continue %d level%s", original_nest_levels, (original_nest_levels == 1) ? "" : "s");
				*/
				return FAILURE;
			}
			jmp_to = &op_array->brk_cont_array[array_offset];
			if (nest_levels > 1) {
				zend_op *brk_opline = &op_array->opcodes[jmp_to->brk];

				switch (brk_opline->opcode) {
				case ZEND_SWITCH_FREE:
					break;
				case ZEND_FREE:
					break;
				}
			}
			array_offset = jmp_to->parent;
		} while (--nest_levels > 0);

		/* rewrite to jmp */
		if (opline->opcode == ZEND_BRK) {
			opline->op1.u.opline_num = jmp_to->brk;
		}
		else {
			opline->op1.u.opline_num = jmp_to->cont;
		}
		opline->op2.op_type = IS_UNUSED;
		opline->opcode = ZEND_JMP;
	}

	if (op_array->brk_cont_array != NULL) {
		efree(op_array->brk_cont_array);
		op_array->brk_cont_array = NULL;
	}
	op_array->last_brk_cont = 0;
	return SUCCESS;
}
/* }}} */
static int op_get_jmpout(bb_t *bb, zend_op *opcodes, zend_op *opline) /* {{{ */
{
	/* break=have follow */
	switch (opline->opcode) {
	case ZEND_RETURN:
	case ZEND_EXIT:
		break;

	case ZEND_JMP:
		bb->jmpout_op1 = opline->op1.u.opline_num;
		return SUCCESS; /* no follow */

	case ZEND_JMPZNZ:
		bb->jmpout_ext = opline->extended_value;
		bb->jmpout_op2 = opline->op2.u.opline_num;
		break;

	case ZEND_JMPZ:
	case ZEND_JMPNZ:
	case ZEND_JMPZ_EX:
	case ZEND_JMPNZ_EX:
#ifndef ZEND_ENGINE_2_1
		/* Pre-PHP 5.1 only */
	case ZEND_JMP_NO_CTOR:
#else       
	case ZEND_NEW:
	case ZEND_FE_RESET:
#endif      
	case ZEND_FE_FETCH:
		bb->jmpout_op2 = opline->op2.u.opline_num;
		break;

	default:
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

/*
 * basic block functions
 */

static bb_t *bb_new_ex(zend_op *opcodes, int count) /* {{{ */
{
	bb_t *bb = (bb_t *) ecalloc(sizeof(bb_t), 1);

	bb->jmpout_op1 = BBID_INVALID;
	bb->jmpout_op2 = BBID_INVALID;
	bb->jmpout_ext = BBID_INVALID;
	bb->follow     = BBID_INVALID;

	if (opcodes) {
		bb->alloc   = 0;
		bb->size    = bb->count = count;
		bb->opcodes = opcodes;
	}
	else {
		bb->alloc   = 1;
		bb->size    = bb->count = 8;
		bb->opcodes = ecalloc(sizeof(zend_op), bb->size);
	}

	return bb;
}
/* }}} */
#define bb_new() bb_new_ex(NULL, 0)
static void bb_destroy(bb_t *bb) /* {{{ */
{
	if (bb->alloc) {
		efree(bb->opcodes);
	}
	efree(bb);
}
/* }}} */
#define bbs_get(bbs, n) xc_stack_get(bbs, n)
static void bbs_destroy(bbs_t *bbs) /* {{{ */
{
	bb_t *bb;
	while (xc_stack_count(bbs)) {
		bb = (bb_t *) xc_stack_pop(bbs);
		bb_destroy(bb);
	}
}
/* }}} */
#define bbs_init(bbs) xc_stack_init_ex(bbs, 16)
static bb_t *bbs_add_bb(bbs_t *bbs, bb_t *bb) /* {{{ */
{
	bb->id = (bbid_t) xc_stack_count(bbs);
	xc_stack_push(bbs, (void *) bb);
	return bb;
}
/* }}} */
static bb_t *bbs_new_bb_ex(bbs_t *bbs, zend_op *opcodes, int count) /* {{{ */
{
	return bbs_add_bb(bbs, bb_new_ex(opcodes, count));
}
/* }}} */
static int bbs_build_from(bbs_t *bbs, zend_op *opcodes, int count) /* {{{ */
{
	int i, prev;
	bb_t bb, *pbb;
	zend_bool *markjmpins  = do_alloca(count);
	zend_bool *markjmpouts = do_alloca(count);

	memset(markjmpins,  0, sizeof(zend_bool));
	memset(markjmpouts, 0, sizeof(zend_bool));

	for (i = 0; i < count; i ++) {
		/* BBID_INVALID=invalidate line 
		 * bb.jmpout_op1 bb.jmpout_op2 bb.jmpout_ext bb.follow means opline number here, not basicblock id
		 */
		bb.jmpout_op1 = bb.jmpout_op2 = BBID_INVALID;
		bb.jmpout_ext = bb.follow     = BBID_INVALID;
		if (op_get_jmpout(&bb, opcodes, &opcodes[i]) == SUCCESS) {
			markjmpouts[i] = 1;

			if (bb.jmpout_op1 != BBID_INVALID) {
				markjmpins[bb.jmpout_op1] = 1;
			}
			if (bb.jmpout_op2 != BBID_INVALID) {
				markjmpins[bb.jmpout_op2] = 1;
			}
			if (bb.jmpout_ext != BBID_INVALID) {
				markjmpins[bb.jmpout_ext] = 1;
			}

			if (i < count && bb.follow != BBID_INVALID) {
				markjmpins[i + 1] = 1;
			}
		}
	}

	prev = 0;
	for (i = 1; i < count; i ++) {
		if (markjmpins[i]) {
			pbb = bbs_new_bb_ex(bbs, opcodes + prev, i - prev);
			op_get_jmpout(pbb, opcodes, &opcodes[prev]);
			prev = i;
		}
	}

	if (prev != count - 1) {
		pbb = bbs_new_bb_ex(bbs, opcodes + prev, count - prev);
	}

	return SUCCESS;
}
/* }}} */

/*
 * optimize
 */
static int xc_optimize_op_array(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	bbs_t bbs;

	if (op_array->type != ZEND_USER_FUNCTION) {
		return 0;
	}
	xc_undo_pass_two(op_array TSRMLS_CC);
#ifdef DEBUG
	TRACE("optimize file: %s", op_array->filename);
	xc_dprint_zend_op_array(op_array, 0 TSRMLS_CC);
#endif

	if (op_array_convert_switch(op_array)) {
		bbs_init(&bbs);
		if (bbs_build_from(&bbs, op_array->opcodes, op_array->last)) {
		}
		bbs_destroy(&bbs);
	}

#ifdef DEBUG
	TRACE("%s", "after compiles");
	xc_dprint_zend_op_array(op_array, 0 TSRMLS_CC);
#endif
	xc_redo_pass_two(op_array TSRMLS_CC);
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
