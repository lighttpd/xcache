#include "php.h"

PHP_FUNCTION(xcache_dasm_file);
PHP_FUNCTION(xcache_dasm_string);
#define XCACHE_DISASSEMBLER_FUNCTIONS() \
	PHP_FE(xcache_dasm_file,         NULL) \
	PHP_FE(xcache_dasm_string,       NULL)
