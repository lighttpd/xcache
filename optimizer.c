#if 0
#	define XCACHE_DEBUG
#endif

#include "utils.h"
#include "optimizer.h"
/* the "vector" stack */
#include "stack.h"
#include "xcache_globals.h"

#ifdef XCACHE_DEBUG
#	include "processor.h"
#	include "const_string.h"
#	include "ext/standard/php_var.h"
#endif

#ifdef IS_CV
#	define XCACHE_IS_CV IS_CV
#else
#	define XCACHE_IS_CV 16
#endif

#ifdef ZEND_ENGINE_2_4
#	undef Z_OP_CONSTANT
/* Z_OP_CONSTANT is used before pass_two is applied */
#	define Z_OP_CONSTANT(op) op_array->literals[op.constant].constant
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

	bbid_t     fall;
#ifdef ZEND_ENGINE_2
	bbid_t     catch;
#endif

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
		if (Z_OP_TYPE(opline->op2) != IS_CONST
		 || Z_OP_CONSTANT(opline->op2).type != IS_LONG) {
			return FAILURE;
		}

		nest_levels = Z_OP_CONSTANT(opline->op2).value.lval;
		original_nest_levels = nest_levels;

		array_offset = Z_OP(opline->op1).opline_num;
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
			Z_OP(opline->op1).opline_num = jmp_to->brk;
		}
		else {
			Z_OP(opline->op1).opline_num = jmp_to->cont;
		}
		Z_OP_TYPE(opline->op2) = IS_UNUSED;
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
#ifdef ZEND_HANDLE_EXCEPTION
	case ZEND_HANDLE_EXCEPTION:
#endif
	case ZEND_RETURN:
	case ZEND_EXIT:
		return SUCCESS; /* no fall */

	case ZEND_JMP:
		fi->jmpout_op1 = Z_OP(opline->op1).opline_num;
		return SUCCESS; /* no fall */

	case ZEND_JMPZNZ:
		fi->jmpout_op2 = Z_OP(opline->op2).opline_num;
		fi->jmpout_ext = (int) opline->extended_value;
		return SUCCESS; /* no fall */

	case ZEND_JMPZ:
	case ZEND_JMPNZ:
	case ZEND_JMPZ_EX:
	case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_SET
	case ZEND_JMP_SET:
#endif
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
		fi->jmpout_op2 = Z_OP(opline->op2).opline_num;
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
#ifdef XCACHE_DEBUG
static void op_snprint(char *buf, int size, zend_uchar op_type, znode_op *op) /* {{{ */
{
	switch (op_type) {
	case IS_CONST:
		{
			zval result;
			zval *zv = &Z_OP_CONSTANT(*op);
			TSRMLS_FETCH();

			/* TODO: update for PHP 6 */
			php_start_ob_buffer(NULL, 0, 1 TSRMLS_CC);
			php_var_export(&zv, 1 TSRMLS_CC);

			php_ob_get_buffer(&result TSRMLS_CC); 
			php_end_ob_buffer(0, 0 TSRMLS_CC);
			snprintf(buf, size, Z_STRVAL(result));
			zval_dtor(&result);
		}
		break;

	case IS_TMP_VAR:
		snprintf(buf, size, "t@%d", Z_OP(*op).var);
		break;

	case XCACHE_IS_CV:
	case IS_VAR:
		snprintf(buf, size, "v@%d", Z_OP(*op).var);
		break;

	case IS_UNUSED:
		if (Z_OP(*op).opline_num) {
			snprintf(buf, size, "u#%d", Z_OP(op).opline_num);
		}
		else {
			snprintf(buf, size, "-");
		}
		break;

	default:
		snprintf(buf, size, "%d %d", op->op_type, Z_OP(op).var);
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
		op_snprint(buf_r, sizeof(buf_r), Z_OP_TYPE(opline->result), &opline->result);
		op_snprint(buf_1, sizeof(buf_1), Z_OP_TYPE(opline->op1),    &opline->op1);
		op_snprint(buf_2, sizeof(buf_2), Z_OP_TYPE(opline->op2),    &opline->op2);
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
#ifdef ZEND_ENGINE_2
	bb->catch      = BBID_INVALID;
#endif

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
#ifdef XCACHE_DEBUG
static void bb_print(bb_t *bb, zend_op *opcodes) /* {{{ */
{
	int line = bb->opcodes - opcodes;
	op_flowinfo_t fi;
	zend_op *last = bb->opcodes + bb->count - 1;
	bbid_t catchbbid;
#ifdef ZEND_ENGINE_2
	catchbbid = BBID_INVALID;
#else
	catchbbid = bb->catch;
#endif

	op_get_flowinfo(&fi, last);

	fprintf(stderr,
			"\r\n==== #%-3d cnt:%-3d lno:%-3d"
			" %c%c"
			" op1:%-3d op2:%-3d ext:%-3d fal:%-3d cat:%-3d %s ====\r\n"
			, bb->id, bb->count, bb->alloc ? -1 : line
			, bb->used ? 'U' : ' ', bb->alloc ? 'A' : ' '
			, fi.jmpout_op1, fi.jmpout_op2, fi.jmpout_ext, bb->fall, catchbbid, xc_get_opcode(last->opcode)
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
#ifdef XCACHE_DEBUG
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
	ALLOCA_FLAG(use_heap_bbids)
	ALLOCA_FLAG(use_heap_catchbbids)
	ALLOCA_FLAG(use_heap_markbbhead)
	bbid_t *bbids          = my_do_alloca(count * sizeof(bbid_t),    use_heap_bbids);
#ifdef ZEND_ENGINE_2
	bbid_t *catchbbids     = my_do_alloca(count * sizeof(bbid_t),    use_heap_catchbbids);
#endif
	zend_bool *markbbhead  = my_do_alloca(count * sizeof(zend_bool), use_heap_markbbhead);

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
#ifdef ZEND_ENGINE_2
	/* mark try start */
	for (i = 0; i < op_array->last_try_catch; i ++) {
		markbbhead[op_array->try_catch_array[i].try_op] = 1;
	}
#endif
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
#ifdef ZEND_ENGINE_2
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
#ifdef XCACHE_DEBUG
	for (i = 0; i < count; i ++) {
		TRACE("catchbbids[%d] = %d", i, catchbbids[i]);
	}
#endif
	/* }}} */
#endif
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
#ifdef ZEND_ENGINE_2
		pbb->catch = catchbbids[start];
#endif

		/* last */
		opline = pbb->opcodes + pbb->count - 1;
		if (op_get_flowinfo(&fi, opline) == SUCCESS) {
			if (fi.jmpout_op1 != XC_OPNUM_INVALID) {
				Z_OP(opline->op1).opline_num = bbids[fi.jmpout_op1];
				assert(Z_OP(opline->op1).opline_num != BBID_INVALID);
			}
			if (fi.jmpout_op2 != XC_OPNUM_INVALID) {
				Z_OP(opline->op2).opline_num = bbids[fi.jmpout_op2];
				assert(Z_OP(opline->op2).opline_num != BBID_INVALID);
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

	my_free_alloca(markbbhead, use_heap_markbbhead);
#ifdef ZEND_ENGINE_2
	my_free_alloca(catchbbids, use_heap_catchbbids);
#endif
	my_free_alloca(bbids,      use_heap_bbids);
	return SUCCESS;
}
/* }}} */
static void bbs_restore_opnum(bbs_t *bbs, zend_op_array *op_array) /* {{{ */
{
	int i;
#ifdef ZEND_ENGINE_2
	bbid_t lasttrybbid;
	bbid_t lastcatchbbid;
#endif

	for (i = 0; i < bbs_count(bbs); i ++) {
		op_flowinfo_t fi;
		bb_t *bb = bbs_get(bbs, i);
		zend_op *last = bb->opcodes + bb->count - 1;

		if (op_get_flowinfo(&fi, last) == SUCCESS) {
			if (fi.jmpout_op1 != XC_OPNUM_INVALID) {
				Z_OP(last->op1).opline_num = bbs_get(bbs, fi.jmpout_op1)->opnum;
				assert(Z_OP(last->op1).opline_num != BBID_INVALID);
			}
			if (fi.jmpout_op2 != XC_OPNUM_INVALID) {
				Z_OP(last->op2).opline_num = bbs_get(bbs, fi.jmpout_op2)->opnum;
				assert(Z_OP(last->op2).opline_num != BBID_INVALID);
			}
			if (fi.jmpout_ext != XC_OPNUM_INVALID) {
				last->extended_value = bbs_get(bbs, fi.jmpout_ext)->opnum;
				assert(last->extended_value != BBID_INVALID);
			}
		}
	}

#ifdef ZEND_ENGINE_2
	lasttrybbid   = BBID_INVALID;
	lastcatchbbid = BBID_INVALID;
	op_array->last_try_catch = 0;
	for (i = 0; i < bbs_count(bbs); i ++) {
		bb_t *bb = bbs_get(bbs, i);

		if (lastcatchbbid != bb->catch) {
			if (lasttrybbid != BBID_INVALID && lastcatchbbid != BBID_INVALID) {
				int try_catch_offset = op_array->last_try_catch ++;

				op_array->try_catch_array = erealloc(op_array->try_catch_array, sizeof(zend_try_catch_element) * op_array->last_try_catch);
				op_array->try_catch_array[try_catch_offset].try_op = bbs_get(bbs, lasttrybbid)->opnum;
				op_array->try_catch_array[try_catch_offset].catch_op = bbs_get(bbs, lastcatchbbid)->opnum;
			}
			lasttrybbid   = i;
			lastcatchbbid = bb->catch;
		}
	}
	/* it is impossible to have last bb catched */
#endif
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

#ifdef XCACHE_DEBUG
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
#ifdef XCACHE_DEBUG
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

#ifdef XCACHE_DEBUG
#	if 0
	TRACE("%s", "after compiles");
	xc_dprint_zend_op_array(op_array, 0 TSRMLS_CC);
#	endif
	op_print(0, op_array->opcodes, op_array->opcodes + op_array->last);
#endif
	return 0;
}
/* }}} */
void xc_optimizer_op_array_handler(zend_op_array *op_array) /* {{{ */
{
	TSRMLS_FETCH();
	if (XG(optimizer)) {
		xc_optimize_op_array(op_array TSRMLS_CC);
	}
}
/* }}} */
