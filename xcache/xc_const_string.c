#include "xcache.h"
#include "xc_const_string.h"

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
#ifdef ZEND_ENGINE_2_1
	/* 3 */ "IS_BOOL",
#else
	/* 6 */ "IS_STRING",
#endif
	/* 4 */ "IS_ARRAY",
	/* 5 */ "IS_OBJECT",
#ifdef ZEND_ENGINE_2_1
	/* 6 */ "IS_STRING",
#else
	/* 3 */ "IS_BOOL",
#endif
	/* 7 */ "IS_RESOURCE",
	/* 8 */ "IS_CONSTANT",
	/* 9 */ "IS_CONSTANT_ARRAY",
	/* 10 */ "IS_UNICODE"
};

zend_uchar xc_get_data_type_count()
{
	return sizeof(data_type_names) / sizeof(data_type_names[0]);
}

const char *xc_get_data_type(zend_uchar data_type)
{
	return data_type_names[(data_type & IS_CONSTANT_TYPE_MASK)];
}
/* }}} */
/* {{{ xc_get_opcode */
#if PHP_MAJOR_VERSION >= 6
#	include "xc_const_string_opcodes_php6.x.h"
#elif defined(ZEND_ENGINE_2_6)
#	include "xc_const_string_opcodes_php5.6.h"
#elif defined(ZEND_ENGINE_2_5)
#	include "xc_const_string_opcodes_php5.5.h"
#elif defined(ZEND_ENGINE_2_4)
#	include "xc_const_string_opcodes_php5.4.h"
#elif defined(ZEND_ENGINE_2_3)
#	include "xc_const_string_opcodes_php5.3.h"
#elif defined(ZEND_ENGINE_2_2)
#	include "xc_const_string_opcodes_php5.2.h"
#elif defined(ZEND_ENGINE_2_1)
#	include "xc_const_string_opcodes_php5.1.h"
#elif defined(ZEND_ENGINE_2)
#	include "xc_const_string_opcodes_php5.0.h"
#else
#	include "xc_const_string_opcodes_php4.x.h"
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
