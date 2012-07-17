#include "php.h"

void xc_dasm_string(zval *return_value, zval *code TSRMLS_DC);
void xc_dasm_file(zval *return_value, const char *filename TSRMLS_DC);
