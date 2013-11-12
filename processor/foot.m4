define(`DEFINE_STORE_API', `
EXPORTED_FUNCTION(`$1 *xc_processor_store_$1(ptrdiff_t relocatediff, xc_allocator_t *allocator, $1 *src TSRMLS_DC)') dnl {{{
{
	$1 *dst;
	xc_processor_t processor;

	memset(&processor, 0, sizeof(processor));
	processor.handle_reference = 1;
	processor.relocatediff = relocatediff;

	IFAUTOCHECK(`xc_stack_init(&processor.allocsizes);')

	/* calc size */ {
		zend_hash_init(&processor.strings, 0, NULL, NULL, 0);
		if (processor.handle_reference) {
			zend_hash_init(&processor.zvalptrs, 0, NULL, NULL, 0);
		}

		processor.size = 0;
		/* allocate */
		processor.size = ALIGN(processor.size + sizeof(src[0]));

		xc_calc_$1(&processor, src TSRMLS_CC);
		if (processor.handle_reference) {
			zend_hash_destroy(&processor.zvalptrs);
		}
		zend_hash_destroy(&processor.strings);
	}
	ifelse(
		`$1', `xc_entry_data_php_t', `SRC(`size')',
		`', `', SRC(`entry.size')) = processor.size;
	ifelse(
		`$1', `xc_entry_var_t', `SRC(`have_references') = processor.have_references;',
		`$1', `xc_entry_data_php_t', `SRC(`have_references') = processor.have_references;'
	)

	IFAUTOCHECK(`xc_stack_reverse(&processor.allocsizes);')
	/* store {{{ */
	{
		IFAUTOCHECK(`char *oldp;')
		zend_hash_init(&processor.strings, 0, NULL, NULL, 0);
		if (processor.handle_reference) {
			zend_hash_init(&processor.zvalptrs, 0, NULL, NULL, 0);
		}

		/* allocator :) */
		processor.p = (char *) allocator->vtable->malloc(allocator, processor.size);
		if (processor.p == NULL) {
			dst = NULL;
			goto err_alloc;
		}
		IFAUTOCHECK(`oldp = processor.p;')
		assert(processor.p == (char *) ALIGN(processor.p));

		/* allocate */
		dst = ($1 *) processor.p;
		processor.p = (char *) ALIGN(processor.p + sizeof(dst[0]));

		xc_store_$1(&processor, dst, src TSRMLS_CC);
		IFAUTOCHECK(` {
			size_t unexpected = processor.p - oldp;
			size_t expected = processor.size;
			if (unexpected != processor.size) {
				fprintf(stderr, "unexpected:%lu - expected:%lu = %ld != 0\n", (unsigned long) unexpected, (unsigned long) expected, (long) unexpected - expected);
				abort();
			}
		}')
err_alloc:
		if (processor.handle_reference) {
			zend_hash_destroy(&processor.zvalptrs);
		}
		zend_hash_destroy(&processor.strings);
	}
	/* }}} */

	IFAUTOCHECK(`xc_stack_destroy(&processor.allocsizes);')

	return dst;
}
dnl }}}
')
DEFINE_STORE_API(`xc_entry_var_t')
DEFINE_STORE_API(`xc_entry_php_t')
DEFINE_STORE_API(`xc_entry_data_php_t')
EXPORTED_FUNCTION(`xc_entry_php_t *xc_processor_restore_xc_entry_php_t(xc_entry_php_t *dst, const xc_entry_php_t *src TSRMLS_DC)') dnl {{{
{
	xc_processor_t processor;

	memset(&processor, 0, sizeof(processor));
	xc_restore_xc_entry_php_t(&processor, dst, src TSRMLS_CC);

	return dst;
}
dnl }}}
EXPORTED_FUNCTION(`xc_entry_data_php_t *xc_processor_restore_xc_entry_data_php_t(const xc_entry_php_t *entry_php, xc_entry_data_php_t *dst, const xc_entry_data_php_t *src, zend_bool readonly_protection TSRMLS_DC)') dnl {{{
{
	xc_processor_t processor;

	memset(&processor, 0, sizeof(processor));
	processor.readonly_protection = readonly_protection;
	/* this function is used for php data only */
	if (SRC(`have_references')) {
		processor.handle_reference = 1;
	}
	processor.entry_php_src = entry_php;

	if (processor.handle_reference) {
		zend_hash_init(&processor.zvalptrs, 0, NULL, NULL, 0);
	}
	xc_restore_xc_entry_data_php_t(&processor, dst, src TSRMLS_CC);
	if (processor.handle_reference) {
		zend_hash_destroy(&processor.zvalptrs);
	}
	return dst;
}
dnl }}}
EXPORTED_FUNCTION(`zval *xc_processor_restore_zval(zval *dst, const zval *src, zend_bool have_references TSRMLS_DC)') dnl {{{
{
	xc_processor_t processor;

	memset(&processor, 0, sizeof(processor));
	processor.handle_reference = have_references;

	if (processor.handle_reference) {
		zend_hash_init(&processor.zvalptrs, 0, NULL, NULL, 0);
		dnl fprintf(stderr, "mark[%p] = %p\n", src, dst);
		zend_hash_add(&processor.zvalptrs, (char *)src, sizeof(src), (void*)&dst, sizeof(dst), NULL);
	}
	xc_restore_zval(&processor, dst, src TSRMLS_CC);
	if (processor.handle_reference) {
		zend_hash_destroy(&processor.zvalptrs);
	}

	return dst;
}
dnl }}}
define(`DEFINE_RELOCATE_API', `
/* src = readable element, before memcpy if any
 * dst = writable element, after memcpy if any
 * virtual_src = brother pointers relatived to this address, before relocation
 * virtual_dst = brother pointers relatived to this address, after  relocation
 */
EXPORTED_FUNCTION(`void xc_processor_relocate_$1($1 *dst, $1 *virtual_dst, $1 *src, $1 *virtual_src TSRMLS_DC)') dnl {{{
{
	char *old_address = 0; /* unkown X used later */
	ptrdiff_t offset = ptrsub(old_address, (ptrdiff_t) virtual_src);

	/* diff to new_ptr */
	ptrdiff_t ptrdiff = ptrsub(dst, virtual_src);
	ptrdiff_t relocatediff = (ptrdiff_t) ptradd($1 *, virtual_dst, offset);

	xc_relocate_$1(dst, ptrdiff, relocatediff TSRMLS_CC);
}
dnl }}}
')
DEFINE_RELOCATE_API(`xc_entry_var_t')
DEFINE_RELOCATE_API(`xc_entry_php_t')
DEFINE_RELOCATE_API(`xc_entry_data_php_t')
EXPORTED(`#ifdef HAVE_XCACHE_DPRINT')
EXPORTED_FUNCTION(`void xc_dprint(xc_entry_php_t *src, int indent TSRMLS_DC)') dnl {{{
{
	IFDPRINT(`INDENT()`'fprintf(stderr, "xc_entry_php_t:src");')
	xc_dprint_xc_entry_php_t(src, indent TSRMLS_CC);
}
dnl }}}
EXPORTED(`#endif')
