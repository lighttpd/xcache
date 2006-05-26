#include "php.h"
#include "xcache.h"

extern char *xc_coveragedump_dir;

void xc_coverager_handle_ext_stmt(zend_op_array *op_array, zend_uchar op);
int xc_coverager_init(int module_number TSRMLS_DC);
void xc_coverager_destroy();
void xc_coverager_request_init(TSRMLS_D);
void xc_coverager_request_shutdown(TSRMLS_D);
PHP_FUNCTION(xcache_coverager_decode);
