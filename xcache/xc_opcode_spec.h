#include "php.h"

#define OPSPECS(OPSPEC) \
	OPSPEC(STD) \
	OPSPEC(UNUSED) \
	OPSPEC(OPLINE) \
	OPSPEC(FCALL) \
	OPSPEC(INIT_FCALL) \
	OPSPEC(ARG) \
	OPSPEC(CAST) \
	OPSPEC(FETCH) \
	OPSPEC(DECLARE) \
	OPSPEC(SEND) \
	OPSPEC(SEND_NOREF) \
	OPSPEC(FCLASS) \
	OPSPEC(UCLASS) \
	OPSPEC(CLASS) \
	OPSPEC(FE) \
	OPSPEC(IFACE) \
	OPSPEC(ISSET) \
	OPSPEC(BIT) \
	OPSPEC(VAR) \
	OPSPEC(TMP) \
	OPSPEC(JMPADDR) \
	OPSPEC(BRK) \
	OPSPEC(CONT) \
	OPSPEC(INCLUDE) \
	OPSPEC(ASSIGN) \
	OPSPEC(FETCHTYPE)

#define OPSPECS_DEF_ENUM(name) OPSPEC_##name,
typedef enum { OPSPECS(OPSPECS_DEF_ENUM) OPSPEC_DUMMY } xc_op_spec_t;

typedef struct {
	xc_op_spec_t ext;
	xc_op_spec_t op1;
	xc_op_spec_t op2;
	xc_op_spec_t res;
} xc_opcode_spec_t;

const xc_opcode_spec_t *xc_get_opcode_spec(zend_uchar opcode);
zend_uchar xc_get_opcode_spec_count();
zend_uchar xc_get_op_spec_count();
const char *xc_get_op_spec(zend_uchar spec);
