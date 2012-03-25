dnl ================
/* {{{ Pre-declare */
DECL_STRUCT_P_FUNC(`zval')
DECL_STRUCT_P_FUNC(`zval_ptr')
DECL_STRUCT_P_FUNC(`zval_ptr_nullable')
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
#ifdef IS_CV
DEF_STRUCT_P_FUNC(`zend_compiled_variable', , `dnl {{{
	DISPATCH(int, name_len)
	PROC_ZSTRING_L(, name, name_len)
	DISPATCH(ulong, hash_value)
')
dnl }}}
#endif
DEF_STRUCT_P_FUNC(`zend_uint', , `dnl {{{
	IFCOPY(`dst[0] = src[0];')
	IFDPRINT(`
		INDENT()
		fprintf(stderr, "%u\n", src[0]);
	')
	DONE_SIZE(sizeof(src[0]))
')
dnl }}}
#ifndef ZEND_ENGINE_2
DEF_STRUCT_P_FUNC(`int', , `dnl {{{
	IFCOPY(`*dst = *src;')
	IFDPRINT(`
		INDENT()
		fprintf(stderr, "%d\n", src[0]);
	')
	DONE_SIZE(sizeof(src[0]))
')
dnl }}}
#endif
#ifdef ZEND_ENGINE_2
DEF_STRUCT_P_FUNC(`zend_try_catch_element', , `dnl {{{
	DISPATCH(zend_uint, try_op)
	DISPATCH(zend_uint, catch_op)
')
dnl }}}
#endif
DEF_STRUCT_P_FUNC(`zend_brk_cont_element', , `dnl {{{
#ifdef ZEND_ENGINE_2_2
	DISPATCH(int, start)
#endif
	DISPATCH(int, cont)
	DISPATCH(int, brk)
	DISPATCH(int, parent)
')
dnl }}}
DEF_HASH_TABLE_FUNC(`HashTable_zval_ptr',           `zval_ptr')
DEF_HASH_TABLE_FUNC(`HashTable_zval_ptr_nullable',  `zval_ptr_nullable')
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
			IFDPRINT(`INDENT()`'fprintf(stderr, "[%p] ", src[0]);')
			STRUCT_P_EX(zval, dst[0], src[0], `[0]', `', ` ')
			FIXPOINTER_EX(zval, dst[0])
		} while (0);
	')
	DONE_SIZE(sizeof(zval_ptr))
')
dnl }}}
DEF_STRUCT_P_FUNC(`zval_ptr_nullable', , `dnl {{{
	if (src[0]) {
		STRUCT_P_EX(zval_ptr, dst, src, `', `', ` ')
	}
	else {
		IFCOPY(`COPYNULL_EX(src[0], src)')
	}
	DONE_SIZE(sizeof(zval_ptr_nullable))
')
dnl }}}
#ifdef ZEND_ENGINE_2
DEF_STRUCT_P_FUNC(`zend_arg_info', , `dnl {{{
	DISPATCH(zend_uint, name_len)
	PROC_ZSTRING_L(, name, name_len)
	DISPATCH(zend_uint, class_name_len)
	PROC_ZSTRING_L(, class_name, class_name_len)
#ifdef ZEND_ENGINE_2_4
	DISPATCH(zend_uchar, type_hint)
#else
	DISPATCH(zend_bool, array_type_hint)
#endif
	DISPATCH(zend_bool, allow_null)
	DISPATCH(zend_bool, pass_by_reference)
#ifndef ZEND_ENGINE_2_4
	DISPATCH(zend_bool, return_reference)
	DISPATCH(int, required_num_args)
#endif
')
dnl }}}
#endif
#ifdef HAVE_XCACHE_CONSTANT
DEF_STRUCT_P_FUNC(`zend_constant', , `dnl {{{
	STRUCT(zval, value)
	DISPATCH(int, flags)
	DISPATCH(uint, name_len)
	pushdef(`estrndup', `zend_strndup')
	PROC_ZSTRING_N(, name, name_len)
	popdef(`estrndup')
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
#ifdef ZEND_ENGINE_2
DEF_STRUCT_P_FUNC(`zend_property_info', , `dnl {{{
	DISPATCH(zend_uint, flags)
	DISPATCH(int, name_length)
	PROC_ZSTRING_L(, name, name_length)
	DISPATCH(ulong, h)
#ifdef ZEND_ENGINE_2_4
	DISPATCH(int, offset)
#endif
#ifdef ZEND_ENGINE_2_1
	DISPATCH(int, doc_comment_len)
	PROC_ZSTRING_L(, doc_comment, doc_comment_len)
#endif
	dnl isnt in php6 yet
#if defined(ZEND_ENGINE_2_2)
	PROC_CLASS_ENTRY_P(ce)
#endif
')
dnl }}}
#endif
#ifdef ZEND_ENGINE_2_4
DEF_STRUCT_P_FUNC(`zend_trait_method_reference', , `dnl {{{
	DISPATCH(unsigned int, mname_len)
	PROC_STRING_L(method_name, mname_len)
	COPYNULL(ce)
	DISPATCH(unsigned int, cname_len)
	PROC_STRING_L(class_name, cname_len)
')
dnl }}}
DEF_STRUCT_P_FUNC(`zend_trait_alias', , `dnl {{{
	STRUCT_P(zend_trait_method_reference, trait_method)
	DISPATCH(unsigned int, alias_len)
	PROC_STRING_L(alias, alias_len)
	DISPATCH(zend_uint, modifiers)
	COPYNULL(function)
')
dnl }}}
DEF_STRUCT_P_FUNC(`zend_trait_precedence', , `dnl {{{
	STRUCT_P(zend_trait_method_reference, trait_method)
	COPYNULL(exclude_from_classes)
	COPYNULL(function)
')
dnl }}}
DEF_STRUCT_P_FUNC(`zend_trait_alias_ptr', , `dnl {{{
	IFDASM(`
		pushdefFUNC_NAME(`zend_trait_alias')
		FUNC_NAME (dst, src[0] TSRMLS_CC);
		popdef(`FUNC_NAME')
	', `
		ALLOC(dst[0], zend_trait_alias)
		STRUCT_P_EX(zend_trait_alias, dst[0], src[0], `[0]', `', ` ')
		FIXPOINTER_EX(zend_trait_alias, dst[0])
	')
	DONE_SIZE(sizeof(zend_trait_alias))
')
dnl }}}
DEF_STRUCT_P_FUNC(`zend_trait_precedence_ptr', , `dnl {{{
	IFDASM(`
		pushdefFUNC_NAME(`zend_trait_precedence')
		FUNC_NAME (dst, src[0] TSRMLS_CC);
		popdef(`FUNC_NAME')
	', `
		ALLOC(dst[0], zend_trait_precedence)
		STRUCT_P_EX(zend_trait_precedence, dst[0], src[0], `[0]', `', ` ')
		FIXPOINTER_EX(zend_trait_precedence, dst[0])
	')
	DONE_SIZE(sizeof(zend_trait_precedence))
')
dnl }}}
#endif
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
#ifndef ZEND_ENGINE_2_4
	DISPATCH(zend_bool, constants_updated)
#endif
#ifdef ZEND_ENGINE_2
	DISPATCH(zend_uint, ce_flags)
#endif

#ifdef ZEND_ENGINE_2
	STRUCT(HashTable, properties_info, HashTable_zend_property_info)
#endif

#ifdef ZEND_ENGINE_2_4
	STRUCT_ARRAY(default_properties_count, zval_ptr_nullable, default_properties_table)
	DISPATCH(int, default_properties_count)
	STRUCT_ARRAY(default_static_members_count, zval_ptr_nullable, default_static_members_table)
	DISPATCH(int, default_static_members_count)
	IFCOPY(`dst->static_members_table = dst->default_static_members_table;')
	DONE(static_members_table)
#else
	IFCOPY(`dst->builtin_functions = src->builtin_functions;')
	DONE(builtin_functions)
	STRUCT(HashTable, default_properties, HashTable_zval_ptr)
#	ifdef ZEND_ENGINE_2_1
	STRUCT(HashTable, default_static_members, HashTable_zval_ptr)
	IFCOPY(`dst->static_members = &dst->default_static_members;')
	DONE(static_members)
#	elif defined(ZEND_ENGINE_2)
	STRUCT_P(HashTable, static_members, HashTable_zval_ptr)
#	endif
#endif /* ZEND_ENGINE_2_4 */

#ifdef ZEND_ENGINE_2
	STRUCT(HashTable, constants_table, HashTable_zval_ptr)

#ifdef ZEND_ENGINE_2_2
	dnl runtime binding: ADD_INTERFACE will deal with it
	COPYNULL(`interfaces')
	COPYNULL(`num_interfaces')

#	ifdef ZEND_ENGINE_2_4
	dnl runtime binding: ADD_TRAIT will deal with it
	COPYNULL(traits)
	COPYNULL(num_traits)
#	endif
#else
	IFRESTORE(`
		if (src->num_interfaces) {
			CALLOC(dst->interfaces, zend_class_entry*, src->num_interfaces)
			DONE(`interfaces')
		}
		else {
			COPYNULL(`interfaces')
		}
	', `
		DONE(`interfaces')
	')
	DISPATCH(zend_uint, num_interfaces)
#endif
	STRUCT_ARRAY(, zend_trait_alias_ptr, trait_aliases)
	STRUCT_ARRAY(, zend_trait_precedence_ptr, trait_precedences)

#	ifdef ZEND_ENGINE_2_4
	DISABLECHECK(`
	IFRESTORE(`dst->info.user.filename = processor->entry_src->filepath;', `PROC_STRING(info.user.filename)')
	DISPATCH(zend_uint, info.user.line_start)
	DISPATCH(zend_uint, info.user.line_end)
	DISPATCH(zend_uint, info.user.doc_comment_len)
	PROC_ZSTRING_L(, info.user.doc_comment, info.user.doc_comment_len)
	')
	DONE(info)
#	else
	IFRESTORE(`dst->filename = processor->entry_src->filepath;DONE(filename)', `PROC_STRING(filename)')
	DISPATCH(zend_uint, line_start)
	DISPATCH(zend_uint, line_end)
#		ifdef ZEND_ENGINE_2_1
	DISPATCH(zend_uint, doc_comment_len)
	PROC_ZSTRING_L(, doc_comment, doc_comment_len)
#		endif
#	endif

	/* # NOT DONE */
	COPY(serialize_func)
	COPY(unserialize_func)
	COPY(iterator_funcs)
	COPY(create_object)
	COPY(get_iterator)
	COPY(interface_gets_implemented)
#	ifdef ZEND_ENGINE_2_3
	COPY(get_static_method)
#	endif
	COPY(serialize)
	COPY(unserialize)
	/* deal with it inside xc_fix_method */
	SETNULL(constructor)
	COPY(destructor)
	COPY(clone)
	COPY(__get)
	COPY(__set)
/* should be >5.1 */
#	ifdef ZEND_ENGINE_2_1
	COPY(__unset)
	COPY(__isset)
#	 if defined(ZEND_ENGINE_2_2) || PHP_MAJOR_VERSION >= 6
	COPY(__tostring)
#	 endif
#	endif
	COPY(__call)
#	ifdef ZEND_CALLSTATIC_FUNC_NAME
	COPY(__callstatic)
#	endif
#	ifndef ZEND_ENGINE_2_4
	/* # NOT DONE */
	COPY(module)
#	endif
#else /* ZEND_ENGINE_2 */
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
#ifdef ZEND_ENGINE_2_4
undefine(`UNION_znode_op')
define(`UNION_znode_op', `dnl {{{
#ifndef NDEBUG
	switch ((src->$1_type ifelse($1, `result', & ~EXT_TYPE_UNUSED))) {
	case IS_CONST:
	case IS_VAR:
	case IS_CV:
	case IS_TMP_VAR:
	case IS_UNUSED:
		break;

	default:
		assert(0);
	}
#endif

	dnl dirty dispatch
	DISABLECHECK(`
	switch ((src->$1_type ifelse($1, `result', & ~EXT_TYPE_UNUSED))) {
		case IS_CONST:
			ifelse($1, `result', `
				DISPATCH(zend_uint, $1.constant)
			', `
				IFDASM(`{
					zval *zv;
					ALLOC_INIT_ZVAL(zv);
					*zv = src->$1.literal->constant;
					zval_copy_ctor(zv);
					add_assoc_zval_ex(dst, ZEND_STRS("$1.constant"), zv);
				}
				', `
					IFCOPY(`
						dst->$1 = src->$1;
					', `
						DISPATCH(zend_uint, $1.constant)
					')
				')
			')
			break;
		IFCOPY(`
			IFNOTMEMCPY(`
				default:
					$1 = $2;
			')
		', `
		case IS_VAR:
		case IS_TMP_VAR:
		case IS_CV:
			DISPATCH(zend_uint, $1.var)
			break;
		case IS_UNUSED:
			IFDASM(`DISPATCH(zend_uint, $1.var)')
			DISPATCH(zend_uint, $1.opline_num)
			break;
		')
	}
	')
	DONE($1)
')
dnl }}}
#else
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
#if 0
	DONE(EA)
#endif
#undef XCACHE_IS_CV
')
dnl }}}
#endif
DEF_STRUCT_P_FUNC(`zend_op', , `dnl {{{
	DISPATCH(zend_uchar, opcode)
#ifdef ZEND_ENGINE_2_4
	IFRESTORE(`', `
	switch (src->opcode) {
	case ZEND_BIND_TRAITS:
		((zend_op *) src)->op2_type = IS_UNUSED;
		break;
	}
	')
	UNION_znode_op(result)
	UNION_znode_op(op1)
	UNION_znode_op(op2)
#else
	STRUCT(znode, result)
	STRUCT(znode, op1)
	STRUCT(znode, op2)
#endif
	DISPATCH(ulong, extended_value)
	DISPATCH(uint, lineno)
#ifdef ZEND_ENGINE_2_1
#ifdef ZEND_ENGINE_2_4
	DISPATCH(zend_uchar, op1_type)
	DISPATCH(zend_uchar, op2_type)
	DISPATCH(zend_uchar, result_type)
#endif
	IFCOPY(`
#ifdef ZEND_ENGINE_2_4
		pushdef(`UNION_znode_op_literal', `
			if (src->$1_type == IS_CONST) {
				dst->$1.constant = src->$1.literal - processor->active_op_array_src->literals;
				dst->$1.literal = &processor->active_op_array_dst->literals[dst->$1.constant];
			}
		')
		UNION_znode_op_literal(op1)
		UNION_znode_op_literal(op2)
#endif
		popdef(`UNION_znode_op_literal')
		switch (src->opcode) {
#ifdef ZEND_GOTO
			case ZEND_GOTO:
#endif
			case ZEND_JMP:
				assert(Z_OP(src->op1).jmp_addr >= processor->active_opcodes_src && Z_OP(src->op1).jmp_addr - processor->active_opcodes_src < processor->active_op_array_src->last);
				Z_OP(dst->op1).jmp_addr = processor->active_opcodes_dst + (Z_OP(src->op1).jmp_addr - processor->active_opcodes_src);
				assert(Z_OP(dst->op1).jmp_addr >= processor->active_opcodes_dst && Z_OP(dst->op1).jmp_addr - processor->active_opcodes_dst < processor->active_op_array_dst->last);
				break;

			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_SET
			case ZEND_JMP_SET:
#endif
				assert(Z_OP(src->op2).jmp_addr >= processor->active_opcodes_src && Z_OP(src->op2).jmp_addr - processor->active_opcodes_src < processor->active_op_array_src->last);
				Z_OP(dst->op2).jmp_addr = processor->active_opcodes_dst + (Z_OP(src->op2).jmp_addr - processor->active_opcodes_src);
				assert(Z_OP(dst->op2).jmp_addr >= processor->active_opcodes_dst && Z_OP(dst->op2).jmp_addr - processor->active_opcodes_dst < processor->active_op_array_dst->last);
				break;

			default:
				break;
		}
	')
	DISPATCH(opcode_handler_t, handler)
#endif
')
dnl }}}
#ifdef ZEND_ENGINE_2_4
DEF_STRUCT_P_FUNC(`zend_literal', , `dnl {{{
	STRUCT(zval, constant)
	DISPATCH(zend_ulong, hash_value)
	DISPATCH(zend_uint,  cache_slot)
')
dnl }}}
#endif
DEF_STRUCT_P_FUNC(`zend_op_array', , `dnl {{{
	IFRESTORE(`
	const xc_op_array_info_t *op_array_info = &processor->active_op_array_infos_src[processor->active_op_array_index++];
	dnl shadow copy must NOT meet:
	dnl readonly_protection=on
	dnl main op_array && have early binding
#ifdef ZEND_COMPILE_DELAYED_BINDING
	zend_bool need_early_binding = 0;
#else
	zend_bool need_early_binding = processor->php_src->have_early_binding;
#endif
	zend_bool shallow_copy = !processor->readonly_protection && !(src == processor->php_src->op_array && need_early_binding);
	if (shallow_copy) {
		zend_bool gc_arg_info = 0;
		zend_bool gc_opcodes  = 0;
		/* really fast shallow copy */
		memcpy(dst, src, sizeof(src[0]));
		dst->refcount[0] = 1000;
		/* deep */
		STRUCT_P(HashTable, static_variables, HashTable_zval_ptr)
#ifdef ZEND_ENGINE_2
		STRUCT_ARRAY(num_args, zend_arg_info, arg_info)
		gc_arg_info = 1;
#endif
		dst->filename = processor->entry_src->filepath;
#ifdef ZEND_ENGINE_2_4
		if (src->literals /* || op_array_info->literalsinfo_cnt */) {
			gc_opcodes = 1;
		}
#else
		if (op_array_info->oplineinfo_cnt) {
			gc_opcodes = 1;
		}
#endif
		if (gc_opcodes) {
			zend_op *opline, *end;
			COPY_N_EX(last, zend_op, opcodes)

			for (opline = dst->opcodes, end = opline + src->last; opline < end; ++opline) {
#ifdef ZEND_ENGINE_2_4
				pushdef(`UNION_znode_op_literal', `
					if (opline->$1_type == IS_CONST) {
						opline->$1.constant = opline->$1.literal - src->literals;
						opline->$1.literal = &dst->literals[opline->$1.constant];
					}
				')
				UNION_znode_op_literal(op1)
				UNION_znode_op_literal(op2)
				popdef(`UNION_znode_op_literal')
#endif

				switch (opline->opcode) {
#ifdef ZEND_GOTO
					case ZEND_GOTO:
#endif
					case ZEND_JMP:
						Z_OP(opline->op1).jmp_addr = dst->opcodes + (Z_OP(opline->op1).jmp_addr - src->opcodes);
						break;

					case ZEND_JMPZ:
					case ZEND_JMPNZ:
					case ZEND_JMPZ_EX:
					case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_SET
					case ZEND_JMP_SET:
#endif
						Z_OP(opline->op2).jmp_addr = dst->opcodes + (Z_OP(opline->op2).jmp_addr - src->opcodes);
						break;

					default:
						break;
				}
			}
		}
		if (gc_arg_info || gc_opcodes) {
			xc_gc_op_array_t gc_op_array;
#ifdef ZEND_ENGINE_2
			gc_op_array.num_args = gc_arg_info ? dst->num_args : 0;
			gc_op_array.arg_info = gc_arg_info ? dst->arg_info : NULL;
#endif
			gc_op_array.opcodes  = gc_opcodes ? dst->opcodes : NULL;
			xc_gc_add_op_array(&gc_op_array TSRMLS_CC);
		}
		define(`SKIPASSERT_ONCE')
	}
	else
	')
	do {
	dnl RESTORE is done above!

	/* Common elements */
	DISPATCH(zend_uchar, type)
	PROC_ZSTRING(, function_name)
#ifdef ZEND_ENGINE_2
	DISPATCH(zend_uint, fn_flags)
	STRUCT_ARRAY(num_args, zend_arg_info, arg_info)
	DISPATCH(zend_uint, num_args)
	DISPATCH(zend_uint, required_num_args)
#	ifndef ZEND_ENGINE_2_4
	DISPATCH(zend_bool, pass_rest_by_reference)
#	endif
#else
	if (src->arg_types) {
		ALLOC(dst->arg_types, zend_uchar, src->arg_types[0] + 1)
		IFCOPY(`memcpy(dst->arg_types, src->arg_types, sizeof(src->arg_types[0]) * (src->arg_types[0]+1));')
		IFDASM(`do {
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
#ifndef ZEND_ENGINE_2_4
	DISPATCH(unsigned char, return_reference)
#endif
	/* END of common elements */
#ifdef IS_UNICODE
	dnl SETNULL(u_twin)
#endif

	STRUCT_P(zend_uint, refcount)
	UNFIXPOINTER(zend_uint, refcount)
	IFSTORE(`dst->refcount[0] = 1;')

#ifdef ZEND_ENGINE_2_4
	dnl before copying opcodes
	STRUCT_ARRAY(last_literal, zend_literal, literals)
	DISPATCH(int, last_literal)
#endif

	pushdef(`AFTER_ALLOC', `IFCOPY(`
#ifndef NDEBUG
		processor->active_op_array_dst = dst;
		processor->active_op_array_src = src;
#endif
		processor->active_opcodes_dst = dst->opcodes;
		processor->active_opcodes_src = src->opcodes;
	')')
	STRUCT_ARRAY(last, zend_op, opcodes)
	popdef(`AFTER_ALLOC')
	DISPATCH(zend_uint, last)
#ifndef ZEND_ENGINE_2_4
	IFCOPY(`dst->size = src->last;DONE(size)', `DISPATCH(zend_uint, size)')
#endif

#ifdef IS_CV
	STRUCT_ARRAY(last_var, zend_compiled_variable, vars)
	DISPATCH(int, last_var)
#	ifndef ZEND_ENGINE_2_4
	IFCOPY(`dst->size_var = src->last_var;DONE(size_var)', `DISPATCH(zend_uint, size_var)')
#	endif
#else
	dnl zend_cv.m4 is illegal to be made public, don not ask me for it
	IFDASM(`
		sinclude(srcdir`/processor/zend_cv.m4')
		')
#endif

	DISPATCH(zend_uint, T)

	STRUCT_ARRAY(last_brk_cont, zend_brk_cont_element, brk_cont_array)
	DISPATCH(zend_uint, last_brk_cont)
#ifndef ZEND_ENGINE_2_4
	DISPATCH(zend_uint, current_brk_cont)
#endif
#ifndef ZEND_ENGINE_2
	DISPATCH(zend_bool, uses_globals)
#endif

#ifdef ZEND_ENGINE_2
	STRUCT_ARRAY(last_try_catch, zend_try_catch_element, try_catch_array)
	DISPATCH(int, last_try_catch)
#endif

	STRUCT_P(HashTable, static_variables, HashTable_zval_ptr)

#ifndef ZEND_ENGINE_2_4
	COPY(start_op)
	DISPATCH(int, backpatch_count)
#endif
#ifdef ZEND_ENGINE_2_3
	DISPATCH(zend_uint, this_var)
#endif

#ifndef ZEND_ENGINE_2_4
	DISPATCH(zend_bool, done_pass_two)
#endif
	/* 5.0 <= ver < 5.3 */
#if defined(ZEND_ENGINE_2) && !defined(ZEND_ENGINE_2_3)
	DISPATCH(zend_bool, uses_this)
#endif

	IFRESTORE(`dst->filename = processor->entry_src->filepath;DONE(filename)', `PROC_STRING(filename)')
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
#ifdef ZEND_COMPILE_DELAYED_BINDING
	DISPATCH(zend_uint, early_binding);
#endif

	/* reserved */
	DONE(reserved)
#if defined(HARDENING_PATCH) && HARDENING_PATCH
	DISPATCH(zend_bool, created_by_eval)
#endif
#ifdef ZEND_ENGINE_2_4
	COPYNULL(run_time_cache)
	COPYNULL(last_cache_slot)
#endif
	} while (0);
	IFRESTORE(`xc_fix_op_array_info(processor->entry_src, processor->php_src, dst, shallow_copy, op_array_info TSRMLS_CC);')

#ifdef ZEND_ENGINE_2
	dnl mark it as -1 on store, and lookup parent on restore
	IFSTORE(`dst->prototype = (processor->active_class_entry_src && src->prototype) ? (zend_function *) -1 : NULL; DONE(prototype)', `
			IFRESTORE(`do {
				zend_function *parent;
				if (src->prototype != NULL
				 && zend_u_hash_find(&(processor->active_class_entry_dst->parent->function_table),
						UG(unicode) ? IS_UNICODE : IS_STRING,
						src->function_name, xc_zstrlen(UG(unicode) ? IS_UNICODE : IS_STRING, src->function_name) + 1,
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
DEF_STRUCT_P_FUNC(`xc_op_array_info_detail_t', , `dnl {{{
	DISPATCH(zend_uint, index)
	DISPATCH(zend_uint, info)
')
dnl }}}
DEF_STRUCT_P_FUNC(`xc_op_array_info_t', , `dnl {{{
#ifdef ZEND_ENGINE_2_4
	DISPATCH(zend_uint, literalinfo_cnt)
	STRUCT_ARRAY(literalinfo_cnt, xc_op_array_info_detail_t, literalinfos)
#else
	DISPATCH(zend_uint, oplineinfo_cnt)
	STRUCT_ARRAY(oplineinfo_cnt, xc_op_array_info_detail_t, oplineinfos)
#endif
')
dnl }}}
DEF_STRUCT_P_FUNC(`xc_funcinfo_t', , `dnl {{{
	DISPATCH(zend_uint, key_size)
#ifdef IS_UNICODE
	DISPATCH(zend_uchar, type)
#endif
	IFRESTORE(`COPY(key)', `
		PROC_ZSTRING_N(type, key, key_size)
	')
	DISPATCH(ulong, h)
	IFRESTORE(`COPY(op_array_info)', `
		STRUCT(xc_op_array_info_t, op_array_info)
	')
	IFRESTORE(`
		processor->active_op_array_infos_src = &src->op_array_info;
		processor->active_op_array_index = 0;
	')
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
	DISPATCH(zend_uint, methodinfo_cnt)
	IFRESTORE(`COPY(methodinfos)', `
		STRUCT_ARRAY(methodinfo_cnt, xc_op_array_info_t, methodinfos)
	')
	IFRESTORE(`
		processor->active_op_array_infos_src = src->methodinfos;
		processor->active_op_array_index = 0;
	')
#ifdef ZEND_ENGINE_2
	STRUCT_P(zend_class_entry, cest)
#else
	STRUCT(zend_class_entry, cest)
#endif
#ifndef ZEND_COMPILE_DELAYED_BINDING
	DISPATCH(int, oplineno)
#endif
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
	DISPATCH(int, type)
	DISPATCH(uint, lineno)
	DISPATCH(int, error_len)
	PROC_STRING_L(error, error_len)
')
dnl }}}
#endif
DEF_STRUCT_P_FUNC(`xc_entry_data_php_t', , `dnl {{{
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

	IFRESTORE(`COPY(op_array_info)', `
		STRUCT(xc_op_array_info_t, op_array_info)
	')
	IFRESTORE(`
		processor->active_op_array_infos_src = &dst->op_array_info;
		processor->active_op_array_index = 0;
	')
	STRUCT_P(zend_op_array, op_array)

#ifdef HAVE_XCACHE_CONSTANT
	DISPATCH(zend_uint, constinfo_cnt)
	STRUCT_ARRAY(constinfo_cnt, xc_constinfo_t, constinfos)
#endif

	DISPATCH(zend_uint, funcinfo_cnt)
	STRUCT_ARRAY(funcinfo_cnt, xc_funcinfo_t, funcinfos)

	DISPATCH(zend_uint, classinfo_cnt)
	STRUCT_ARRAY(classinfo_cnt, xc_classinfo_t, classinfos, , IFRESTORE(`processor->active_class_index'))
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
#ifndef ZEND_COMPILE_DELAYED_BINDING
	DISPATCH(zend_bool, have_early_binding)
#endif
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
DEF_STRUCT_P_FUNC(`xc_entry_t', , `dnl {{{
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
			IFCALCCOPY(`COPY(data.php)', `STRUCT_P(xc_entry_data_php_t, data.php)')
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

	if (src->type == XC_TYPE_PHP) {
		DISPATCH(int, filepath_len)
		IFRESTORE(`COPY(filepath)', `PROC_STRING_L(filepath, filepath_len)')
		DISPATCH(int, dirpath_len)
		IFRESTORE(`COPY(dirpath)', `PROC_STRING_L(dirpath, dirpath_len)')
#ifdef IS_UNICODE
		DISPATCH(int, ufilepath_len)
		IFRESTORE(`COPY(ufilepath)', `PROC_USTRING_L(ufilepath, ufilepath_len)')
		DISPATCH(int, udirpath_len)
		IFRESTORE(`COPY(udirpath)', `PROC_USTRING_L(udirpath, udirpath_len)')
#endif
	}
	else {
		DONE(filepath_len)
		DONE(filepath)
		DONE(dirpath_len)
		DONE(dirpath)
#ifdef IS_UNICODE
		DONE(ufilepath_len)
		DONE(ufilepath)
		DONE(udirpath_len)
		DONE(udirpath)
#endif
	}
')
dnl }}}
dnl ====================================================
