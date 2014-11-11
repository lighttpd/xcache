/* {{{ var object helpers */
#ifdef ZEND_ENGINE_2
xc_vector_t           object_handles;  /* in calc/restore only */
HashTable             handle_to_index; /* in calc/store only */
#endif
const xc_entry_var_t *entry_var_src;   /* in restore */

xc_vector_t           class_names;          /* in calc only */
HashTable             class_name_to_index;  /* in calc/store only */
zend_class_entry    **index_to_ce;     /* in restore only */
/* }}} */
