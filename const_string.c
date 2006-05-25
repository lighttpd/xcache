#include "xcache.h"
#include "const_string.h"

/* {{{ xc_get_op_type */
static const char *const op_type_names[] = {
	/* 0 */ "NULL?",
	/* 1 */ "IS_CONST",
	/* 2 */ "IS_TMP_VAR",
	/* 3 */ NULL,
	/* 4 */ "IS_VAR",
	/* 5 */ NULL,
	/* 6 */ NULL,
	/* 7 */ NULL,
	/* 8 */ "IS_UNUSED",
#ifdef IS_CV
	/* 9  */ NULL,
	/* 10 */ NULL,
	/* 11 */ NULL,
	/* 12 */ NULL,
	/* 13 */ NULL,
	/* 14 */ NULL,
	/* 15 */ NULL,
	/* 16 */ "IS_CV"
#endif
};

zend_uchar xc_get_op_type_count()
{
	return sizeof(op_type_names) / sizeof(op_type_names[0]);
}

const char *xc_get_op_type(zend_uchar op_type)
{
	assert(op_type < xc_get_op_type_count());
	return op_type_names[op_type];
}
/* }}} */
/* {{{ xc_get_data_type */
static const char *const data_type_names[] = {
	/* 0 */ "IS_NULL",
	/* 1 */ "IS_LONG",
	/* 2 */ "IS_DOUBLE",
	/* 3 */ "IS_BOOL",
	/* 4 */ "IS_ARRAY",
	/* 5 */ "IS_OBJECT",
	/* 6 */ "IS_STRING",
	/* 7 */ "IS_RESOURCE",
	/* 8 */ "IS_CONSTANT",
	/* 9 */ "IS_CONSTANT_ARRAY",
	/* 10 */ "IS_UNICODE",
#if 0
	/* 11 */ "",
	/* 12 */ "",
	/* 13 */ "",
	/* 14 */ "",
	/* 15 */ "", "", "", "", "",

/* IS_CONSTANT_INDEX */
	/* 20 */ "CIDX IS_NULL",
	/* 21 */ "CIDX IS_LONG",
	/* 22 */ "CIDX IS_DOUBLE",
	/* 23 */ "CIDX IS_BOOL",
	/* 24 */ "CIDX IS_ARRAY",
	/* 25 */ "CIDX IS_OBJECT",
	/* 26 */ "CIDX IS_STRING",
	/* 27 */ "CIDX IS_RESOURCE",
	/* 28 */ "CIDX IS_CONSTANT",
	/* 29 */ "CIDX IS_CONSTANT_ARRAY"
	/* 20 */ "CIDX IS_UNICODE",
#endif
};

zend_uchar xc_get_data_type_count()
{
	return sizeof(data_type_names) / sizeof(data_type_names[0]);
}

const char *xc_get_data_type(zend_uchar data_type)
{
#if 0
	if (data_type & IS_CONSTANT_INDEX) {
		data_type = (data_type & ~IS_CONSTANT_INDEX) + 20;
	}
#endif
	data_type &= ~IS_CONSTANT_INDEX;
	return data_type_names[data_type];
}
/* }}} */
/* {{{ xc_get_opcode */
#if PHP_MAJAR_VERSION >= 6
#	include     "const_string_opcodes_php6.x.h"
#else
#	ifdef ZEND_ENGINE_2_1
#		include     "const_string_opcodes_php5.1.h"
#	else
#		ifdef ZEND_ENGINE_2
#			include "const_string_opcodes_php5.0.h"
#		else
#			include "const_string_opcodes_php4.x.h"
#		endif
#	endif
#endif

zend_uchar xc_get_opcode_count()
{
	return sizeof(xc_opcode_names) / sizeof(xc_opcode_names[0]);
}

const char *xc_get_opcode(zend_uchar opcode)
{
	assert(opcode < xc_get_opcode_count());
	return xc_opcode_names[opcode];
}
/* }}} */
