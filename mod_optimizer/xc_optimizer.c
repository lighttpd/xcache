#if 0
#	define XCACHE_DEBUG
#endif

#include "xc_optimizer.h"
#include "xcache/xc_extension.h"
#include "xcache/xc_ini.h"
#include "xcache/xc_utils.h"
#include "util/xc_stack.h"
#include "util/xc_trace.h"
#include "xcache_globals.h"

#include "ext/standard/info.h"

#ifdef XCACHE_DEBUG
#	include "xc_processor.h"
#	include "xcache/xc_const_string.h"
#	include "xcache/xc_opcode_spec.h"
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
#	define Z_OP_CONSTANT(op) op_array->literals[(op).constant].constant
#endif

typedef zend_uint bbid_t;
#define BBID_INVALID ((bbid_t) -1)
/* {{{ basic block */
typedef struct _bb_t {
	bbid_t     id;
	zend_bool  used;

	zend_bool  alloc;
	zend_op   *opcodes;
	int        count;
	int        size;

	bbid_t     fall;

	zend_uint  opnum; /* opnum after joining basic block */
} bb_t;
/* }}} */

/* basic blocks */
typedef xc_stack_t bbs_t;

/* op array helper functions */
static int op_array_convert_switch(zend_op_array *op_array) /* {{{ */
{
	zend_uint i;

	if (op_array->brk_cont_array == NULL) {
		return SUCCESS;
	}

	for (i = 0; i < op_array->last; i ++) {
		zend_op *opline = &op_array->opcodes[i];
		zend_brk_cont_element *jmp_to;
		zend_bool can_convert = 1;
		int array_offset, nest_levels;

		switch (opline->opcode) {
		case ZEND_BRK:
		case ZEND_CONT:
			break;

#ifdef ZEND_GOTO
		case ZEND_GOTO:
#endif
			continue;

		default:
			continue;
		}

		if (Z_OP_TYPE(opline->op2) != IS_CONST
		 || Z_OP_CONSTANT(opline->op2).type != IS_LONG) {
			return FAILURE;
		}

		nest_levels = Z_OP_CONSTANT(opline->op2).value.lval;

		array_offset = Z_OP(opline->op1).opline_num;
		do {
			if (array_offset == -1) {
				return FAILURE;
			}
			jmp_to = &op_array->brk_cont_array[array_offset];
			if (nest_levels > 1) {
				zend_op *brk_opline = &op_array->opcodes[jmp_to->brk];

				switch (brk_opline->opcode) {
				case ZEND_SWITCH_FREE:
				case ZEND_FREE:
#ifdef EXT_TYPE_FREE_ON_RETURN
					if (!(brk_opline->extended_value & EXT_TYPE_FREE_ON_RETURN))
#endif
					{
						can_convert = 0;
					}
					break;
				}
			}
			array_offset = jmp_to->parent;
		} while (--nest_levels > 0);

		if (can_convert) {
			/* rewrite to jmp */
			switch (opline->opcode) {
			case ZEND_BRK:
				Z_OP(opline->op1).opline_num = jmp_to->brk;
				break;

			case ZEND_CONT:
				Z_OP(opline->op1).opline_num = jmp_to->cont;
				break;
			}
			Z_OP_TYPE(opline->op2) = IS_UNUSED;
			opline->opcode = ZEND_JMP;
		}
	}

	return SUCCESS;
}
/* }}} */
/* {{{ op_flowinfo helper func */
enum {
	XC_OPNUM_INVALID = -1
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

	switch (opline->opcode) {
#ifdef ZEND_HANDLE_EXCEPTION
	case ZEND_HANDLE_EXCEPTION:
#endif
	case ZEND_RETURN:
#ifdef ZEND_FAST_RET
	case ZEND_FAST_RET:
#endif
#ifdef ZEND_GENERATOR_RETURN
	case ZEND_GENERATOR_RETURN:
#endif
	case ZEND_EXIT:
		return SUCCESS; /* no fall */

#ifdef ZEND_GOTO
	case ZEND_GOTO:
#endif
	case ZEND_JMP:
		fi->jmpout_op1 = Z_OP(opline->op1).opline_num;
		return SUCCESS; /* no fall */

#ifdef ZEND_FAST_CALL
	case ZEND_FAST_CALL:
		fi->jmpout_op1 = Z_OP(opline->op1).opline_num;
		if (opline->extended_value) {
			fi->jmpout_op2 = Z_OP(opline->op2).opline_num;
		}
		return SUCCESS; /* no fall */
#endif

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
#ifdef ZEND_JMP_SET_VAR
	case ZEND_JMP_SET_VAR:
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
		fi->fall = 1;
		return SUCCESS;

#ifdef ZEND_CATCH
	case ZEND_CATCH:
		fi->jmpout_ext = (int) opline->extended_value;
		fi->fall = 1;
		return SUCCESS;
#endif

	default:
		return FAILURE;
	}
}
/* }}} */
#ifdef XCACHE_DEBUG
static void op_snprint(zend_op_array *op_array, char *buf, int size, zend_uchar op_type, znode_op *op, xc_op_spec_t op_spec) /* {{{ */
{
	switch ((op_spec & OPSPEC_UNUSED) ? IS_UNUSED : op_type) {
	case IS_CONST:
		{
			zval result;
			zval *zv = &Z_OP_CONSTANT(*op);
			TSRMLS_FETCH();

			php_output_start_default(TSRMLS_C);
			php_var_export(&zv, 1 TSRMLS_CC);
			php_output_get_contents(&result TSRMLS_CC); 
			php_output_discard(TSRMLS_C);

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
			snprintf(buf, size, "u#%d", Z_OP(*op).opline_num);
		}
		else {
			snprintf(buf, size, "-");
		}
		break;

	default:
		snprintf(buf, size, "%d %d", op_type, Z_OP(*op).var);
	}
}
/* }}} */
static void op_print(zend_op_array *op_array, int line, zend_op *first, zend_op *end) /* {{{ */
{
	zend_op *opline;
	for (opline = first; opline < end; opline ++) {
		char buf_r[20];
		char buf_1[20];
		char buf_2[20];
		const xc_opcode_spec_t *opcode_spec = xc_get_opcode_spec(opline->opcode);
		op_snprint(op_array, buf_r, sizeof(buf_r), Z_OP_TYPE(opline->result), &opline->result, opcode_spec->res);
		op_snprint(op_array, buf_1, sizeof(buf_1), Z_OP_TYPE(opline->op1),    &opline->op1,    opcode_spec->op1);
		op_snprint(op_array, buf_2, sizeof(buf_2), Z_OP_TYPE(opline->op2),    &opline->op2,    opcode_spec->op2);
		fprintf(stderr,
				"%3d %3lu"
				" %-25s%-8s%-20s%-20s%5lu\n"
				, opline->lineno, (long) (opline - first + line)
				, xc_get_opcode(opline->opcode), buf_r, buf_1, buf_2, opline->extended_value);
	}
}
/* }}} */
static void op_array_print_try_catch(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	int i;
	for (i = 0; i < op_array->last_try_catch; i ++) {
		zend_try_catch_element *element = &op_array->try_catch_array[i];
#	ifdef ZEND_ENGINE_2_5
		fprintf(stderr, "try_catch[%d] = %u, %u, %u, %u\n", i, element->try_op, element->catch_op, element->finally_op, element->finally_end);
#	else
		fprintf(stderr, "try_catch[%d] = %u, %u\n", i, element->try_op, element->catch_op);
#	endif
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
static void bb_print(bb_t *bb, zend_op_array *op_array) /* {{{ */
{
	int line = bb->opcodes - op_array->opcodes;
	op_flowinfo_t fi;
	zend_op *last = bb->opcodes + bb->count - 1;

	op_get_flowinfo(&fi, last);

	fprintf(stderr,
			"\n==== #%-3d cnt:%-3d lno:%-3d"
			" %c%c"
			" op1:%-3d op2:%-3d ext:%-3d fal:%-3d %s ====\n"
			, bb->id, bb->count, bb->alloc ? -1 : line
			, bb->used ? 'U' : ' ', bb->alloc ? 'A' : ' '
			, fi.jmpout_op1, fi.jmpout_op2, fi.jmpout_ext, bb->fall, xc_get_opcode(last->opcode)
			);
	op_print(op_array, line, bb->opcodes, last + 1);
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
static void bbs_print(bbs_t *bbs, zend_op_array *op_array) /* {{{ */
{
	int i;
	for (i = 0; i < xc_stack_count(bbs); i ++) {
		bb_print(bbs_get(bbs, i), op_array);
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
	bb_t *bb;
	bbid_t id;
	op_flowinfo_t fi;
	zend_op *opline;
	ALLOCA_FLAG(opline_infos_use_heap)
	typedef struct {
		zend_bool isbbhead;
		bbid_t bbid;
	} oplineinfo_t;
	oplineinfo_t *oplineinfos = xc_do_alloca(count * sizeof(oplineinfo_t), opline_infos_use_heap);

	memset(oplineinfos, 0, count * sizeof(oplineinfo_t));

	/* {{{ mark jmpin/jumpout */
	oplineinfos[0].isbbhead = 1;
	for (i = 0; i < count; i ++) {
		if (op_get_flowinfo(&fi, &op_array->opcodes[i]) == SUCCESS) {
			if (fi.jmpout_op1 != XC_OPNUM_INVALID) {
				oplineinfos[fi.jmpout_op1].isbbhead = 1;
			}
			if (fi.jmpout_op2 != XC_OPNUM_INVALID) {
				oplineinfos[fi.jmpout_op2].isbbhead = 1;
			}
			if (fi.jmpout_ext != XC_OPNUM_INVALID) {
				oplineinfos[fi.jmpout_ext].isbbhead = 1;
			}
			if (i + 1 < count) {
				oplineinfos[i + 1].isbbhead = 1;
			}
		}
	}
#ifdef ZEND_ENGINE_2
	/* mark try start */
	for (i = 0; i < op_array->last_try_catch; i ++) {
#	define MARK_OP_BB_HEAD(name) \
		oplineinfos[op_array->try_catch_array[i].name].isbbhead = 1
		MARK_OP_BB_HEAD(try_op);
		MARK_OP_BB_HEAD(catch_op);
#	ifdef ZEND_ENGINE_2_5
		MARK_OP_BB_HEAD(finally_op);
#	endif
#	undef MARK_OP_BB_HEAD
	}
#endif
	/* }}} */
	/* {{{ fill op lines with newly allocated id */
	for (i = 0; i < count; i ++) {
		oplineinfos[i].bbid = BBID_INVALID;
	}

	id = -1;
	for (i = 0; i < count; i ++) {
		if (oplineinfos[i].isbbhead) {
			id ++;
		}
		oplineinfos[i].bbid = id;
		TRACE("bbids[%d] = %d", i, id);
	}
	/* }}} */
#ifdef ZEND_ENGINE_2
	/* {{{ convert try_catch_array.* from oplinenum to bbid */
	for (i = 0; i < op_array->last_try_catch; i ++) {
#	define OPNUM_TO_BBID(name) \
		op_array->try_catch_array[i].name = oplineinfos[op_array->try_catch_array[i].name].bbid;
		OPNUM_TO_BBID(try_op);
		OPNUM_TO_BBID(catch_op);
#	ifdef ZEND_ENGINE_2_5
		OPNUM_TO_BBID(finally_op);
#	endif
#	undef OPNUM_TO_BBID
	}
	/* }}} */
#endif
	/* {{{ create basic blocks */
	start = 0;
	id = 0;
	/* loop over to deal with the last block */
	for (i = 1; i <= count; i ++) {
		if (i < count && id == oplineinfos[i].bbid) {
			continue;
		}

		opline = op_array->opcodes + start;
		bb = bbs_new_bb_ex(bbs, opline, i - start);

		/* last */
		opline = bb->opcodes + bb->count - 1;
		if (op_get_flowinfo(&fi, opline) == SUCCESS) {
			if (fi.jmpout_op1 != XC_OPNUM_INVALID) {
				Z_OP(opline->op1).opline_num = oplineinfos[fi.jmpout_op1].bbid;
				assert(Z_OP(opline->op1).opline_num != BBID_INVALID);
			}
			if (fi.jmpout_op2 != XC_OPNUM_INVALID) {
				Z_OP(opline->op2).opline_num = oplineinfos[fi.jmpout_op2].bbid;
				assert(Z_OP(opline->op2).opline_num != BBID_INVALID);
			}
			if (fi.jmpout_ext != XC_OPNUM_INVALID) {
				opline->extended_value = oplineinfos[fi.jmpout_ext].bbid;
				assert(opline->extended_value != BBID_INVALID);
			}
			if (fi.fall && i + 1 < count) {
				bb->fall = oplineinfos[i + 1].bbid;
				TRACE("fall %d", bb->fall);
				assert(bb->fall != BBID_INVALID);
			}
		}
		if (i >= count) {
			break;
		}
		start = i;
		id = oplineinfos[i].bbid;
	}
	/* }}} */

	xc_free_alloca(oplineinfos, opline_infos_use_heap);
	return SUCCESS;
}
/* }}} */
static void bbs_restore_opnum(bbs_t *bbs, zend_op_array *op_array) /* {{{ */
{
	int bbid;
	int i;

	for (bbid = 0; bbid < bbs_count(bbs); bbid ++) {
		op_flowinfo_t fi;
		bb_t *bb = bbs_get(bbs, bbid);
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
	/* {{{ convert try_catch_array from bbid to oplinenum */
	for (i = 0; i < op_array->last_try_catch; i ++) {
#	define BBID_TO_OPNUM(name) \
		op_array->try_catch_array[i].name = bbs_get(bbs, op_array->try_catch_array[i].name)->opnum;
		BBID_TO_OPNUM(try_op);
		BBID_TO_OPNUM(catch_op);
#	ifdef ZEND_ENGINE_2_5
		BBID_TO_OPNUM(finally_op);
#	endif
#	undef BBID_TO_OPNUM
	}
	/* }}} */
#endif /* ZEND_ENGINE_2 */
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
	TRACE("optimize file: %s", op_array->filename);
#	if 0 && HAVE_XCACHE_DPRINT
	xc_dprint_zend_op_array(op_array, 0 TSRMLS_CC);
#	endif
	op_array_print_try_catch(op_array TSRMLS_CC);
	op_print(op_array, 0, op_array->opcodes, op_array->opcodes + op_array->last);
#endif

	if (op_array_convert_switch(op_array) == SUCCESS) {
		bbs_init(&bbs);
		if (bbs_build_from(&bbs, op_array, op_array->last) == SUCCESS) {
			int i;
#ifdef XCACHE_DEBUG
			bbs_print(&bbs, op_array);
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
	TRACE("%s", "after compiles");
#	if 0
	xc_dprint_zend_op_array(op_array, 0 TSRMLS_CC);
#	endif
	op_array_print_try_catch(op_array TSRMLS_CC);
	op_print(op_array, 0, op_array->opcodes, op_array->opcodes + op_array->last);
#endif
	return 0;
}
/* }}} */
static void xc_optimizer_op_array_handler(zend_op_array *op_array) /* {{{ */
{
	TSRMLS_FETCH();
	if (XG(optimizer)) {
		xc_optimize_op_array(op_array TSRMLS_CC);
	}
}
/* }}} */

static int xc_coverager_zend_startup(zend_extension *extension) /* {{{ */
{
	return SUCCESS;
}
/* }}} */
static void xc_coverager_zend_shutdown(zend_extension *extension) /* {{{ */
{
}
/* }}} */
/* {{{ zend extension definition structure */
static zend_extension xc_optimizer_zend_extension_entry = {
	XCACHE_NAME " Optimizer",
	XCACHE_VERSION,
	XCACHE_AUTHOR,
	XCACHE_URL,
	XCACHE_COPYRIGHT,
	xc_coverager_zend_startup,
	xc_coverager_zend_shutdown,
	NULL,           /* activate_func_t */
	NULL,           /* deactivate_func_t */
	NULL,           /* message_handler_func_t */
	xc_optimizer_op_array_handler,
	NULL,           /* statement_handler_func_t */
	NULL,           /* fcall_begin_handler_func_t */
	NULL,           /* fcall_end_handler_func_t */
	NULL,           /* op_array_ctor_func_t */
	NULL,           /* op_array_dtor_func_t */
	STANDARD_ZEND_EXTENSION_PROPERTIES
};
/* }}} */

/* {{{ ini */
PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("xcache.optimizer",              "0", PHP_INI_ALL,    OnUpdateBool,        optimizer,         zend_xcache_globals, xcache_globals)
PHP_INI_END()
/* }}} */
static PHP_MINFO_FUNCTION(xcache_optimizer) /* {{{ */
{
	php_info_print_table_start();
	php_info_print_table_row(2, "XCache Optimizer Module", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */
static PHP_MINIT_FUNCTION(xcache_optimizer) /* {{{ */
{
	REGISTER_INI_ENTRIES();
	return xcache_zend_extension_add(&xc_optimizer_zend_extension_entry, 0);
}
/* }}} */
static PHP_MSHUTDOWN_FUNCTION(xcache_optimizer) /* {{{ */
{
	UNREGISTER_INI_ENTRIES();
	return xcache_zend_extension_remove(&xc_optimizer_zend_extension_entry);
}
/* }}} */
static zend_module_entry xcache_optimizer_module_entry = { /* {{{ */
	STANDARD_MODULE_HEADER,
	XCACHE_NAME " Optimizer",
	NULL,
	PHP_MINIT(xcache_optimizer),
	PHP_MSHUTDOWN(xcache_optimizer),
	NULL,
	NULL,
	PHP_MINFO(xcache_optimizer),
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
int xc_optimizer_startup_module() /* {{{ */
{
	return zend_startup_module(&xcache_optimizer_module_entry);
}
/* }}} */
