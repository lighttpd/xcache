static zend_class_entry *xc_lookup_class(const char *class_name, int class_name_len TSRMLS_DC) /* {{{ */
{
	xc_cest_t *cest;
#ifdef ZEND_ENGINE_2
	if (zend_lookup_class_ex(class_name, class_name_len, NULL, 0, &cest TSRMLS_CC) != SUCCESS) {
		return NULL;
	}
#else
	if (zend_hash_find(EG(class_table), class_name, class_name_len, (void **) &cest) != SUCCESS) {
		return NULL;
	}
#endif
	return CestToCePtr(*cest);
}
/* }}} */
