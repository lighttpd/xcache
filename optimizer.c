#if 0
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
	bbid_t     fall;
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
static int op_get_jmpout(int *jmpout_op1, int *jmpout_op2, int *jmpout_ext, zend_bool *fall, zend_op *opline) /* {{{ */
{
	/* break=will fall */
	switch (opline->opcode) {
	case ZEND_RETURN:
	case ZEND_EXIT:
		return SUCCESS; /* no fall */

	case ZEND_JMP:
		*jmpout_op1 = opline->op1.u.opline_num;
		return SUCCESS; /* no fall */

	case ZEND_JMPZNZ:
		*jmpout_ext = opline->extended_value;
		*jmpout_op2 = opline->op2.u.opline_num;
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
		*jmpout_op2 = opline->op2.u.opline_num;
		break;

	default:
		return FAILURE;
	}

	*fall = 1;
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
	bb->fall       = BBID_INVALID;

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
#ifdef DEBUG
static void bb_print(bb_t *bb, zend_op *opcodes) /* {{{ */
{
	fprintf(stderr,
			"%3d %3d %3d"
			" %c%c"
			" %3d %3d %3d %3d\r\n"
			, bb->id, bb->count, bb->opcodes - opcodes
			, bb->used ? 'U' : ' ', bb->alloc ? 'A' : ' '
			, bb->jmpout_op1, bb->jmpout_op2, bb->jmpout_ext, bb->fall
			);
}
/* }}} */
#endif

#define bbs_get(bbs, n) xc_stack_get(bbs, n)
static void bbs_destroy(bbs_t *bbs) /* {{{ */
{
	bb_t *bb;
	while (xc_stack_count(bbs)) {
		bb = (bb_t *) xc_stack_pop(bbs);
		bb_destroy(bb);
	}
	xc_stack_destroy(bbs);
}
/* }}} */
#ifdef DEBUG
static void bbs_print(bbs_t *bbs, zend_op *opcodes) /* {{{ */
{
	int i;
	for (i = 0; i < xc_stack_count(bbs); i ++) {
		bb_print(bbs_get(bbs, i), opcodes);
	}
}
/* }}} */
#endif
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
	bb_t *pbb;
	bbid_t id;
	int jmpout_op1, jmpout_op2, jmpout_ext;
	zend_bool fall;
	bbid_t *bbids          = do_alloca(count * sizeof(bbid_t));
	zend_bool *markjmpins  = do_alloca(count * sizeof(zend_bool));
	zend_bool *markjmpouts = do_alloca(count * sizeof(zend_bool));

	/* {{{ mark jmpin/jumpout */
	memset(markjmpins,  0, count * sizeof(zend_bool));
	memset(markjmpouts, 0, count * sizeof(zend_bool));

	for (i = 0; i < count; i ++) {
		jmpout_op1 = jmpout_op2 = jmpout_ext = -1;
		fall = 0;
		if (op_get_jmpout(&jmpout_op1, &jmpout_op2, &jmpout_ext, &fall, &opcodes[i]) == SUCCESS) {
			markjmpouts[i] = 1;

			if (jmpout_op1 != -1) {
				markjmpins[jmpout_op1] = 1;
			}
			if (jmpout_op2 != -1) {
				markjmpins[jmpout_op2] = 1;
			}
			if (jmpout_ext != -1) {
				markjmpins[jmpout_ext] = 1;
			}
		}
	}
	/* }}} */
	/* {{{ fill opcodes with newly allocated id */
	for (i = 0; i < count; i ++) {
		bbids[i] = BBID_INVALID;
	}

	prev = 0;
	id = 0;
	for (i = 1; i < count; i ++) {
		if (markjmpins[i] || markjmpouts[i - 1]) {
			for (; prev < i; prev ++) {
				bbids[prev] = id;
			}
			id ++;
			prev = i;
		}
	}

	if (prev < count) {
		for (; prev < i; prev ++) {
			bbids[prev] = id;
		}
	}
	/* }}} */
	/* {{{ create basic blocks */
	prev = 0;
	id = 0;
	for (i = 1; i < count; i ++) {
		if (id == bbids[i]) {
			continue;
		}
		id = bbids[i];

		pbb = bbs_new_bb_ex(bbs, opcodes + prev, i - prev);
		jmpout_op1 = jmpout_op2 = jmpout_ext = -1;
		fall = 0;
		if (op_get_jmpout(&jmpout_op1, &jmpout_op2, &jmpout_ext, &fall, &opcodes[prev]) == SUCCESS) {
			if (jmpout_op1 != -1) {
				pbb->jmpout_op1 = bbids[jmpout_op1];
				assert(pbb->jmpout_op1 != BBID_INVALID);
			}
			if (jmpout_op2 != -1) {
				pbb->jmpout_op2 = bbids[jmpout_op2];
				assert(pbb->jmpout_op2 != BBID_INVALID);
			}
			if (jmpout_ext != -1) {
				pbb->jmpout_ext = bbids[jmpout_ext];
				assert(pbb->jmpout_ext != BBID_INVALID);
			}
			if (fall && i + 1 < count) {
				pbb->fall = bbids[i + 1];
				assert(pbb->fall != BBID_INVALID);
			}
		}
		prev = i + 1;
	}
	assert(prev == count);
	/* }}} */

	free_alloca(bbids);
	free_alloca(markjmpins);
	free_alloca(markjmpouts);
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
#	if 0
	TRACE("optimize file: %s", op_array->filename);
	xc_dprint_zend_op_array(op_array, 0 TSRMLS_CC);
#	endif
#endif

	if (op_array_convert_switch(op_array) == SUCCESS) {
		bbs_init(&bbs);
		if (bbs_build_from(&bbs, op_array->opcodes, op_array->last) == SUCCESS) {
#ifdef DEBUG
			bbs_print(&bbs, op_array->opcodes);
#endif
		}
		bbs_destroy(&bbs);
	}

#ifdef DEBUG
#	if 0
	TRACE("%s", "after compiles");
	xc_dprint_zend_op_array(op_array, 0 TSRMLS_CC);
#	endif
#endif
	xc_redo_pass_two(op_array TSRMLS_CC);
	return 0;
}
/* }}} */
void xc_optimize(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	xc_compile_result_t cr;

	if (!op_array) {
		return;
	}

	xc_compile_result_init_cur(&cr, op_array TSRMLS_CC);

	xc_apply_op_array(&cr, (apply_func_t) xc_undo_pass_two TSRMLS_CC);
	xc_apply_op_array(&cr, (apply_func_t) xc_optimize_op_array TSRMLS_CC);
	xc_apply_op_array(&cr, (apply_func_t) xc_redo_pass_two TSRMLS_CC);

	xc_compile_result_free(&cr);
}
/* }}} */
