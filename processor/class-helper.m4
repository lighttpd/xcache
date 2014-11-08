/* {{{ xc_get_class_num
 * return class_index + 1
 */
static zend_ulong xc_get_class_num(xc_processor_t *processor, zend_class_entry *ce) {
	zend_uint i;
	const xc_entry_data_php_t *php = processor->php_src;
	zend_class_entry *ceptr;

	if (processor->cache_ce == ce) {
		return processor->cache_class_index + 1;
	}
	for (i = 0; i < php->classinfo_cnt; i ++) {
		ceptr = CestToCePtr(php->classinfos[i].cest);
		if (ZCEP_REFCOUNT_PTR(ceptr) == ZCEP_REFCOUNT_PTR(ce)) {
			processor->cache_ce = ceptr;
			processor->cache_class_index = i;
			assert(i <= processor->active_class_index);
			return i + 1;
		}
	}
	assert(0);
	return (zend_ulong) -1;
}
define(`xc_get_class_num', `IFSTORE(``xc_get_class_num'($@)',``xc_get_class_num' can be use in store only')')
/* }}} */
#ifdef ZEND_ENGINE_2
static zend_class_entry *xc_get_class(xc_processor_t *processor, zend_ulong class_num) { /* {{{ */
	/* must be parent or currrent class */
	assert(class_num > 0);
	assert(class_num <= processor->active_class_index + 1);
	return CestToCePtr(processor->php_dst->classinfos[class_num - 1].cest);
}
define(`xc_get_class', `IFRESTORE(``xc_get_class'($@)',``xc_get_class' can be use in restore only')')
/* }}} */
#endif
#ifdef ZEND_ENGINE_2
/* fix method on store */
static void xc_fix_method(xc_processor_t *processor, zend_op_array *dst TSRMLS_DC) /* {{{ */
{
	zend_function *zf = (zend_function *) dst;
	zend_class_entry *ce = processor->active_class_entry_dst;
	const zend_class_entry *srcce = processor->active_class_entry_src;

	/* Fixing up the default functions for objects here since
	 * we need to compare with the newly allocated functions
	 *
	 * caveat: a sub-class method can have the same name as the
	 * parent~s constructor and create problems.
	 */

	if (zf->common.fn_flags & ZEND_ACC_CTOR) {
		if (!ce->constructor) {
			ce->constructor = zf;
		}
	}
	else if (zf->common.fn_flags & ZEND_ACC_DTOR) {
		ce->destructor = zf;
	}
	else if (zf->common.fn_flags & ZEND_ACC_CLONE) {
		ce->clone = zf;
	}
	else {
	pushdef(`SET_IF_SAME_NAMEs', `
		SET_IF_SAME_NAME(__get);
		SET_IF_SAME_NAME(__set);
#ifdef ZEND_ENGINE_2_1
		SET_IF_SAME_NAME(__unset);
		SET_IF_SAME_NAME(__isset);
#endif
		SET_IF_SAME_NAME(__call);
#ifdef ZEND_CALLSTATIC_FUNC_NAME
		SET_IF_SAME_NAME(__callstatic);
#endif
#if defined(ZEND_ENGINE_2_2) || PHP_MAJOR_VERSION >= 6
		SET_IF_SAME_NAME(__tostring);
#endif
#if defined(ZEND_ENGINE_2_6)
		SET_IF_SAME_NAME(__debugInfo);
#endif
	')
#ifdef IS_UNICODE
		if (UG(unicode)) {
#define SET_IF_SAME_NAME(member) \
			do { \
				if (srcce->member && u_strcmp(ZSTR_U(zf->common.function_name), ZSTR_U(srcce->member->common.function_name)) == 0) { \
					ce->member = zf; \
				} \
			} \
			while(0)

			SET_IF_SAME_NAMEs()
#undef SET_IF_SAME_NAME
		}
		else
#endif
		do {
#define SET_IF_SAME_NAME(member) \
			do { \
				if (srcce->member && strcmp(ZSTR_S(zf->common.function_name), ZSTR_S(srcce->member->common.function_name)) == 0) { \
					ce->member = zf; \
				} \
			} \
			while(0)

			SET_IF_SAME_NAMEs()
#undef SET_IF_SAME_NAME
		} while (0);

	popdef(`SET_IF_SAME_NAMEs')

	}
}
/* }}} */
#endif
