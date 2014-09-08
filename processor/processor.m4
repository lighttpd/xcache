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
DECL_STRUCT_P_FUNC(`xc_entry_var_t')
DECL_STRUCT_P_FUNC(`xc_entry_php_t')
#ifdef ZEND_ENGINE_2
DECL_STRUCT_P_FUNC(`zend_property_info')
#endif
/* }}} */
dnl ====================================================
#ifdef IS_CV
DEF_STRUCT_P_FUNC(`zend_compiled_variable', , `dnl {{{
	PROCESS(int, name_len)
	PROC_ZSTRING_L(, name, name_len)
	PROCESS(ulong, hash_value)
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
	PROCESS(zend_uint, try_op)
	PROCESS(zend_uint, catch_op)
#ifdef ZEND_ENGINE_2_5
	PROCESS(zend_uint, finally_op)
	PROCESS(zend_uint, finally_end)
#endif
')
dnl }}}
#endif
DEF_STRUCT_P_FUNC(`zend_brk_cont_element', , `dnl {{{
#ifdef ZEND_ENGINE_2_2
	PROCESS(int, start)
#endif
	PROCESS(int, cont)
	PROCESS(int, brk)
	PROCESS(int, parent)
')
dnl }}}
DEF_HASH_TABLE_FUNC(`HashTable_zval_ptr',           `zval_ptr')
DEF_HASH_TABLE_FUNC(`HashTable_zend_function',      `zend_function')
#ifdef ZEND_ENGINE_2
DEF_HASH_TABLE_FUNC(`HashTable_zend_property_info', `zend_property_info')
#endif
#ifdef IS_CONSTANT_AST
define(`ZEND_AST_HELPER', `dnl {{{
{
	IFCALCCOPY(`
		size_t zend_ast_size = ($1->kind == ZEND_CONST)
		 ? sizeof(zend_ast) + sizeof(zval)
		 : sizeof(zend_ast) + sizeof(zend_ast *) * ($1->children - 1);
	')

	pushdef(`ALLOC_SIZE_HELPER', `zend_ast_size')
	$2
	popdef(`ALLOC_SIZE_HELPER')
}
')
dnl }}}
DEF_STRUCT_P_FUNC(`zend_ast', , `dnl {{{
		zend_ushort i;
		PROCESS(zend_ushort, kind)
		PROCESS(zend_ushort, children)
		DONE(u)
		DISABLECHECK(`
			if (src->kind == ZEND_CONST) {
				assert(src->u.val);
				IFCOPY(`
					dst->u.val = (zval *) (dst + 1);
					memcpy(dst->u.val, src->u.val, sizeof(zval));
				')
				STRUCT_P_EX(zval, dst->u.val, src->u.val, `[]', `', ` ')
				FIXPOINTER(zval, u.val)
			}
			else {
				for (i = 0; i < src->children; ++i) {
					zend_ast *src_ast = (&src->u.child)[i];
					if (src_ast) {
						ZEND_AST_HELPER(`src_ast', `
							ALLOC(`(&dst->u.child)[i]', zend_ast)
							STRUCT_P_EX(zend_ast, (&dst->u.child)[i], src_ast, `[]', `', ` ')
						')
						FIXPOINTER_EX(zend_ast, (&dst->u.child)[i])
					}
					else {
						SETNULL_EX(`(&dst->u.child)[i]', `[]')
					}
				}
			}
		')
')
dnl }}}
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
				PROCESS(long, value.lval)
				break;
			case IS_DOUBLE:
				PROCESS(double, value.dval)
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
				PROCESS(int, value.str.len)
				PROC_STRING_L(value.str.val, value.str.len)
				break;
#ifdef IS_UNICODE
			case IS_UNICODE:
proc_unicode:
				PROCESS(int32_t, value.uni.len)
				PROC_ZSTRING_L(1, value.uni.val, value.uni.len)
				break;
#endif

			case IS_ARRAY:
#ifdef IS_CONSTANT_ARRAY
			case IS_CONSTANT_ARRAY:
#endif
				assert(src->value.ht);
				STRUCT_P(HashTable, value.ht, HashTable_zval_ptr)
				break;

#ifdef IS_CONSTANT_AST
			case IS_CONSTANT_AST:
				assert(src->value.ast);
				ZEND_AST_HELPER(`src->value.ast', `STRUCT_P(zend_ast, value.ast)')
				break;
#endif

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
		PROCESS(xc_zval_type_t, type)
#ifdef ZEND_ENGINE_2_3
		PROCESS(zend_uchar, is_ref__gc)
#else
		PROCESS(zend_uchar, is_ref)
#endif

#ifdef ZEND_ENGINE_2_3
		PROCESS(zend_uint, refcount__gc)
#elif defined(ZEND_ENGINE_2)
		PROCESS(zend_uint, refcount)
#else
		PROCESS(zend_ushort, refcount)
#endif
	')dnl IFDASM
')
dnl }}}
DEF_STRUCT_P_FUNC(`zval_ptr', , `dnl {{{
	IFDASM(`
		pushdefFUNC_NAME(`zval')
		FUNC_NAME (dasm, dst, src[0] TSRMLS_CC);
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
							dnl fprintf(stderr, "*dst is set to %p, PROCESSOR_TYPE is_shm %d\n", dst[0], xc_is_shm(dst[0]));
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
			IFDPRINT(`INDENT()`'fprintf(stderr, "[%p] ", (void *) src[0]);')
			STRUCT_P_EX(zval, dst[0], src[0], `[0]', `', ` ')
			FIXPOINTER_EX(zval, dst[0])
		} while (0);
	')
	DONE_SIZE(sizeof(zval_ptr))
')
dnl }}}
DEF_STRUCT_P_FUNC(`zval_ptr_nullable', , `dnl {{{
	if (src[0]) {
		pushdef(`DASM_STRUCT_DIRECT')
		STRUCT_P_EX(zval_ptr, dst, src, `', `', ` ')
		popdef(`DASM_STRUCT_DIRECT')
	}
	else {
		IFCOPY(`COPYNULL_EX(src[0], src)')
	}
	DONE_SIZE(sizeof(zval_ptr_nullable))
')
dnl }}}
#ifdef ZEND_ENGINE_2
DEF_STRUCT_P_FUNC(`zend_arg_info', , `dnl {{{
	PROCESS(zend_uint, name_len)
	PROC_ZSTRING_L(, name, name_len)
	PROCESS(zend_uint, class_name_len)
	PROC_ZSTRING_L(, class_name, class_name_len)
#ifdef ZEND_ENGINE_2_4
	PROCESS(zend_uchar, type_hint)
#elif defined(ZEND_ENGINE_2_1)
	PROCESS(zend_bool, array_type_hint)
#endif
#ifdef ZEND_ENGINE_2_6
	PROCESS(zend_uchar, pass_by_reference)
#endif
	PROCESS(zend_bool, allow_null)

#ifdef ZEND_ENGINE_2_6
	PROCESS(zend_bool, is_variadic)
#else
	PROCESS(zend_bool, pass_by_reference)
#endif

#ifndef ZEND_ENGINE_2_4
	PROCESS(zend_bool, return_reference)
	PROCESS(int, required_num_args)
#endif
')
dnl }}}
#endif
#ifdef HAVE_XCACHE_CONSTANT
DEF_STRUCT_P_FUNC(`zend_constant', , `dnl {{{
	STRUCT(zval, value)
	PROCESS(int, flags)
	PROCESS(uint, name_len)
	pushdef(`estrndup', `zend_strndup')
	PROC_ZSTRING_N(, name, name_len)
	popdef(`estrndup')
	PROCESS(int, module_number)
')
dnl }}}
#endif
DEF_STRUCT_P_FUNC(`zend_function', , `dnl {{{
	DISABLECHECK(`
	switch (SRC(`type')) {
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
	PROCESS(zend_uint, flags)
	PROCESS(int, name_length)
	PROC_ZSTRING_L(, name, name_length)
	PROCESS(ulong, h)
#ifdef ZEND_ENGINE_2_4
	PROCESS(int, offset)
#endif
#ifdef ZEND_ENGINE_2_1
	PROCESS(int, doc_comment_len)
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
	PROCESS(unsigned int, mname_len)
	PROC_STRING_L(method_name, mname_len)
	COPYNULL(ce)
	PROCESS(unsigned int, cname_len)
	PROC_STRING_L(class_name, cname_len)
')
dnl }}}
DEF_STRUCT_P_FUNC(`zend_trait_alias', , `dnl {{{
	STRUCT_P(zend_trait_method_reference, trait_method)
	PROCESS(unsigned int, alias_len)
	PROC_STRING_L(alias, alias_len)
	PROCESS(zend_uint, modifiers)
#ifndef ZEND_ENGINE_2_5
	COPYNULL(function)
#endif
')
dnl }}}
DEF_STRUCT_P_FUNC(`zend_trait_precedence', , `dnl {{{
	STRUCT_P(zend_trait_method_reference, trait_method)
	PROCESS_ARRAY(, xc_ztstring, exclude_from_classes, zend_class_entry*)
#ifndef ZEND_ENGINE_2_5
	COPYNULL(function)
#endif
')
dnl }}}
DEF_STRUCT_P_FUNC(`zend_trait_alias_ptr', , `dnl {{{
	IFDASM(`
		pushdefFUNC_NAME(`zend_trait_alias')
		FUNC_NAME (dasm, dst, src[0] TSRMLS_CC);
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
		FUNC_NAME (dasm, dst, src[0] TSRMLS_CC);
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
	PROCESS(char, type)
	PROCESS(zend_uint, name_length)
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
	PROCESS(int, refcount)
#else
	STRUCT_P(int, refcount)
#endif
#ifndef ZEND_ENGINE_2_4
	PROCESS(zend_bool, constants_updated)
#endif
#ifdef ZEND_ENGINE_2
	PROCESS(zend_uint, ce_flags)
#endif

#ifdef ZEND_ENGINE_2
	STRUCT(HashTable, properties_info, HashTable_zend_property_info)
#endif

#ifdef ZEND_ENGINE_2_4
	STRUCT_ARRAY(int, default_properties_count, zval_ptr_nullable, default_properties_table)
	PROCESS(int, default_properties_count)
	STRUCT_ARRAY(int, default_static_members_count, zval_ptr_nullable, default_static_members_table)
	PROCESS(int, default_static_members_count)
	IFCOPY(`DST(`static_members_table') = DST(`default_static_members_table');')
	DONE(static_members_table)
#else
	IFCOPY(`DST(`builtin_functions') = SRC(`builtin_functions');')
	DONE(builtin_functions)
	STRUCT(HashTable, default_properties, HashTable_zval_ptr)
#	ifdef ZEND_ENGINE_2_1
	STRUCT(HashTable, default_static_members, HashTable_zval_ptr)
	IFCOPY(`DST(`static_members') = &DST(`default_static_members');')
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
	COPYZERO(`num_interfaces')

#	ifdef ZEND_ENGINE_2_4
	dnl runtime binding: ADD_TRAIT will deal with it
	COPYNULL(traits)
	COPYZERO(num_traits)
	STRUCT_ARRAY(, , zend_trait_alias_ptr, trait_aliases)
	STRUCT_ARRAY(, , zend_trait_precedence_ptr, trait_precedences)
#	endif
#else
	IFRESTORE(`
		if (SRC(`num_interfaces')) {
			CALLOC(DST(`interfaces'), zend_class_entry*, SRC(`num_interfaces'))
			DONE(`interfaces')
		}
		else {
			COPYNULL(`interfaces')
		}
	', `
		DONE(`interfaces')
	')
	PROCESS(zend_uint, num_interfaces)
#endif

#	ifdef ZEND_ENGINE_2_4
	DISABLECHECK(`
	IFRESTORE(`DST(`info.user.filename') = processor->entry_php_src->filepath;', `PROC_STRING(info.user.filename)')
	PROCESS(zend_uint, info.user.line_start)
	PROCESS(zend_uint, info.user.line_end)
	PROCESS(zend_uint, info.user.doc_comment_len)
	PROC_ZSTRING_L(, info.user.doc_comment, info.user.doc_comment_len)
	')
	DONE(info)
#	else
	IFRESTORE(`DST(`filename') = processor->entry_php_src->filepath;DONE(filename)', `PROC_STRING(filename)')
	PROCESS(zend_uint, line_start)
	PROCESS(zend_uint, line_end)
	PROCESS(zend_uint, doc_comment_len)
	PROC_ZSTRING_L(, doc_comment, doc_comment_len)
#	endif

	/* # NOT DONE */
#	ifdef ZEND_ENGINE_2_1
	COPY(serialize_func)
	COPY(unserialize_func)
#	endif
	COPY(iterator_funcs)
	COPY(create_object)
	COPY(get_iterator)
	COPY(interface_gets_implemented)
#	ifdef ZEND_ENGINE_2_3
	COPY(get_static_method)
#	endif
#	ifdef ZEND_ENGINE_2_1
	COPY(serialize)
	COPY(unserialize)
#	endif
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
#	endif
	COPY(__call)
#	ifdef ZEND_CALLSTATIC_FUNC_NAME
	COPY(__callstatic)
#	endif
# if defined(ZEND_ENGINE_2_2) || PHP_MAJOR_VERSION >= 6
	COPY(__tostring)
# endif
# if defined(ZEND_ENGINE_2_6)
	COPY(__debugInfo)
# endif
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
	IFRESTORE(`DST(`function_table.pDestructor') = ZEND_FUNCTION_DTOR;')
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
	switch ((SRC(`$1_type') ifelse($1, `result', & ~EXT_TYPE_UNUSED))) {
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
	switch ((SRC(`$1_type') ifelse($1, `result', & ~EXT_TYPE_UNUSED))) {
		case IS_CONST:
			ifelse($1, `result', `
				PROCESS(zend_uint, $1.constant)
			', `
				IFDASM(`{
					zval *zv;
					zval *srczv = &dasm->active_op_array_src->literals[SRC(`$1.constant')].constant;
					ALLOC_ZVAL(zv);
					MAKE_COPY_ZVAL(&srczv, zv);
					add_assoc_zval_ex(dst, XCACHE_STRS("$1.constant"), zv);
				}
				', `
					IFCOPY(`
						DST(`$1') = SRC(`$1');
					', `
						PROCESS(zend_uint, $1.constant)
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
			PROCESS(zend_uint, $1.var)
			break;
		case IS_UNUSED:
			IFDASM(`PROCESS(zend_uint, $1.var)')
			PROCESS(zend_uint, $1.opline_num)
			break;
		')
	}
	')
	DONE($1)
')
dnl }}}
#else
DEF_STRUCT_P_FUNC(`znode', , `dnl {{{
	PROCESS(xc_op_type, op_type)

#ifdef IS_CV
#	define XCACHE_IS_CV IS_CV
#else
/* compatible with zend optimizer */
#	define XCACHE_IS_CV 16
#endif
	assert(SRC(`op_type') == IS_CONST ||
		SRC(`op_type') == IS_VAR ||
		SRC(`op_type') == XCACHE_IS_CV ||
		SRC(`op_type') == IS_TMP_VAR ||
		SRC(`op_type') == IS_UNUSED);
	dnl dirty dispatch
	DISABLECHECK(`
	switch (SRC(`op_type')) {
		case IS_CONST:
			STRUCT(zval, u.constant)
			break;
		IFCOPY(`
			IFNOTMEMCPY(`
				default:
					memcpy(&DST(`u'), &SRC(`u'), sizeof(SRC(`u')));
			')
		', `
		case IS_VAR:
		case IS_TMP_VAR:
		case XCACHE_IS_CV:
			PROCESS(zend_uint, u.var)
			PROCESS(zend_uint, u.EA.type)
			break;
		case IS_UNUSED:
			IFDASM(`PROCESS(zend_uint, u.var)')
			PROCESS(zend_uint, u.opline_num)
#ifndef ZEND_ENGINE_2
			PROCESS(zend_uint, u.fetch_type)
#endif
			PROCESS(zend_uint, u.EA.type)
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
	PROCESS(xc_opcode, opcode)
#ifdef ZEND_ENGINE_2_4
	IFRESTORE(`', `
	switch (SRC(`opcode')) {
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
	PROCESS(ulong, extended_value)
	PROCESS(uint, lineno)
#ifdef ZEND_ENGINE_2_4
	PROCESS(zend_uchar, op1_type)
	PROCESS(zend_uchar, op2_type)
	PROCESS(zend_uchar, result_type)
#endif
	IFCOPY(`
		assert(processor->active_op_array_src);
		assert(processor->active_op_array_dst);
#ifdef ZEND_ENGINE_2_4
		pushdef(`UNION_znode_op_literal', `
			if (SRC(`$1_type') == IS_CONST) {
				DST(`$1').constant = SRC(`$1.literal') - processor->active_op_array_src->literals;
				DST(`$1').literal = &processor->active_op_array_dst->literals[DST(`$1').constant];
			}
		')
		UNION_znode_op_literal(op1)
		UNION_znode_op_literal(op2)
#endif
		popdef(`UNION_znode_op_literal')
#ifdef ZEND_ENGINE_2
		switch (SRC(`opcode')) {
#	ifdef ZEND_GOTO
			case ZEND_GOTO:
#	endif
			case ZEND_JMP:
#	ifdef ZEND_FAST_CALL
			case ZEND_FAST_CALL:
#	endif
				assert(Z_OP(SRC(`op1')).jmp_addr >= processor->active_op_array_src->opcodes);
				assert(Z_OP(SRC(`op1')).jmp_addr - processor->active_op_array_src->opcodes < processor->active_op_array_src->last);
				Z_OP(DST(`op1')).jmp_addr = processor->active_op_array_dst->opcodes + (Z_OP(SRC(`op1')).jmp_addr - processor->active_op_array_src->opcodes);
				assert(Z_OP(DST(`op1')).jmp_addr >= processor->active_op_array_dst->opcodes);
				assert(Z_OP(DST(`op1')).jmp_addr - processor->active_op_array_dst->opcodes < processor->active_op_array_dst->last);
				FIXPOINTER_EX(zend_op, `Z_OP(DST(`op1')).jmp_addr')
				break;

			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
#	ifdef ZEND_JMP_SET
			case ZEND_JMP_SET:
#	endif
#	ifdef ZEND_JMP_SET_VAR
			case ZEND_JMP_SET_VAR:
#	endif
				assert(Z_OP(SRC(`op2')).jmp_addr >= processor->active_op_array_src->opcodes);
				assert(Z_OP(SRC(`op2')).jmp_addr - processor->active_op_array_src->opcodes < processor->active_op_array_src->last);
				Z_OP(DST(`op2')).jmp_addr = processor->active_op_array_dst->opcodes + (Z_OP(SRC(`op2')).jmp_addr - processor->active_op_array_src->opcodes);
				assert(Z_OP(DST(`op2')).jmp_addr >= processor->active_op_array_dst->opcodes);
				assert(Z_OP(DST(`op2')).jmp_addr - processor->active_op_array_dst->opcodes < processor->active_op_array_dst->last);
				FIXPOINTER_EX(zend_op, `Z_OP(DST(`op2')).jmp_addr')
				break;

			default:
				break;
		}
#	endif
	')
#ifdef ZEND_ENGINE_2
	PROCESS(opcode_handler_t, handler)
#endif
')
dnl }}}
#ifdef ZEND_ENGINE_2_4
DEF_STRUCT_P_FUNC(`zend_literal', , `dnl {{{
	STRUCT(zval, constant)
	PROCESS(zend_ulong, hash_value)
	PROCESS(zend_uint,  cache_slot)
')
dnl }}}
#endif
DEF_STRUCT_P_FUNC(`zend_op_array', , `dnl {{{
	IFCOPY(`
		processor->active_op_array_dst = dst;
		processor->active_op_array_src = src;
	')
	IFDASM(`
		dasm->active_op_array_src = src;
	')
	{
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
#ifdef ZEND_ENGINE_2_4
		zend_bool gc_literals = 0;
#endif
		/* really fast shallow copy */
		memcpy(dst, src, sizeof(src[0]));
		DST(`refcount') = &XG(op_array_dummy_refcount_holder);
		XG(op_array_dummy_refcount_holder) = ((zend_uint) -1) / 2;
#ifdef ZEND_ACC_ALIAS
		if ((processor->active_class_entry_src && (processor->active_class_entry_src->ce_flags & ZEND_ACC_TRAIT))) {
			PROC_ZSTRING(, function_name)
		}
#endif
		/* deep */
		STRUCT_P(HashTable, static_variables, HashTable_zval_ptr)
#ifdef ZEND_ENGINE_2
		if (SRC(`arg_info')) {
			gc_arg_info = 1;
			STRUCT_ARRAY(zend_uint, num_args, zend_arg_info, arg_info)
		}
#endif
		DST(`filename') = processor->entry_php_src->filepath;

#ifdef ZEND_ENGINE_2_4
		if (SRC(`literals') && op_array_info->literalinfo_cnt) {
			gc_opcodes = 1;
			gc_literals = 1;
		}
#else
		if (op_array_info->oplineinfo_cnt) {
			gc_opcodes = 1;
		}
#endif

#ifdef ZEND_ENGINE_2_4
		if (gc_literals) {
			dnl used when copying opcodes
			COPY_N_EX(last_literal, zend_literal, literals)
		}
#endif
		if (gc_opcodes) {
			zend_op *opline, *end;
			COPY_N_EX(last, zend_op, opcodes)

			for (opline = DST(`opcodes'), end = opline + SRC(`last'); opline < end; ++opline) {
#ifdef ZEND_ENGINE_2_4
				pushdef(`UNION_znode_op_literal', `
					if (opline->$1_type == IS_CONST) {
						opline->$1.literal = &DST(`literals[opline->$1.literal - SRC(`literals')]');
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
#ifdef ZEND_FAST_CALL
					case ZEND_FAST_CALL:
#endif
#ifdef ZEND_ENGINE_2
						Z_OP(opline->op1).jmp_addr = &DST(`opcodes[Z_OP(opline->op1).jmp_addr') - SRC(`opcodes')];
#endif
						break;

					case ZEND_JMPZ:
					case ZEND_JMPNZ:
					case ZEND_JMPZ_EX:
					case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_SET
					case ZEND_JMP_SET:
#endif
#ifdef ZEND_JMP_SET_VAR
			case ZEND_JMP_SET_VAR:
#endif
#ifdef ZEND_ENGINE_2
						Z_OP(opline->op2).jmp_addr = &DST(`opcodes[Z_OP(opline->op2).jmp_addr') - SRC(`opcodes')];
#endif
						break;

					default:
						break;
				}
			}
		}
		if (gc_arg_info || gc_opcodes
#ifdef ZEND_ENGINE_2_4
		 || gc_literals
#endif
		) {
			xc_gc_op_array_t gc_op_array;
#ifdef ZEND_ENGINE_2
			gc_op_array.num_args = gc_arg_info ? DST(`num_args') : 0;
			gc_op_array.arg_info = gc_arg_info ? DST(`arg_info') : NULL;
#endif
			gc_op_array.opcodes  = gc_opcodes ? DST(`opcodes') : NULL;
#ifdef ZEND_ENGINE_2_4
			gc_op_array.literals = gc_literals ? DST(`literals') : NULL;
#endif
			xc_gc_add_op_array(&gc_op_array TSRMLS_CC);
		}
		IFAUTOCHECK(`xc_autocheck_skip = 1;')
	}
	else
	')
	do {
	dnl RESTORE is done above!

	/* Common elements */
	PROCESS(zend_uchar, type)
	PROC_ZSTRING(, function_name)
#ifdef ZEND_ENGINE_2
	PROCESS(zend_uint, fn_flags)
	STRUCT_ARRAY(zend_uint, num_args, zend_arg_info, arg_info)
	PROCESS(zend_uint, num_args)
	PROCESS(zend_uint, required_num_args)
#	ifndef ZEND_ENGINE_2_4
	PROCESS(zend_bool, pass_rest_by_reference)
#	endif
#else
	if (SRC(`arg_types')) {
		ALLOC(`DST(`arg_types')', zend_uchar, SRC(`arg_types[0]') + 1)
		IFCOPY(`memcpy(DST(`arg_types'), SRC(`arg_types'), sizeof(SRC(`arg_types[0]')) * (SRC(`arg_types[0]')+1));')
		IFDASM(`do {
			int i;
			zval *zv;
			ALLOC_INIT_ZVAL(zv);
			array_init(zv);
			for (i = 0; i < SRC(`arg_types[0]'); i ++) {
				add_next_index_long(zv, SRC(`arg_types[i + 1]'));
			}
			add_assoc_zval_ex(dst, ZEND_STRS("arg_types"), zv);
		} while (0);')
		DONE(arg_types)
	}
	else {
		COPYNULL(arg_types)
	}
#endif
#ifndef ZEND_ENGINE_2_4
	PROCESS(unsigned char, return_reference)
#endif
	/* END of common elements */
#ifdef IS_UNICODE
	dnl SETNULL(u_twin)
#endif

	STRUCT_P(zend_uint, refcount)
	IFSTORE(`
		UNFIXPOINTER(zend_uint, refcount)
		DST(`refcount[0]') = 1;
		FIXPOINTER(zend_uint, refcount)
	')

#ifdef ZEND_ENGINE_2_4
	dnl used when copying opcodes
	STRUCT_ARRAY(int, last_literal, zend_literal, literals)
	PROCESS(int, last_literal)
#endif

	dnl uses literals
	STRUCT_ARRAY(zend_uint, last, zend_op, opcodes)
	PROCESS(zend_uint, last)
#ifndef ZEND_ENGINE_2_4
	IFCOPY(`DST(`size') = SRC(`last');DONE(size)', `PROCESS(zend_uint, size)')
#endif

#ifdef IS_CV
	STRUCT_ARRAY(int, last_var, zend_compiled_variable, vars)
	PROCESS(int, last_var)
#	ifndef ZEND_ENGINE_2_4
	IFCOPY(`DST(`size_var') = SRC(`last_var');DONE(size_var)', `PROCESS(zend_uint, size_var)')
#	endif
#else
	dnl zend_cv.m4 is illegal to be made public, don not ask me for it
	IFDASM(`
		sinclude(srcdir`/processor/zend_cv.m4')
		')
#endif

	PROCESS(zend_uint, T)

#ifdef ZEND_ENGINE_2_5
	PROCESS(zend_uint, nested_calls)
	PROCESS(zend_uint, used_stack)
#endif

	STRUCT_ARRAY(last_brk_cont_t, last_brk_cont, zend_brk_cont_element, brk_cont_array)
	PROCESS(last_brk_cont_t, last_brk_cont)
#ifndef ZEND_ENGINE_2_4
	PROCESS(zend_uint, current_brk_cont)
#endif
#ifndef ZEND_ENGINE_2
	PROCESS(zend_bool, uses_globals)
#endif

#ifdef ZEND_ENGINE_2
	STRUCT_ARRAY(int, last_try_catch, zend_try_catch_element, try_catch_array)
	PROCESS(int, last_try_catch)
#endif
#ifdef ZEND_ENGINE_2_5
	PROCESS(zend_bool, has_finally_block)
#endif

	STRUCT_P(HashTable, static_variables, HashTable_zval_ptr)

#ifndef ZEND_ENGINE_2_4
	COPY(start_op)
	PROCESS(int, backpatch_count)
#endif
#ifdef ZEND_ENGINE_2_3
	PROCESS(zend_uint, this_var)
#endif

#ifndef ZEND_ENGINE_2_4
	PROCESS(zend_bool, done_pass_two)
#endif
	/* 5.0 <= ver < 5.3 */
#if defined(ZEND_ENGINE_2) && !defined(ZEND_ENGINE_2_3)
	PROCESS(zend_bool, uses_this)
#endif

	IFRESTORE(`DST(`filename') = processor->entry_php_src->filepath;DONE(filename)', `PROC_STRING(filename)')
#ifdef IS_UNICODE
	IFRESTORE(`
		COPY(script_encoding)
	', `
		PROC_STRING(script_encoding)
	')
#endif
#ifdef ZEND_ENGINE_2
	PROCESS(zend_uint, line_start)
	PROCESS(zend_uint, line_end)
	PROCESS(int, doc_comment_len)
	PROC_ZSTRING_L(, doc_comment, doc_comment_len)
#endif
#ifdef ZEND_COMPILE_DELAYED_BINDING
	PROCESS(zend_uint, early_binding);
#endif

	/* reserved */
	DONE(reserved)
#if defined(HARDENING_PATCH) && HARDENING_PATCH
	PROCESS(zend_bool, created_by_eval)
#endif
#ifdef ZEND_ENGINE_2_4
	SETNULL(run_time_cache)
	PROCESS(int, last_cache_slot)
#endif
	} while (0);
	IFRESTORE(`xc_fix_op_array_info(processor->entry_php_src, processor->php_src, dst, shallow_copy, op_array_info TSRMLS_CC);')

#ifdef ZEND_ENGINE_2
	dnl mark it as -1 on store, and lookup parent on restore
	IFSTORE(`DST(`prototype') = (processor->active_class_entry_src && SRC(`prototype')) ? (zend_function *) -1 : NULL;', `
		IFRESTORE(`do {
			zend_function *parent;
			if (SRC(`prototype') != NULL
			 && zend_u_hash_find(&(processor->active_class_entry_dst->parent->function_table),
					UG(unicode) ? IS_UNICODE : IS_STRING,
					SRC(`function_name'), xc_zstrlen(UG(unicode) ? IS_UNICODE : IS_STRING, SRC(`function_name')) + 1,
					(void **) &parent) == SUCCESS) {
				/* see do_inherit_method_check() */
				if ((parent->common.fn_flags & ZEND_ACC_ABSTRACT)) {
					DST(`prototype') = parent;
				} else if (!(parent->common.fn_flags & ZEND_ACC_CTOR) || (parent->common.prototype && (parent->common.prototype->common.scope->ce_flags & ZEND_ACC_INTERFACE))) {
					/* ctors only have a prototype if it comes from an interface */
					DST(`prototype') = parent->common.prototype ? parent->common.prototype : parent;
				}
				else {
					DST(`prototype') = NULL;
				}
			}
			else {
				DST(`prototype') = NULL;
			}
		} while (0);
		')
	')
	DONE(prototype)

#endif

#ifdef ZEND_ENGINE_2
	PROC_CLASS_ENTRY_P(scope)
	IFCOPY(`
		if (SRC(`scope')) {
			xc_fix_method(processor, dst TSRMLS_CC);
		}
	')
#endif

	IFRESTORE(`
		if (xc_have_op_array_ctor) {
			zend_llist_apply_with_argument(&zend_extensions, (llist_apply_with_arg_func_t) xc_zend_extension_op_array_ctor_handler, dst TSRMLS_CC);
		}
	')
	}
	IFCOPY(`
		processor->active_op_array_dst = NULL;
		processor->active_op_array_src = NULL;
	')
	IFDASM(`
		dasm->active_op_array_src = NULL;
	')
')
dnl }}}

#ifdef HAVE_XCACHE_CONSTANT
DEF_STRUCT_P_FUNC(`xc_constinfo_t', , `dnl {{{
	PROCESS(zend_uint, key_size)
#ifdef IS_UNICODE
	PROCESS(zend_uchar, type)
#endif
	IFRESTORE(`COPY(key)', `
		PROC_ZSTRING_N(type, key, key_size)
	')
	PROCESS(ulong, h)
	STRUCT(zend_constant, constant)
')
dnl }}}
#endif
IFRESTORE(`', `
DEF_STRUCT_P_FUNC(`xc_op_array_info_detail_t', , `dnl {{{
	PROCESS(zend_uint, index)
	PROCESS(zend_uint, info)
')
dnl }}}
DEF_STRUCT_P_FUNC(`xc_op_array_info_t', , `dnl {{{
#ifdef ZEND_ENGINE_2_4
	PROCESS(zend_uint, literalinfo_cnt)
	STRUCT_ARRAY(zend_uint, literalinfo_cnt, xc_op_array_info_detail_t, literalinfos)
#else
	PROCESS(zend_uint, oplineinfo_cnt)
	STRUCT_ARRAY(zend_uint, oplineinfo_cnt, xc_op_array_info_detail_t, oplineinfos)
#endif
')
dnl }}}
')
DEF_STRUCT_P_FUNC(`xc_funcinfo_t', , `dnl {{{
	PROCESS(zend_uint, key_size)
#ifdef IS_UNICODE
	PROCESS(zend_uchar, type)
#endif
	IFRESTORE(`COPY(key)', `
		PROC_ZSTRING_N(type, key, key_size)
	')
	PROCESS(ulong, h)
	IFRESTORE(`COPY(op_array_info)', `
		STRUCT(xc_op_array_info_t, op_array_info)
	')
	IFRESTORE(`
		processor->active_op_array_infos_src = &SRC(`op_array_info');
		processor->active_op_array_index = 0;
	')
	STRUCT(zend_function, func)
')
dnl }}}
DEF_STRUCT_P_FUNC(`xc_classinfo_t', , `dnl {{{
	PROCESS(zend_uint, key_size)
#ifdef IS_UNICODE
	PROCESS(zend_uchar, type)
#endif
	IFRESTORE(`COPY(key)', `
		PROC_ZSTRING_N(type, key, key_size)
	')
	PROCESS(ulong, h)
	PROCESS(zend_uint, methodinfo_cnt)
	IFRESTORE(`COPY(methodinfos)', `
		STRUCT_ARRAY(zend_uint, methodinfo_cnt, xc_op_array_info_t, methodinfos)
	')
	IFRESTORE(`
		processor->active_op_array_infos_src = SRC(`methodinfos');
		processor->active_op_array_index = 0;
	')
#ifdef ZEND_ENGINE_2
	STRUCT_P(zend_class_entry, cest)
#else
	STRUCT(zend_class_entry, cest)
#endif
#ifndef ZEND_COMPILE_DELAYED_BINDING
	PROCESS(int, oplineno)
#endif
')
dnl }}}
IFRESTORE(`', `
#ifdef ZEND_ENGINE_2_1
DEF_STRUCT_P_FUNC(`xc_autoglobal_t', , `dnl {{{
	PROCESS(zend_uint, key_len)
#ifdef IS_UNICODE
	PROCESS(zend_uchar, type)
#endif
	IFRESTORE(`COPY(key)', `
		PROC_ZSTRING_L(type, key, key_len)
	')
	PROCESS(ulong, h)
')
dnl }}}
#endif
')
IFRESTORE(`', `
#ifdef XCACHE_ERROR_CACHING
DEF_STRUCT_P_FUNC(`xc_compilererror_t', , `dnl {{{
	PROCESS(int, type)
	PROCESS(uint, lineno)
	PROCESS(int, error_len)
	PROC_STRING_L(error, error_len)
')
dnl }}}
#endif
')
DEF_STRUCT_P_FUNC(`xc_entry_data_php_t', , `dnl {{{
	IFCOPY(`
		processor->php_dst = dst;
		processor->php_src = src;
	')

	/* skip */
	DONE(next)
	PROCESS(xc_hash_value_t, hvalue)
	PROCESS(xc_md5sum_t, md5)
	PROCESS(zend_ulong, refcount)

	PROCESS(zend_ulong, hits)
	PROCESS(size_t, size)

	IFRESTORE(`COPY(op_array_info)', `
		STRUCT(xc_op_array_info_t, op_array_info)
	')
	IFRESTORE(`
		processor->active_op_array_infos_src = &DST(`op_array_info');
		processor->active_op_array_index = 0;
	')
	STRUCT_P(zend_op_array, op_array)

#ifdef HAVE_XCACHE_CONSTANT
	PROCESS(zend_uint, constinfo_cnt)
	STRUCT_ARRAY(zend_uint, constinfo_cnt, xc_constinfo_t, constinfos)
#endif

	PROCESS(zend_uint, funcinfo_cnt)
	STRUCT_ARRAY(zend_uint, funcinfo_cnt, xc_funcinfo_t, funcinfos)

	PROCESS(zend_uint, classinfo_cnt)
	STRUCT_ARRAY(zend_uint, classinfo_cnt, xc_classinfo_t, classinfos, , IFRESTORE(`processor->active_class_index'))
#ifdef ZEND_ENGINE_2_1
	PROCESS(zend_uint, autoglobal_cnt)
	IFRESTORE(`
		COPY(autoglobals)
	', `
		STRUCT_ARRAY(zend_uint, autoglobal_cnt, xc_autoglobal_t, autoglobals)
	')
#endif
#ifdef XCACHE_ERROR_CACHING
	PROCESS(zend_uint, compilererror_cnt)
	IFRESTORE(`
		COPY(compilererrors)
	', `
		STRUCT_ARRAY(zend_uint, compilererror_cnt, xc_compilererror_t, compilererrors)
	')
#endif
#ifndef ZEND_COMPILE_DELAYED_BINDING
	PROCESS(zend_bool, have_early_binding)
#endif
	PROCESS(zend_bool, have_references)
')
dnl }}}
DEF_STRUCT_P_FUNC(`xc_entry_t', , `dnl {{{
	/* skip */
	DONE(next)
	PROCESS(size_t, size)

	PROCESS(time_t, ctime)
	PROCESS(time_t, atime)
	PROCESS(time_t, dtime)
	PROCESS(long, ttl)
	PROCESS(zend_ulong, hits)
	DONE(name) dnl handle in xc_entry_php_t and xc_entry_var_t
')
dnl }}}
DEF_STRUCT_P_FUNC(`xc_entry_php_t', , `dnl {{{
	STRUCT(xc_entry_t, entry)
	DISABLECHECK(`
		PROCESS(int, entry.name.str.len)
		IFRESTORE(`COPY(entry.name.str.val)', `
			PROC_STRING_L(entry.name.str.val, entry.name.str.len)
		')
	')

	IFCALCCOPY(`COPY(php)', `STRUCT_P(xc_entry_data_php_t, php)')

	IFSTORE(`DST(`refcount') = 0; DONE(refcount)', `PROCESS(long, refcount)')
	PROCESS(time_t, file_mtime)
	PROCESS(size_t, file_size)
	PROCESS(size_t, file_device)
	PROCESS(size_t, file_inode)

	PROCESS(size_t, filepath_len)
	IFRESTORE(`COPY(filepath)', `PROC_STRING_L(filepath, filepath_len)')
	PROCESS(size_t, dirpath_len)
	IFRESTORE(`COPY(dirpath)', `PROC_STRING_L(dirpath, dirpath_len)')
#ifdef IS_UNICODE
	PROCESS(int, ufilepath_len)
	IFRESTORE(`COPY(ufilepath)', `PROC_USTRING_L(ufilepath, ufilepath_len)')
	PROCESS(int, udirpath_len)
	IFRESTORE(`COPY(udirpath)', `PROC_USTRING_L(udirpath, udirpath_len)')
#endif
')
dnl }}}
DEF_STRUCT_P_FUNC(`xc_entry_var_t', , `dnl {{{
	STRUCT(xc_entry_t, entry)

#ifdef IS_UNICODE
	PROCESS(zend_uchar, name_type)
#endif
	dnl {{{ entry.name
	DISABLECHECK(`
#ifdef IS_UNICODE
		if (SRC(`name_type') == IS_UNICODE) {
			PROCESS(int32_t, entry.name.ustr.len)
		}
		else {
			PROCESS(int, entry.name.str.len)
		}
#else
		PROCESS(int, entry.name.str.len)
#endif
		IFRESTORE(`COPY(entry.name.str.val)', `
#ifdef IS_UNICODE
			PROC_ZSTRING_L(name_type, entry.name.uni.val, entry.name.uni.len)
#else
			PROC_STRING_L(entry.name.str.val, entry.name.str.len)
#endif
		')
	')
	dnl }}}

	IFDPRINT(`INDENT()`'fprintf(stderr, "zval:value");')
	STRUCT_P_EX(zval_ptr, DST(`value'), SRC(`value'), `value', `', `&')
	PROCESS(zend_bool, have_references)
	DONE(value)
')
dnl }}}
dnl ====================================================
