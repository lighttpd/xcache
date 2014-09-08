define(`pushdefFUNC_NAME', `
	pushdef(`FUNC_NAME', `xc_`'PROCESSOR_TYPE`'_`'ifelse(`$2', `', `$1', `$2')')
')
dnl {{{ DECL_STRUCT_P_FUNC(1:type, 2:name, 3:comma=;)
define(`DECL_STRUCT_P_FUNC', `translit(
	pushdefFUNC_NAME(`$1', `$2')
	define(`DEFINED_'ifelse(`$2', `', `$1', `$2'), `')
	ifdef(`EXPORT_'ifelse(`$2', `', `$1', `$2'), `void', `static void inline')
	FUNC_NAME`'(
		IFDPRINT( `const $1 * const src, int indent')
		IFCALC(   `xc_processor_t *processor, const $1 * const src')
		IFSTORE(  `xc_processor_t *processor, $1 *dst, const $1 * const src')
		IFRESTORE(`xc_processor_t *processor, $1 *dst, const $1 * const src')
		IFDASM(   `xc_dasm_t *dasm, zval *dst, const $1 * const src')
		TSRMLS_DC
	)ifelse(`$3', `', `;')
	popdef(`FUNC_NAME')dnl
, `
', ` ')')
dnl }}}
dnl {{{ DEF_STRUCT_P_FUNC(1:type, 2:name, 3:body)
define(`DEF_STRUCT_P_FUNC', `
	pushdefFUNC_NAME(`$1', `$2')
/* {`{'{ FUNC_NAME */
	ifdef(`EXPORT_'ifelse(`$2', `', `$1', `$2'), `
		/* export: DECL_STRUCT_P_FUNC(`$1', `$2') :export */
	')
DECL_STRUCT_P_FUNC(`$1', `$2', 1)
	{
		pushdef(`ELEMENTS_DONE')
		IFAUTOCHECK(`
			/* {{{ init assert */
			ifdef(`SIZEOF_$1', , `m4_errprint(`Warning: missing SIZEOF_$1, safe to ignore')')
			ifdef(`COUNTOF_$1', , `m4_errprint(`Warning: missing COUNTOF_$1, safe to ignore')')
			dnl SIZEOF_x COUNTOF_x can be both defined or both not
			ifdef(`SIZEOF_$1', `
				ifdef(`COUNTOF_$1', , `m4_errprint(`AUTOCHECK WARN: missing COUNTOF_$1')')
			', `
				define(`SIZEOF_$1', 0)
			')
			ifdef(`COUNTOF_$1', `
				ifdef(`SIZEOF_$1', , `m4_errprint(`AUTOCHECK WARN: missing SIZEOF_$1')')
			', `
				define(`COUNTOF_$1', 0)
			')
			int xc_autocheck_assert_size = SIZEOF_$1, assert_count = COUNTOF_$1;
			int xc_autocheck_done_size = 0, xc_autocheck_done_count = 0;
			ifdef(`ELEMENTSOF_$1', `
				const char *xc_autocheck_assert_names[] = { ELEMENTSOF_$1 };
				size_t xc_autocheck_assert_names_count = sizeof(xc_autocheck_assert_names) / sizeof(xc_autocheck_assert_names[0]);
			', `
				const char **xc_autocheck_assert_names = NULL;
				size_t xc_autocheck_assert_names_count = 0;
			')
			zend_bool xc_autocheck_skip = 0;
			HashTable xc_autocheck_done_names;
			zend_hash_init(&xc_autocheck_done_names, 0, NULL, NULL, 0);
			/* }}} */
			IFRESTORE(`assert(xc_is_shm(src));')
			IFCALCSTORE(`assert(!xc_is_shm(src));')
			do {
		')
		ifdef(`SIZEOF_$1', , `m4_errprint(`AUTOCHECK WARN: $1: missing structinfo, dont panic')')

		ifdef(`USEMEMCPY', `IFCOPY(`
			memcpy(dst, src, sizeof($1));
			do {
		')')

		IFDPRINT(`
			fprintf(stderr, "%s", "{\n");
			indent ++;
			{
		')
		$3`'
		IFDPRINT(`
			}
			indent --;
			INDENT()fprintf(stderr, "}\n");
		')
		IFAUTOCHECK(`
		/* {{{ autocheck */
		if (!xc_autocheck_skip) {
			int name_check_errors = xc_check_names(__FILE__, __LINE__, "FUNC_NAME", xc_autocheck_assert_names, xc_autocheck_assert_names_count, &xc_autocheck_done_names);

			if (xc_autocheck_done_count != assert_count) {
				fprintf(stderr
					, "count assertion failed at %s `#'%d FUNC_NAME`' : unexpected:%d - expecting:%d = %d != 0\n"
					, __FILE__, __LINE__
					, xc_autocheck_done_count, assert_count, xc_autocheck_done_count - assert_count
					);
			}
			if (xc_autocheck_done_size != xc_autocheck_assert_size) {
				fprintf(stderr
					, "size assertion failed at %s `#'%d FUNC_NAME`' : unexpected:%d - expecting:%d = %d != 0\n"
					, __FILE__, __LINE__
					, xc_autocheck_done_size, xc_autocheck_assert_size, xc_autocheck_done_size - xc_autocheck_assert_size
					);
			}
			if (name_check_errors || xc_autocheck_done_count != assert_count || xc_autocheck_done_size != xc_autocheck_assert_size) {
				assert(0);
			}
		}
		zend_hash_destroy(&xc_autocheck_done_names);
		/* }}} */
		')
		ifdef(`ELEMENTSOF_$1', `
			pushdef(`ELEMENTS_UNDONE', LIST_DIFF(defn(`ELEMENTSOF_$1'), defn(`ELEMENTS_DONE')))
			ifelse(defn(`ELEMENTS_UNDONE'), , `m4_errprint(`AUTOCHECK INFO: $1: processor looks good')', `
				m4_errprint(`AUTOCHECK ERROR: ====' PROCESSOR_TYPE `$1 =================')
				m4_errprint(`AUTOCHECK expected:' defn(`ELEMENTSOF_$1'))
				m4_errprint(`AUTOCHECK missing :' defn(`ELEMENTS_UNDONE'))
				define(`EXIT_PENDING', 1)
			')
			popdef(`ELEMENTS_UNDONE')
		')
		ifdef(`USEMEMCPY', `IFCOPY(`
			} while (0);
		')')
		IFAUTOCHECK(`
			} while (0);
		')
		popdef(`ELEMENTS_DONE')
	}
/* }`}'} */
	popdef(`FUNC_NAME')
')
dnl }}}
dnl {{{ STRUCT_P_EX(1:type, 2:dst, 3:src, 4:elm-name, 5:name=type, 6:&)
define(`STRUCT_P_EX', `
	DBG(`$0($*)')
	pushdefFUNC_NAME(`$1', `$5')
	ifdef(`DEFINED_'ifelse(`$5', `', `$1', `$5'), `', `m4_errprint(`AUTOCHECK ERROR: Unknown struct "'ifelse(`$5', `', `$1', `$5')`"')define(`EXIT_PENDING', 1)')
	assert(sizeof($1) == sizeof(($6 $3)[0]));
	ifelse(`$6', `', `ALLOC(`$2', `$1')')
ifdef(`DASM_STRUCT_DIRECT', `', `
	IFDASM(`do {
		zval *zv;
		ALLOC_INIT_ZVAL(zv);
		array_init(zv);
	')
')
	FUNC_NAME`'(
		IFDPRINT( `           $6 $3, indent')
		IFCALC(   `processor, $6 $3')
		IFSTORE(  `processor, $6 $2, $6 $3')
		IFRESTORE(`processor, $6 $2, $6 $3')
		IFDASM(   `dasm, ifdef(`DASM_STRUCT_DIRECT', `dst', `zv'), $6 $3')
		TSRMLS_CC
	);
ifdef(`DASM_STRUCT_DIRECT', `', `
	IFDASM(`
		ifelse(`$4', `[]', `
			add_next_index_zval(dst, zv);
		', `
			add_assoc_zval_ex(dst, XCACHE_STRS("$4"), zv);
		')
	} while (0);
	')
')
	popdef(`FUNC_NAME')
	ifelse(`$6', , `FIXPOINTER_EX(`$1', `$2')')
')
dnl }}}
dnl {{{ STRUCT_P(1:type, 2:elm, 3:name=type)
define(`STRUCT_P', `
	DBG(`$0($*)')
	if (SRC(`$2')) {
		IFDPRINT(`INDENT()`'fprintf(stderr, "$1:$2 ");')
		STRUCT_P_EX(`$1', `DST(`$2')', `SRC(`$2')', `$2', `$3')
	}
	else {
		IFDPRINT(`INDENT()`'fprintf(stderr, "$1:$2:\tNULL\n");')
		COPYNULL_EX(`DST(`$2')', `$2')
	}
	DONE(`$2')
')
dnl }}}
dnl {{{ STRUCT(1:type, 2:elm, 3:name=type)
define(`STRUCT', `
	DBG(`$0($*)')
	assert(sizeof($1) == sizeof(SRC(`$2')));
	IFDPRINT(`INDENT()`'fprintf(stderr, "$1:$2 ");')
	STRUCT_P_EX(`$1', `DST(`$2')', `SRC(`$2')', `$2', `$3', `&')
	DONE(`$2')
')
dnl }}}
dnl {{{ STRUCT_ARRAY(1:count_type, 2:count, 3:type, 4:elm, 5:name=type, 6:loopcounter)
define(`STRUCT_ARRAY', `
	if (SRC(`$4')) {
		ifelse(
			`$6', `', `ifelse(`$1', `', `size_t', `$1') i; pushdef(`LOOPCOUNTER', `i')',
			`', `', `pushdef(`LOOPCOUNTER', `$6')')
		pushdefFUNC_NAME(`$3', `$5')
		IFDASM(`
			zval *arr;
			ALLOC_INIT_ZVAL(arr);
			array_init(arr);

			for (LOOPCOUNTER = 0;
					ifelse(`$2', `', `SRC(`$4[LOOPCOUNTER]')',
					`', `', `LOOPCOUNTER < SRC(`$2')');
					++LOOPCOUNTER) {
				zval *zv;

				ALLOC_INIT_ZVAL(zv);
				array_init(zv);
				FUNC_NAME (dasm, zv, &(SRC(`$4[LOOPCOUNTER]')) TSRMLS_CC);
				add_next_index_zval(arr, zv);
			}
			add_assoc_zval_ex(dst, XCACHE_STRS("$4"), arr);
		', `
			dnl find count with NULL
			ifelse(`$2', `', `
				size_t count = 0;
				while (SRC(`$4[count]')) {
					++count;
				}
				++count;
				pushdef(`ARRAY_ELEMENT_COUNT', `count')
			',
			`', `', `pushdef(`ARRAY_ELEMENT_COUNT', `SRC(`$2')')')
			ALLOC(`DST(`$4')', `$3', `ARRAY_ELEMENT_COUNT')
			popdef(`ARRAY_ELEMENT_COUNT')

			for (LOOPCOUNTER = 0;
					ifelse(`$2', `', `SRC(`$4[LOOPCOUNTER]')',
					`', `', `LOOPCOUNTER < SRC(`$2')');
					++LOOPCOUNTER) {
				DISABLECHECK(`
					STRUCT(`$3', `$4[LOOPCOUNTER]', `$5')
				')
			}
			dnl the end marker
			ifelse(`$2', `', `IFCOPY(`DST(`$4[LOOPCOUNTER]') = NULL;')')
		')dnl IFDASM
		FIXPOINTER(`$3', `$4')
		DONE(`$4')
		popdef(`FUNC_NAME')
		popdef(`LOOPCOUNTER')
	}
	else {
		COPYNULL(`$4')
	}
')
dnl }}}
