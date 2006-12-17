#if 0
#	define DEBUG
#endif

#include "utils.h"
#include "optimizer.h"
/* the "vector" stack */
#include "stack.h"

#ifdef DEBUG
#	include "processor.h"
#	include "const_string.h"
#	include "ext/standard/php_var.h"
#endif

#ifdef IS_CV
#	define XCACHE_IS_CV IS_CV
#else
#	define XCACHE_IS_CV 16
#endif
#define optimized_flag done_pass_two

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

	bbid_t     fall;
	bbid_t     catch;

	int        opnum; /* opnum after joining basic block */
} bb_t;
/* }}} */

/* basic blocks */
typedef xc_stack_t bbs_t;

/* op array helper functions */
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
/* {{{ op_flowinfo helper func */
enum {
	XC_OPNUM_INVALID = -1,
};
typedef struct {
	int       jmpout_op1;
	int       jmpout_op2;
	int       jmpout_ext;
	zend_bool fall;
} op_flowinfo_t;
static void op_flowinfo_init(op_flowinfo_t *fi)
{
	fi->jmpout_op1 = fi->jmpout_op2 = fi->jmpout_ext = XC_OPNUM_INVALID;
	fi->fall = 0;
}
/* }}} */
static int op_get_flowinfo(op_flowinfo_t *fi, zend_op *opline) /* {{{ */
{
	op_flowinfo_init(fi);

	/* break=will fall */
	switch (opline->opcode) {
	case ZEND_HANDLE_EXCEPTION:
	case ZEND_RETURN:
	case ZEND_EXIT:
		return SUCCESS; /* no fall */

	case ZEND_JMP:
		fi->jmpout_op1 = opline->op1.u.opline_num;
		return SUCCESS; /* no fall */

	case ZEND_JMPZNZ:
		fi->jmpout_op2 = opline->op2.u.opline_num;
		fi->jmpout_ext = (int) opline->extended_value;
		return SUCCESS; /* no fall */

	case ZEND_JMPZ:
	case ZEND_JMPNZ:
	case ZEND_JMPZ_EX:
	case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_NO_CTOR
	case ZEND_JMP_NO_CTOR:
#endif
#ifdef ZEND_NEW
	case ZEND_NEW:
#endif
#ifdef ZEND_FE_RESET
	case ZEND_FE_RESET:
#endif      
	case ZEND_FE_FETCH:
		fi->jmpout_op2 = opline->op2.u.opline_num;
		break;

#ifdef ZEND_CATCH
	case ZEND_CATCH:
		fi->jmpout_ext = (int) opline->extended_value;
		break;
#endif

	default:
		return FAILURE;
	}

	fi->fall = 1;
	return SUCCESS;
}
/* }}} */
#ifdef DEBUG
static void op_snprint(char *buf, int size, znode *op) /* {{{ */
{
	switch (op->op_type) {
	case IS_CONST:
		{
			zval result;
			zval *zv = &op->u.constant;
			TSRMLS_FETCH();

			php_start_ob_buffer(NULL, 0, 1 TSRMLS_CC);
			php_var_export(&zv, 1 TSRMLS_CC);

			php_ob_get_buffer(&result TSRMLS_CC); 
			php_end_ob_buffer(0, 0 TSRMLS_CC);
			snprintf(buf, size, Z_STRVAL(result));
			zval_dtor(&result);
		}
		break;

	case IS_TMP_VAR:
		snprintf(buf, size, "t@%d", op->u.var);
		break;

	case XCACHE_IS_CV:
	case IS_VAR:
		snprintf(buf, size, "v@%d", op->u.var);
		break;

	case IS_UNUSED:
		if (op->u.opline_num) {
			snprintf(buf, size, "u#%d", op->u.opline_num);
		}
		else {
			snprintf(buf, size, "-");
		}
		break;

	default:
		snprintf(buf, size, "%d %d", op->op_type, op->u.var);
	}
}
/* }}} */
static void op_print(int line, zend_op *first, zend_op *end) /* {{{ */
{
	zend_op *opline;
	for (opline = first; opline < end; opline ++) {
		char buf_r[20];
		char buf_1[20];
		char buf_2[20];
		op_snprint(buf_r, sizeof(buf_r), &opline->result);
		op_snprint(buf_1, sizeof(buf_1), &opline->op1);
		op_snprint(buf_2, sizeof(buf_2), &opline->op2);
		fprintf(stderr,
				"%3d %3d"
				" %-25s%-5s%-20s%-20s%5lu\r\n"
				, opline->lineno, opline - first + line
				, xc_get_opcode(opline->opcode), buf_r, buf_1, buf_2, opline->extended_value);
	}
}
/* }}} */
#endif

/*
 * basic block functions
 */

static bb_t *bb_new_ex(zend_op *opcodes, int count) /* {{{ */
{
	bb_t *bb = (bb_t *) ecalloc(sizeof(bb_t), 1);

	bb->fall       = BBID_INVALID;
	bb->catch      = BBID_INVALID;

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
	int line = bb->opcodes - opcodes;
	op_flowinfo_t fi;
	zend_op *last = bb->opcodes + bb->count - 1;

	op_get_flowinfo(&fi, last);

	fprintf(stderr,
			"\r\n==== #%-3d cnt:%-3d lno:%-3d"
			" %c%c"
			" op1:%-3d op2:%-3d ext:%-3d fal:%-3d cat:%-3d %s ====\r\n"
			, bb->id, bb->count, bb->alloc ? -1 : line
			, bb->used ? 'U' : ' ', bb->alloc ? 'A' : ' '
			, fi.jmpout_op1, fi.jmpout_op2, fi.jmpout_ext, bb->fall, bb->catch, xc_get_opcode(last->opcode)
			);
	op_print(line, bb->opcodes, last + 1);
}
/* }}} */
#endif

static bb_t *bbs_get(bbs_t *bbs, int n) /* {{{ */
{
	return (bb_t *) xc_stack_get(bbs, n);
}
/* }}} */
static int bbs_count(bbs_t *bbs) /* {{{ */
{
	return xc_stack_count(bbs);
}
/* }}} */
static void bbs_destroy(bbs_t *bbs) /* {{{ */
{
	bb_t *bb;
	while (bbs_count(bbs)) {
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
static int bbs_build_from(bbs_t *bbs, zend_op_array *op_array, int count) /* {{{ */
{
	int i, start;
	bb_t *pbb;
	bbid_t id;
	op_flowinfo_t fi;
	zend_op *opline;
	bbid_t *bbids          = do_alloca(count * sizeof(bbid_t));
	bbid_t *catchbbids     = do_alloca(count * sizeof(bbid_t));
	zend_bool *markbbhead  = do_alloca(count * sizeof(zend_bool));

	/* {{{ mark jmpin/jumpout */
	memset(markbbhead,  0, count * sizeof(zend_bool));

	markbbhead[0] = 1;
	for (i = 0; i < count; i ++) {
		if (op_get_flowinfo(&fi, &op_array->opcodes[i]) == SUCCESS) {
			if (fi.jmpout_op1 != XC_OPNUM_INVALID) {
				markbbhead[fi.jmpout_op1] = 1;
			}
			if (fi.jmpout_op2 != XC_OPNUM_INVALID) {
				markbbhead[fi.jmpout_op2] = 1;
			}
			if (fi.jmpout_ext != XC_OPNUM_INVALID) {
				markbbhead[fi.jmpout_ext] = 1;
			}
			if (i + 1 < count) {
				markbbhead[i + 1] = 1;
			}
		}
	}
	/* mark try start */
	for (i = 0; i < op_array->last_try_catch; i ++) {
		markbbhead[op_array->try_catch_array[i].try_op] = 1;
	}
	/* }}} */
	/* {{{ fill op lines with newly allocated id */
	for (i = 0; i < count; i ++) {
		bbids[i] = BBID_INVALID;
	}

	id = -1;
	for (i = 0; i < count; i ++) {
		if (markbbhead[i]) {
			id ++;
		}
		bbids[i] = id;
		TRACE("bbids[%d] = %d", i, id);
	}
	/* }}} */
	/* {{{ fill op lines with catch id */
	for (i = 0; i < count; i ++) {
		catchbbids[i] = BBID_INVALID;
	}

	for (i = 0; i < op_array->last_try_catch; i ++) {
		int j;
		zend_try_catch_element *e = &op_array->try_catch_array[i];
		for (j = e->try_op; j < e->catch_op; j ++) {
			catchbbids[j] = bbids[e->catch_op];
		}
	}
#ifdef DEBUG
	for (i = 0; i < count; i ++) {
		TRACE("catchbbids[%d] = %d", i, catchbbids[i]);
	}
#endif
	/* }}} */
	/* {{{ create basic blocks */
	start = 0;
	id = 0;
	/* loop over to deal with the last block */
	for (i = 1; i <= count; i ++) {
		if (i < count && id == bbids[i]) {
			continue;
		}

		opline = op_array->opcodes + start;
		pbb = bbs_new_bb_ex(bbs, opline, i - start);
		pbb->catch = catchbbids[start];

		/* last */
		opline = pbb->opcodes + pbb->count - 1;
		if (op_get_flowinfo(&fi, opline) == SUCCESS) {
			if (fi.jmpout_op1 != XC_OPNUM_INVALID) {
				opline->op1.u.opline_num = bbids[fi.jmpout_op1];
				assert(opline->op1.u.opline_num != BBID_INVALID);
			}
			if (fi.jmpout_op2 != XC_OPNUM_INVALID) {
				opline->op2.u.opline_num = bbids[fi.jmpout_op2];
				assert(opline->op2.u.opline_num != BBID_INVALID);
			}
			if (fi.jmpout_ext != XC_OPNUM_INVALID) {
				opline->extended_value = bbids[fi.jmpout_ext];
				assert(opline->extended_value != BBID_INVALID);
			}
			if (fi.fall && i + 1 < count) {
				pbb->fall = bbids[i + 1];
				TRACE("fall %d", pbb->fall);
				assert(pbb->fall != BBID_INVALID);
			}
		}
		if (i >= count) {
			break;
		}
		start = i;
		id = bbids[i];
	}
	/* }}} */

	free_alloca(catchbbids);
	free_alloca(bbids);
	free_alloca(markbbhead);
	return SUCCESS;
}
/* }}} */
static void bbs_restore_opnum(bbs_t *bbs, zend_op_array *op_array) /* {{{ */
{
	int i;
	bbid_t lasttrybbid;

	for (i = 0; i < bbs_count(bbs); i ++) {
		op_flowinfo_t fi;
		bb_t *bb = bbs_get(bbs, i);
		zend_op *last = bb->opcodes + bb->count - 1;

		if (op_get_flowinfo(&fi, last) == SUCCESS) {
			if (fi.jmpout_op1 != XC_OPNUM_INVALID) {
				last->op1.u.opline_num = bbs_get(bbs, fi.jmpout_op1)->opnum;
				assert(last->op1.u.opline_num != BBID_INVALID);
			}
			if (fi.jmpout_op2 != XC_OPNUM_INVALID) {
				last->op2.u.opline_num = bbs_get(bbs, fi.jmpout_op2)->opnum;
				assert(last->op2.u.opline_num != BBID_INVALID);
			}
			if (fi.jmpout_ext != XC_OPNUM_INVALID) {
				last->extended_value = bbs_get(bbs, fi.jmpout_ext)->opnum;
				assert(last->extended_value != BBID_INVALID);
			}
		}
	}

	lasttrybbid = BBID_INVALID;
	op_array->last_try_catch = 0;
	for (i = 0; i < bbs_count(bbs); i ++) {
		bb_t *bb = bbs_get(bbs, i);

		if (lasttrybbid != bb->catch) {
			if (lasttrybbid != BBID_INVALID) {
				int try_catch_offset = op_array->last_try_catch ++;

				op_array->try_catch_array = erealloc(op_array->try_catch_array, sizeof(zend_try_catch_element) * op_array->last_try_catch);
				op_array->try_catch_array[try_catch_offset].try_op = bbs_get(bbs, lasttrybbid)->opnum;
				op_array->try_catch_array[try_catch_offset].catch_op = bbs_get(bbs, bb->id)->opnum;
			}
			lasttrybbid = bb->catch;
		}
	}
	/* it is impossible to have last bb catched */
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

	/* don't optimize twice */
	if (op_array->optimized_flag) {
		return 0;
	}
	op_array->optimized_flag = 1;

#ifdef DEBUG
#	if 0
	TRACE("optimize file: %s", op_array->filename);
	xc_dprint_zend_op_array(op_array, 0 TSRMLS_CC);
#	endif
	op_print(0, op_array->opcodes, op_array->opcodes + op_array->last);
#endif

	if (op_array_convert_switch(op_array) == SUCCESS) {
		bbs_init(&bbs);
		if (bbs_build_from(&bbs, op_array, op_array->last) == SUCCESS) {
			int i;
#ifdef DEBUG
			bbs_print(&bbs, op_array->opcodes);
#endif
			/* TODO: calc opnum after basic block move */
			for (i = 0; i < bbs_count(&bbs); i ++) {
				bb_t *bb = bbs_get(&bbs, i);
				bb->opnum = bb->opcodes - op_array->opcodes;
			}
			bbs_restore_opnum(&bbs, op_array);
		}
		bbs_destroy(&bbs);
	}

#ifdef DEBUG
#	if 0
	TRACE("%s", "after compiles");
	xc_dprint_zend_op_array(op_array, 0 TSRMLS_CC);
#	endif
#endif
	return 0;
}
/* }}} */
int xc_clear_flag_optimized(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	op_array->done_pass_two = 0;
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
	/* op_array->done_pass_two is now used as if it's op_array->flag_optimized */
	xc_apply_op_array(&cr, (apply_func_t) xc_optimize_op_array TSRMLS_CC);
	xc_apply_op_array(&cr, (apply_func_t) xc_clear_flag_optimized TSRMLS_CC);
	xc_apply_op_array(&cr, (apply_func_t) xc_redo_pass_two TSRMLS_CC);

	xc_compile_result_free(&cr);
}
/* }}} */
