#include "php.h"
#include "xcache.h"

void xc_coverager_handle_ext_stmt(zend_op_array *op_array, zend_uchar op);
int xc_coverager_module_init(int module_number TSRMLS_DC);
void xc_coverager_module_shutdown();
void xc_coverager_request_init(TSRMLS_D);
void xc_coverager_request_shutdown(TSRMLS_D);
PHP_FUNCTION(xcache_coverager_decode);
PHP_FUNCTION(xcache_coverager_start);
PHP_FUNCTION(xcache_coverager_stop);
PHP_FUNCTION(xcache_coverager_get);
