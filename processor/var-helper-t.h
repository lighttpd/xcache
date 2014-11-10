/* {{{ var object helpers */
zend_bool             have_objects;
xc_vector_t           objects;         /* in calc only */
HashTable             handle_to_index; /* in calc/store only */
zend_object_handle   *object_handles;  /* in restore only */
const xc_entry_var_t *entry_var_src;   /* in restore */

xc_vector_t           class_names;          /* in calc only */
HashTable             class_name_to_index;  /* in calc/store only */
zend_class_entry    **index_to_ce;     /* in restore only */
/* }}} */
