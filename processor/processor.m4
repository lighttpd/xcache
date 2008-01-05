dnl ================
/* {{{ Pre-declare */
DECL_STRUCT_P_FUNC(`zval')
DECL_STRUCT_P_FUNC(`zval_ptr')
DECL_STRUCT_P_FUNC(`zend_op_array')
DECL_STRUCT_P_FUNC(`zend_class_entry')
#ifdef HAVE_XCACHE_CONSTANT
DECL_STRUCT_P_FUNC(`zend_constant')
#endif
DECL_STRUCT_P_FUNC(`zend_function')
DECL_STRUCT_P_FUNC(`xc_entry_t')
#ifdef ZEND_ENGINE_2
DECL_STRUCT_P_FUNC(`zend_property_info')
#endif
/* }}} */
dnl ====================================================
dnl {{{ zend_compiled_variable
#ifdef IS_CV
DEF_STRUCT_P_FUNC(`zend_compiled_variable', , `
	DISPATCH(int, name_len)
	PROC_ZSTRING_L(, name, name_len)
	DISPATCH(ulong, hash_value)
')
#endif
dnl }}}
dnl {{{ zend_uint
DEF_STRUCT_P_FUNC(`zend_uint', , `
	IFCOPY(`dst[0] = src[0];')
	IFDPRINT(`
		INDENT()
		fprintf(stderr, "%u\n", src[0]);
	')
	DONE_SIZE(sizeof(src[0]))
')
dnl }}}
dnl {{{ int
#ifndef ZEND_ENGINE_2
DEF_STRUCT_P_FUNC(`int', , `
	IFCOPY(`*dst = *src;')
	IFDPRINT(`
		INDENT()
		fprintf(stderr, "%d\n", src[0]);
	')
	DONE_SIZE(sizeof(src[0]))
')
#endif
dnl }}}
dnl {{{ zend_try_catch_element
#ifdef ZEND_ENGINE_2
DEF_STRUCT_P_FUNC(`zend_try_catch_element', , `
	DISPATCH(zend_uint, try_op)
	DISPATCH(zend_uint, catch_op)
')
#endif /* ifdef ZEND_ENGINE_2 */
dnl }}}
dnl {{{ zend_brk_cont_element
DEF_STRUCT_P_FUNC(`zend_brk_cont_element', , `
#ifdef ZEND_ENGINE_2_2
	DISPATCH(int, start)
#endif
	DISPATCH(int, cont)
	DISPATCH(int, brk)
	DISPATCH(int, parent)
')
dnl }}}
DEF_HASH_TABLE_FUNC(`HashTable_zval_ptr',           `zval_ptr')
#ifdef HAVE_XCACHE_CONSTANT
DEF_HASH_TABLE_FUNC(`HashTable_zend_constant',      `zend_constant')
#endif
DEF_HASH_TABLE_FUNC(`HashTable_zend_function',      `zend_function')
#ifdef ZEND_ENGINE_2
DEF_HASH_TABLE_FUNC(`HashTable_zend_property_info', `zend_property_info')
#endif
DEF_STRUCT_P_FUNC(`zval', , `dnl {{{
	IFDASM(`do {
		zval_dtor(dst);
		*dst = *src;
		zval_copy_ctor(dst);
		Z_SET_REFCOUNT(*dst, 1);
		DONE(value)
		DONE(type)
#ifdef ZEND_ENGINE_2_3
		DONE(is_ref__gc)
		DONE(refcount__gc)
#else
		DONE(is_ref)
		DONE(refcount)
#endif
	} while(0);
	return;
	', `
		dnl IFDASM else
		/* Variable information */
dnl {{{ zvalue_value
		DISABLECHECK(`
		switch ((Z_TYPE_P(src) & IS_CONSTANT_TYPE_MASK)) {
			case IS_LONG:
			case IS_RESOURCE:
			case IS_BOOL:
				DISPATCH(long, value.lval)
				break;
			case IS_DOUBLE:
				DISPATCH(double, value.dval)
				break;
			case IS_NULL:
				IFDPRINT(`INDENT()`'fprintf(stderr, "\tNULL\n");')
				break;

			case IS_CONSTANT:
#ifdef IS_UNICODE
				if (UG(unicode)) {
					goto proc_unicode;
				}
#endif
			case IS_STRING:
#ifdef FLAG_IS_BC
			case FLAG_IS_BC:
#endif
				DISPATCH(int, value.str.len)
				PROC_STRING_L(value.str.val, value.str.len)
				break;
#ifdef IS_UNICODE
			case IS_UNICODE:
proc_unicode:
				DISPATCH(int32_t, value.uni.len)
				PROC_ZSTRING_L(1, value.uni.val, value.uni.len)
				break;
#endif

			case IS_ARRAY:
			case IS_CONSTANT_ARRAY:
				STRUCT_P(HashTable, value.ht, HashTable_zval_ptr)
				break;

			case IS_OBJECT:
				IFNOTMEMCPY(`IFCOPY(`memcpy(dst, src, sizeof(src[0]));')')
				dnl STRUCT(value.obj)
#ifndef ZEND_ENGINE_2
				STRUCT_P(zend_class_entry, value.obj.ce)
				STRUCT_P(HashTable, value.obj.properties, HashTable_zval_ptr)
#endif
				break;

			default:
				assert(0);
		}
		')
dnl }}}
		DONE(value)
		DISPATCH(zval_data_type, type)
#ifdef ZEND_ENGINE_2_3
		DISPATCH(zend_uchar, is_ref__gc)
#else
		DISPATCH(zend_uchar, is_ref)
#endif

#ifdef ZEND_ENGINE_2_3
		DISPATCH(zend_uint, refcount__gc)
#elif defined(ZEND_ENGINE_2)
		DISPATCH(zend_uint, refcount)
#else
		DISPATCH(zend_ushort, refcount)
#endif
	')dnl IFDASM
')
dnl }}}
DEF_STRUCT_P_FUNC(`zval_ptr', , `dnl {{{
	IFDASM(`
		pushdefFUNC_NAME(`zval')
		FUNC_NAME (dst, src[0] TSRMLS_CC);
		popdef(`FUNC_NAME')
	', `
		do {
			IFCALCCOPY(`
				if (processor->reference) {
					zval_ptr *ppzv;
					if (zend_hash_find(&processor->zvalptrs, (char *) &src[0], sizeof(src[0]), (void **) &ppzv) == SUCCESS) {
						IFCOPY(`
							dst[0] = *ppzv;
							/* *dst is updated */
							dnl fprintf(stderr, "*dst is set to %p, KIND is_shm %d\n", dst[0], xc_is_shm(dst[0]));
						')
						IFCALCSTORE(`processor->have_references = 1;')
						IFSTORE(`assert(xc_is_shm(dst[0]));')
						IFRESTORE(`assert(!xc_is_shm(dst[0]));')
						break;
					}
				}
			')
			
			ALLOC(dst[0], zval)
			IFCALCCOPY(`
				if (processor->reference) {
					IFCALC(`
						/* make dummy */
						zval_ptr pzv = (zval_ptr)-1;
					', `
						zval_ptr pzv = dst[0];
						FIXPOINTER_EX(zval, pzv)
					')
					if (zend_hash_add(&processor->zvalptrs, (char *) &src[0], sizeof(src[0]), (void *) &pzv, sizeof(pzv), NULL) == SUCCESS) {
						/* first add, go on */
						dnl fprintf(stderr, "mark[%p] = %p\n", src[0], pzv);
					}
					else {
						assert(0);
					}
				}
			')
			IFCOPY(`
				dnl fprintf(stderr, "copy from %p to %p\n", src[0], dst[0]);
			')
			IFDPRINT(`INDENT()`'fprintf(stderr, "[%p]", src[0]);')
			STRUCT_P_EX(zval, dst[0], src[0], `[0]', `', ` ')
			FIXPOINTER_EX(zval, dst[0])
		} while (0);
	')
	DONE_SIZE(sizeof(zval_ptr))
')
dnl }}}
dnl {{{ zend_arg_info
#ifdef ZEND_ENGINE_2
DEF_STRUCT_P_FUNC(`zend_arg_info', , `
	DISPATCH(zend_uint, name_len)
	PROC_ZSTRING_L(, name, name_len)
	DISPATCH(zend_uint, class_name_len)
	PROC_ZSTRING_L(, class_name, class_name_len)
	DISPATCH(zend_bool, array_type_hint)
	DISPATCH(zend_bool, allow_null)
	DISPATCH(zend_bool, pass_by_reference)
	DISPATCH(zend_bool, return_reference)
	DISPATCH(int, required_num_args)
')
#endif
dnl }}}
#ifdef HAVE_XCACHE_CONSTANT
DEF_STRUCT_P_FUNC(`zend_constant', , `dnl {{{
	STRUCT(zval, value)
	DISPATCH(int, flags)
	DISPATCH(uint, name_len)
	pushdef(`emalloc', `malloc($1)')
	pushdef(`ecalloc', `calloc($1, $2)')
	PROC_ZSTRING_N(, name, name_len)
	popdef(`ecalloc')
	popdef(`emalloc')
	DISPATCH(int, module_number)
')
dnl }}}
#endif
DEF_STRUCT_P_FUNC(`zend_function', , `dnl {{{
	DISABLECHECK(`
	switch (src->type) {
	case ZEND_INTERNAL_FUNCTION:
	case ZEND_OVERLOADED_FUNCTION:
		IFNOTMEMCPY(`IFCOPY(`memcpy(dst, src, sizeof(src[0]));')')
		break;

	case ZEND_USER_FUNCTION:
	case ZEND_EVAL_CODE:
		DONE(type)
		STRUCT(zend_op_array, op_array)
		break;

	default:
		assert(0);
	}
	')
	DONE_SIZE(sizeof(src[0]))
')
dnl }}}
dnl {{{ zend_property_info
#ifdef ZEND_ENGINE_2
DEF_STRUCT_P_FUNC(`zend_property_info', , `
	DISPATCH(zend_uint, flags)
	DISPATCH(int, name_length)
	PROC_ZSTRING_L(, name, name_length)
	DISPATCH(ulong, h)
#ifdef ZEND_ENGINE_2_1
	DISPATCH(int, doc_comment_len)
	PROC_ZSTRING_L(, doc_comment, doc_comment_len)
#endif
	dnl isnt in php6 yet
#if defined(ZEND_ENGINE_2_2)
	PROC_CLASS_ENTRY_P(ce)
#endif
')
#endif
dnl }}}
DEF_STRUCT_P_FUNC(`zend_class_entry', , `dnl {{{
	IFCALCCOPY(`
		processor->active_class_entry_src = src;
		IFCOPY(`processor->active_class_entry_dst = dst;')
	')
	DISPATCH(char, type)
	DISPATCH(zend_uint, name_length)
	PROC_ZSTRING_L(, name, name_length)
	IFRESTORE(`
#ifndef ZEND_ENGINE_2
		/* just copy parent and resolve on install_class */
		COPY(parent)
#else
		PROC_CLASS_ENTRY_P(parent)
#endif
	', `
		PROC_CLASS_ENTRY_P(parent)
	')
#ifdef ZEND_ENGINE_2
	DISPATCH(int, refcount)
#else
	STRUCT_P(int, refcount)
#endif
	DISPATCH(zend_bool, constants_updated)
#ifdef ZEND_ENGINE_2
	DISPATCH(zend_uint, ce_flags)
#endif

	STRUCT(HashTable, default_properties, HashTable_zval_ptr)
	IFCOPY(`dst->builtin_functions = src->builtin_functions;')
	DONE(builtin_functions)
#ifdef ZEND_ENGINE_2
	STRUCT(HashTable, properties_info, HashTable_zend_property_info)
#	ifdef ZEND_ENGINE_2_1
	STRUCT(HashTable, default_static_members, HashTable_zval_ptr)
	IFCOPY(`dst->static_members = &dst->default_static_members;')
	DONE(static_members)
#	else
	STRUCT_P(HashTable, static_members, HashTable_zval_ptr)
#	endif
	STRUCT(HashTable, constants_table, HashTable_zval_ptr)

	dnl runtime binding: ADD_INTERFACE will deal with it
	IFRESTORE(`
		if (src->num_interfaces) {
			CALLOC(dst->interfaces, zend_class_entry*, src->num_interfaces)
			DONE(`interfaces')
		}
		else {
			COPYNULL(interfaces)
		}
	')
	IFDASM(`
		if (src->num_interfaces) {
			/*
			zval *arr;
			ALLOC_INIT_ZVAL(arr);
			array_init(arr);
			for (i = 0; i < src->num_interfaces; i ++) {
				zval *zv;
				ALLOC_INIT_ZVAL(zv);
				ZVAL_STRING(src->num_interfaces);
			}
			add_assoc_zval_ex(dst, ZEND_STRS("interfaces"), arr);
			*/
			DONE(`interfaces')
		}
		else {
			COPYNULL(interfaces)
		}
	')
	IFRESTORE(`', `
		IFDASM(`', `
			DONE(`interfaces')
		')
	')
	DISPATCH(zend_uint, num_interfaces)

	IFRESTORE(`COPY(filename)', `PROC_STRING(filename)')
	DISPATCH(zend_uint, line_start)
	DISPATCH(zend_uint, line_end)
#ifdef ZEND_ENGINE_2_1
	DISPATCH(zend_uint, doc_comment_len)
	PROC_ZSTRING_L(, doc_comment, doc_comment_len)
#endif
	/* # NOT DONE */
	COPY(serialize_func)
	COPY(unserialize_func)
	COPY(iterator_funcs)
	COPY(create_object)
	COPY(get_iterator)
	COPY(interface_gets_implemented)
#ifdef ZEND_ENGINE_2_3
	COPY(get_static_method)
#endif
	COPY(serialize)
	COPY(unserialize)
	/* deal with it inside xc_fix_method */
	SETNULL(constructor)
	COPY(destructor)
	COPY(clone)
	COPY(__get)
	COPY(__set)
/* should be >5.1 */
#ifdef ZEND_ENGINE_2_1
	COPY(__unset)
	COPY(__isset)
# if defined(ZEND_ENGINE_2_2) || PHP_MAJOR_VERSION >= 6
	COPY(__tostring)
# endif
#endif
	COPY(__call)
#ifdef ZEND_CALLSTATIC_FUNC_NAME
	COPY(__callstatic)
#endif
	/* # NOT DONE */
	COPY(module)
#else
	COPY(handle_function_call)
	COPY(handle_property_get)
	COPY(handle_property_set)
#endif
	dnl must do after SETNULL(constructor) and dst->parent
	STRUCT(HashTable, function_table, HashTable_zend_function)
	IFRESTORE(`dst->function_table.pDestructor = ZEND_FUNCTION_DTOR;')
	IFCALCCOPY(`
		processor->active_class_entry_src = NULL;
		IFCOPY(`processor->active_class_entry_dst = NULL;')
	')
')
dnl }}}
DEF_STRUCT_P_FUNC(`znode', , `dnl {{{
	DISPATCH(int, op_type)

#ifdef IS_CV
#	define XCACHE_IS_CV IS_CV
#else
/* compatible with zend optimizer */
#	define XCACHE_IS_CV 16
#endif
	assert(src->op_type == IS_CONST ||
		src->op_type == IS_VAR ||
		src->op_type == XCACHE_IS_CV ||
		src->op_type == IS_TMP_VAR ||
		src->op_type == IS_UNUSED);
	dnl dirty dispatch
	DISABLECHECK(`
	switch (src->op_type) {
		case IS_CONST:
			STRUCT(zval, u.constant)
			break;
		IFCOPY(`
			IFNOTMEMCPY(`
				default:
					memcpy(&dst->u, &src->u, sizeof(src->u));
			')
		', `
		case IS_VAR:
		case IS_TMP_VAR:
		case XCACHE_IS_CV:
			DISPATCH(zend_uint, u.var)
			DISPATCH(zend_uint, u.EA.type)
			break;
		case IS_UNUSED:
			IFDASM(`DISPATCH(zend_uint, u.var)')
			DISPATCH(zend_uint, u.opline_num)
#ifndef ZEND_ENGINE_2
			DISPATCH(zend_uint, u.fetch_type)
#endif
			DISPATCH(zend_uint, u.EA.type)
			break;
		')
	}
	')
	DONE(u)
#undef XCACHE_IS_CV
')
dnl }}}
DEF_STRUCT_P_FUNC(`zend_op', , `dnl {{{
	DISPATCH(zend_uchar, opcode)
	STRUCT(znode, result)
	STRUCT(znode, op1)
	STRUCT(znode, op2)
	DISPATCH(ulong, extended_value)
	DISPATCH(uint, lineno)
#ifdef ZEND_ENGINE_2_1
	IFCOPY(`
		switch (src->opcode) {
			case ZEND_JMP:
				dst->op1.u.jmp_addr = processor->active_opcodes_dst + (src->op1.u.jmp_addr - processor->active_opcodes_src);
				break;

			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_SET
			case ZEND_JMP_SET:
#endif
				dst->op2.u.jmp_addr = processor->active_opcodes_dst + (src->op2.u.jmp_addr - processor->active_opcodes_src);
				break;

			default:
				break;
		}
	')
	DISPATCH(opcode_handler_t, handler)
#endif
')
dnl }}}
DEF_STRUCT_P_FUNC(`zend_op_array', , `dnl {{{
	IFRESTORE(`
	dnl shadow copy must NOT meet:
	dnl readonly_protection=on
	dnl main op_array && have early binding
	zend_uint ii;
	if (!processor->readonly_protection && !(src == processor->php_src->op_array && processor->php_src->have_early_binding)) {
		/* really fast shallow copy */
		memcpy(dst, src, sizeof(src[0]));
		dst->refcount[0] = 1000;
		/* deep */
		STRUCT_P(HashTable, static_variables, HashTable_zval_ptr)
		STRUCT_ARRAY_I(num_args, zend_arg_info, arg_info)
		xc_gc_add_op_array(dst TSRMLS_CC);
		define(`SKIPASSERT_ONCE')
	}
	else
	')
	do {
	dnl RESTORE is done above!
	zend_uint ii;
	int i;

	/* Common elements */
	DISPATCH(zend_uchar, type)
	PROC_ZSTRING(, function_name)
#ifdef ZEND_ENGINE_2
	DISPATCH(zend_uint, fn_flags)
	STRUCT_ARRAY_I(num_args, zend_arg_info, arg_info)
	DISPATCH(zend_uint, num_args)
	DISPATCH(zend_uint, required_num_args)
	DISPATCH(zend_bool, pass_rest_by_reference)
#else
	if (src->arg_types) {
		ALLOC(dst->arg_types, zend_uchar, src->arg_types[0] + 1)
		IFCOPY(`memcpy(dst->arg_types, src->arg_types, sizeof(src->arg_types[0]) * (src->arg_types[0]+1));')
		IFDASM(`do {
			zend_uint ii;
			int i;
			zval *zv;
			ALLOC_INIT_ZVAL(zv);
			array_init(zv);
			for (i = 0; i < src->arg_types[0]; i ++) {
				add_next_index_long(zv, src->arg_types[i + 1]);
			}
			add_assoc_zval_ex(dst, ZEND_STRS("arg_types"), zv);
		} while (0);')
		DONE(arg_types)
	}
	else {
		IFDASM(`do {
			/* empty array */
			zval *zv;
			ALLOC_INIT_ZVAL(zv);
			array_init(zv);
			add_assoc_zval_ex(dst, ZEND_STRS("arg_types"), zv);
		} while (0);
		DONE(arg_types)
		', `
		COPYNULL(arg_types)
		')
	}
#endif
	DISPATCH(unsigned char, return_reference)
	/* END of common elements */
#ifdef IS_UNICODE
	dnl SETNULL(u_twin)
#endif

	STRUCT_P(zend_uint, refcount)
	UNFIXPOINTER(zend_uint, refcount)
	IFSTORE(`dst->refcount[0] = 1;')

	pushdef(`AFTER_ALLOC', `IFCOPY(`
		processor->active_opcodes_dst = dst->opcodes;
		processor->active_opcodes_src = src->opcodes;
	')')
	STRUCT_ARRAY_I(last, zend_op, opcodes)
	popdef(`AFTER_ALLOC')
	DISPATCH(zend_uint, last)
	IFCOPY(`dst->size = src->last;DONE(size)', `DISPATCH(zend_uint, size)')

#ifdef IS_CV
	STRUCT_ARRAY(last_var, zend_compiled_variable, vars)
	DISPATCH(int, last_var)
	IFCOPY(`dst->size_var = src->last_var;DONE(size_var)', `DISPATCH(zend_uint, size_var)')
#else
	dnl zend_cv.m4 is illegal to be made public, don not ask me for it
	IFDASM(`
		sinclude(srcdir`/processor/zend_cv.m4')
		')
#endif

	DISPATCH(zend_uint, T)

	STRUCT_ARRAY_I(last_brk_cont, zend_brk_cont_element, brk_cont_array)
	DISPATCH(zend_uint, last_brk_cont)
	DISPATCH(zend_uint, current_brk_cont)
#ifndef ZEND_ENGINE_2
	DISPATCH(zend_bool, uses_globals)
#endif

#ifdef ZEND_ENGINE_2
	STRUCT_ARRAY(last_try_catch, zend_try_catch_element, try_catch_array)
	DISPATCH(int, last_try_catch)
#endif

	STRUCT_P(HashTable, static_variables, HashTable_zval_ptr)

	COPY(start_op)
	DISPATCH(int, backpatch_count)

	DISPATCH(zend_bool, done_pass_two)
#ifdef ZEND_ENGINE_2
	DISPATCH(zend_bool, uses_this)
#endif

	IFRESTORE(`COPY(filename)', `PROC_STRING(filename)')
#ifdef IS_UNICODE
	IFRESTORE(`
		COPY(script_encoding)
	', `
		PROC_STRING(script_encoding)
	')
#endif
#ifdef ZEND_ENGINE_2
	DISPATCH(zend_uint, line_start)
	DISPATCH(zend_uint, line_end)
	DISPATCH(int, doc_comment_len)
	PROC_ZSTRING_L(, doc_comment, doc_comment_len)
#endif

	/* reserved */
	DONE(reserved)
#if defined(HARDENING_PATCH) && HARDENING_PATCH
	DISPATCH(zend_bool, created_by_eval)
#endif
	} while (0);

#ifdef ZEND_ENGINE_2
	dnl mark it as -1 on store, and lookup parent on restore
	IFSTORE(`dst->prototype = (processor->active_class_entry_src && src->prototype) ? (zend_function *) -1 : NULL; DONE(prototype)', `
			IFRESTORE(`do {
				zend_function *parent;
				if (src->prototype != NULL
				 && zend_u_hash_find(&(processor->active_class_entry_dst->parent->function_table),
						UG(unicode) ? IS_UNICODE : IS_STRING,
						src->function_name, xc_zstrlen(UG(unicode), src->function_name) + 1,
						(void **) &parent) == SUCCESS) {
					/* see do_inherit_method_check() */
					if ((parent->common.fn_flags & ZEND_ACC_ABSTRACT)) {
					  dst->prototype = parent;
					} else if (!(parent->common.fn_flags & ZEND_ACC_CTOR) || (parent->common.prototype && (parent->common.prototype->common.scope->ce_flags & ZEND_ACC_INTERFACE))) {
						/* ctors only have a prototype if it comes from an interface */
						dst->prototype = parent->common.prototype ? parent->common.prototype : parent;
					}
					else {
						dst->prototype = NULL;
					}
				}
				else {
					dst->prototype = NULL;
				}
				DONE(prototype)
			} while (0);
			', `
				COPYNULL(prototype)
			')
	')
#endif

#ifdef ZEND_ENGINE_2
	PROC_CLASS_ENTRY_P(scope)
	IFCOPY(`
		if (src->scope) {
			xc_fix_method(processor, dst TSRMLS_CC);
		}
	')
#endif

	IFRESTORE(`
		if (xc_have_op_array_ctor) {
			zend_llist_apply_with_argument(&zend_extensions, (llist_apply_with_arg_func_t) xc_zend_extension_op_array_ctor_handler, dst TSRMLS_CC);
		}
	')
')
dnl }}}

#ifdef HAVE_XCACHE_CONSTANT
DEF_STRUCT_P_FUNC(`xc_constinfo_t', , `dnl {{{
	DISPATCH(zend_uint, key_size)
#ifdef IS_UNICODE
	DISPATCH(zend_uchar, type)
#endif
	IFRESTORE(`COPY(key)', `
		PROC_ZSTRING_N(type, key, key_size)
	')
	DISPATCH(ulong, h)
	STRUCT(zend_constant, constant)
')
dnl }}}
#endif
DEF_STRUCT_P_FUNC(`xc_funcinfo_t', , `dnl {{{
	DISPATCH(zend_uint, key_size)
#ifdef IS_UNICODE
	DISPATCH(zend_uchar, type)
#endif
	IFRESTORE(`COPY(key)', `
		PROC_ZSTRING_N(type, key, key_size)
	')
	DISPATCH(ulong, h)
	STRUCT(zend_function, func)
')
dnl }}}
DEF_STRUCT_P_FUNC(`xc_classinfo_t', , `dnl {{{
	DISPATCH(zend_uint, key_size)
#ifdef IS_UNICODE
	DISPATCH(zend_uchar, type)
#endif
	IFRESTORE(`COPY(key)', `
		PROC_ZSTRING_N(type, key, key_size)
	')
	DISPATCH(ulong, h)
#ifdef ZEND_ENGINE_2
	STRUCT_P(zend_class_entry, cest)
#else
	STRUCT(zend_class_entry, cest)
#endif
	DISPATCH(int, oplineno)
')
dnl }}}
#ifdef ZEND_ENGINE_2_1
DEF_STRUCT_P_FUNC(`xc_autoglobal_t', , `dnl {{{
	DISPATCH(zend_uint, key_len)
#ifdef IS_UNICODE
	DISPATCH(zend_uchar, type)
#endif
	IFRESTORE(`COPY(key)', `
		PROC_ZSTRING_L(type, key, key_len)
	')
	DISPATCH(ulong, h)
')
dnl }}}
#endif
#ifdef E_STRICT
DEF_STRUCT_P_FUNC(`xc_compilererror_t', , `dnl {{{
	DISPATCH(uint, lineno)
	DISPATCH(int, error_len)
	PROC_STRING_L(error, error_len)
')
dnl }}}
#endif
DEF_STRUCT_P_FUNC(`xc_entry_data_php_t', , `dnl {{{
	zend_uint i;

	IFCOPY(`
		processor->php_dst = dst;
		processor->php_src = src;
	')

	DISPATCH(xc_hash_value_t, hvalue)
	/* skip */
	DONE(next)
	COPY(cache)
	DISPATCH(xc_md5sum_t, md5)
	DISPATCH(zend_ulong, refcount)

	DISPATCH(size_t, sourcesize)
	DISPATCH(zend_ulong, hits)
	DISPATCH(size_t, size)

	STRUCT_P(zend_op_array, op_array)

#ifdef HAVE_XCACHE_CONSTANT
	DISPATCH(zend_uint, constinfo_cnt)
	STRUCT_ARRAY(constinfo_cnt, xc_constinfo_t, constinfos)
#endif

	DISPATCH(zend_uint, funcinfo_cnt)
	STRUCT_ARRAY(funcinfo_cnt, xc_funcinfo_t, funcinfos)

	DISPATCH(zend_uint, classinfo_cnt)
	pushdef(`BEFORE_LOOP', `
		IFCOPY(`
			processor->active_class_num = i + 1;
		')
	')
	STRUCT_ARRAY(classinfo_cnt, xc_classinfo_t, classinfos)
	popdef(`BEFORE_LOOP')
#ifdef ZEND_ENGINE_2_1
	DISPATCH(zend_uint, autoglobal_cnt)
	IFRESTORE(`
		COPY(autoglobals)
	', `
		STRUCT_ARRAY(autoglobal_cnt, xc_autoglobal_t, autoglobals)
	')
#endif
#ifdef E_STRICT
	DISPATCH(zend_uint, compilererror_cnt)
	IFRESTORE(`
		COPY(compilererrors)
	', `
		STRUCT_ARRAY(compilererror_cnt, xc_compilererror_t, compilererrors)
	')
#endif
	DISPATCH(zend_bool, have_early_binding)
	DISPATCH(zend_bool, have_references)
')
dnl }}}
DEF_STRUCT_P_FUNC(`xc_entry_data_var_t', , `dnl {{{
	IFDPRINT(`INDENT()`'fprintf(stderr, "zval:value");')
	STRUCT_P_EX(zval_ptr, dst->value, src->value, `value', `', `&')
	DISPATCH(zend_bool, have_references)
	DONE(value)
')
dnl }}}
dnl {{{ xc_entry_t
DEF_STRUCT_P_FUNC(`xc_entry_t', , `
	DISPATCH(xc_entry_type_t, type)
	DISPATCH(size_t, size)

	DISPATCH(xc_hash_value_t, hvalue)
	COPY(cache)
	/* skip */
	DONE(next)

	IFSTORE(`dst->refcount = 0; DONE(refcount)', `DISPATCH(long, refcount)')

	DISPATCH(time_t, ctime)
	DISPATCH(time_t, atime)
	DISPATCH(time_t, dtime)
	DISPATCH(long, ttl)
	DISPATCH(zend_ulong, hits)
#ifdef IS_UNICODE
	DISPATCH(zend_uchar, name_type)
#endif
	dnl {{{ name
	DISABLECHECK(`
#ifdef IS_UNICODE
		if (src->name_type == IS_UNICODE) {
			DISPATCH(int32_t, name.ustr.len)
		}
		else {
			DISPATCH(int, name.str.len)
		}
#else
		DISPATCH(int, name.str.len)
#endif
		IFRESTORE(`COPY(name.str.val)', `
#ifdef IS_UNICODE
			PROC_ZSTRING_L(name_type, name.uni.val, name.uni.len)
#else
			PROC_STRING_L(name.str.val, name.str.len)
#endif
		')
	')
	DONE(name)
	dnl }}}

	dnl {{{ data
	DISABLECHECK(`
		switch (src->type) {
		case XC_TYPE_PHP:
			IFCALCCOPY(`DONE(data.php)', `STRUCT_P(xc_entry_data_php_t, data.php)')
			break;

		case XC_TYPE_VAR:
			STRUCT_P(xc_entry_data_var_t, data.var)
			break;

		default:
			assert(0);
		}
	')
	DONE(data)
	dnl }}}
	DISPATCH(time_t, mtime)
#ifdef HAVE_INODE
	DISPATCH(int, device)
	DISPATCH(int, inode)
#endif
')
dnl }}}
dnl ====================================================
