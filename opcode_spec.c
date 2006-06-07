#include "xcache.h"
#include "opcode_spec.h"
#include "const_string.h"

#ifdef HAVE_XCACHE_OPCODE_SPEC_DEF
/* {{{ opcode_spec */
#define OPSPEC(ext, op1, op2, res) { OPSPEC_##ext, OPSPEC_##op1, OPSPEC_##op2, OPSPEC_##res },
#ifdef ZEND_ENGINE_2
#	define OPSPEC_VAR_2 OPSPEC_STD
#else
#	define OPSPEC_VAR_2 OPSPEC_VAR
#endif
#include "opcode_spec_def.h"

zend_uchar xc_get_opcode_spec_count()
{
	return sizeof(xc_opcode_spec) / sizeof(xc_opcode_spec[0]);
}

const xc_opcode_spec_t *xc_get_opcode_spec(zend_uchar opcode)
{
	assert(xc_get_opcode_count() == xc_get_opcode_spec_count());
	assert(opcode < xc_get_opcode_spec_count());
	return &xc_opcode_spec[opcode];
}
/* }}} */
#endif
/* {{{ op_spec */

#define OPSPECS_DEF_NAME(name) #name,
static const char *xc_op_spec[] = { OPSPECS(OPSPECS_DEF_NAME) };

zend_uchar xc_get_op_spec_count()
{
	return sizeof(xc_op_spec) / sizeof(xc_op_spec[0]);
}

const char *xc_get_op_spec(zend_uchar spec)
{
	assert(spec < xc_get_op_spec_count());
	return xc_op_spec[spec];
}
/* }}} */
