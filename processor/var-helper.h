#ifdef ZEND_ENGINE_2
static void xc_var_collect_object(xc_processor_t *processor, zend_object_handle handle TSRMLS_DC) /* {{{ */
{
	size_t next_index;

	if (!xc_vector_initialized(&processor->objects)) {
		xc_vector_init(zend_object, &processor->objects, 0);
		zend_hash_init(&processor->handle_to_index, 0, NULL, NULL, 0);
	}

	next_index = xc_vector_size(&processor->objects);
	if (_zend_hash_index_update_or_next_insert(&processor->handle_to_index, handle, (void *) &next_index, sizeof(next_index), NULL, HASH_ADD ZEND_FILE_LINE_CC) == SUCCESS) {
		zend_object *object = zend_object_store_get_object_by_handle(handle TSRMLS_CC);
		xc_vector_push_back(&processor->objects, object);
	}
}
/* }}} */
static size_t xc_var_store_handle(xc_processor_t *processor, zend_object_handle handle TSRMLS_DC) /* {{{ */
{
	size_t *index;

	if (zend_hash_index_find(&processor->handle_to_index, handle, (void **) &index) != SUCCESS) {
		php_error_docref(NULL TSRMLS_CC, E_CORE_ERROR, "Internal error: handle not found in objects");
		return (size_t) -1;
	}

	return *index;
}
/* }}} */
static zend_object_handle xc_var_restore_handle(xc_processor_t *processor, size_t index TSRMLS_DC) /* {{{ */
{
	zend_object_handle handle = processor->object_handles[index];
	zend_objects_store_add_ref_by_handle(handle TSRMLS_CC);
	return handle;
}
/* }}} */
#endif
static void xc_var_collect_class(xc_processor_t *processor, zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	size_t next_index;

	if (!xc_vector_initialized(&processor->class_names)) {
		xc_vector_init(xc_constant_string_t, &processor->class_names, 0);
		zend_hash_init(&processor->class_name_to_index, 0, NULL, NULL, 0);
	}

	/* HashTable <=PHP_4 cannot handle NULL pointers, +1 needed */
	next_index = xc_vector_size(&processor->class_names) + 1;
	if (zend_hash_add(&processor->class_name_to_index, ce->name, ce->name_length, (void *) &next_index, sizeof(next_index), NULL) == SUCCESS) {
		xc_constant_string_t class_name;
		class_name.str = (char *) ce->name;
		class_name.len = ce->name_length;
		xc_vector_push_back(&processor->class_names, &class_name);
	}
}
/* }}} */
static size_t xc_var_store_ce(xc_processor_t *processor, zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	size_t *index;

	if (zend_hash_find(&processor->class_name_to_index, ce->name, ce->name_length, (void **) &index) != SUCCESS) {
		php_error_docref(NULL TSRMLS_CC, E_CORE_ERROR, "Internal error: class name not found in class names");
		return (size_t) - 1;
	}

	return *index - 1;
}
/* }}} */
/* on restore */
