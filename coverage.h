#include "php.h"
#include "xcache.h"

extern char *xc_coveragedump_dir;

void xc_coverage_handle_ext_stmt(zend_op_array *op_array, zend_uchar op);
int xc_coverage_init(int module_number TSRMLS_DC);
void xc_coverage_destroy();
void xc_coverage_request_init(TSRMLS_D);
void xc_coverage_request_shutdown(TSRMLS_D);
PHP_FUNCTION(xcache_coverage_decode);
