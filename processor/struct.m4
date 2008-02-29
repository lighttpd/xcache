define(`pushdefFUNC_NAME', `
	pushdef(`FUNC_NAME', `xc_`'KIND`'_`'ifelse(`$2', `', `$1', `$2')')
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
		IFDASM(   `zval *dst, const $1 * const src')
		IFASM(    `$1 *dst, const $1 * const src')
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
		ifdef(`SIZEOF_$1', , `m4_errprint(`AUTOCHECK WARN: $1: missing structinfo, dont panic')define(`SIZEOF_$1', 0)')
		IFASSERT(`
			/* {{{ init assert */
			ifdef(`SIZEOF_$1', , `m4_errprint(`missing SIZEOF_$1, safe to ignore')define(`SIZEOF_$1', 0)')
			ifdef(`COUNTOF_$1', , `m4_errprint(`missing COUNTOF_$1, safe to ignore')define(`COUNTOF_$1', 0)')
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
			int assert_size = SIZEOF_$1, assert_count = COUNTOF_$1;
			int done_size = 0, done_count = 0;
			/* }}} */
			IFRESTORE(`assert(xc_is_shm(src));')
			IFCALCSTORE(`assert(!xc_is_shm(src));')
			do {
		')

		ifdef(`USEMEMCPY', `IFCOPY(`
			memcpy(dst, src, sizeof($1));
			do {
		')')

		IFDPRINT(`
			fprintf(stderr, "%s", "{\n");
			indent ++;
		')
		$3`'
		IFDPRINT(`
			indent --;
			INDENT()fprintf(stderr, "}\n");
		')
		ifdef(`SKIPASSERT_ONCE', `undefine(`SKIPASSERT_ONCE')', `
			IFASSERT(`
				/* {{{ check assert */
				if (done_count != assert_count) {
					fprintf(stderr
						, "count assertion failed at %s `#'%d FUNC_NAME`' : unexpected:%d - expecting:%d = %d != 0\n"
						, __FILE__, __LINE__
						, done_count, assert_count, done_count - assert_count
						);
				}
				if (done_size != assert_size) {
					fprintf(stderr
						, "size assertion failed at %s `#'%d FUNC_NAME`' : unexpected:%d - expecting:%d = %d != 0\n"
						, __FILE__, __LINE__
						, done_size, assert_size, done_size - assert_size
						);
				}
				if (done_count != assert_count || done_size != assert_size) {
					assert(0);
				}
				/* }}} */
			')
			ifdef(`ELEMENTSOF_$1', `
				pushdef(`ELEMENTS_UNDONE', LIST_DIFF(defn(`ELEMENTSOF_$1'), defn(`ELEMENTS_DONE')))
				ifelse(defn(`ELEMENTS_UNDONE'), , `m4_errprint(`AUTOCHECK INFO: $1: processor looks good')', `
					m4_errprint(`AUTOCHECK ERROR: ====' KIND `$1 =================')
					m4_errprint(`AUTOCHECK expected:' defn(`ELEMENTSOF_$1'))
					m4_errprint(`AUTOCHECK missing :' defn(`ELEMENTS_UNDONE'))
					define(`EXIT_PENDING', 1)
				')
				popdef(`ELEMENTS_UNDONE')
			')
		')
		ifdef(`USEMEMCPY', `IFCOPY(`
			} while (0);
		')')
		IFASSERT(`
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
	ifdef(`DEFINED_'ifelse(`$5', `', `$1', `$5'), `', `m4_errprint(`Unknown struct "'ifelse(`$5', `', `$1', `$5')`"')')
	assert(sizeof($1) == sizeof(($6 $3)[0]));
	ifelse(`$6', `', `ALLOC(`$2', `$1')')
	IFDASM(`do {
		zval *zv;
		ALLOC_INIT_ZVAL(zv);
		array_init(zv);
	')
	FUNC_NAME`'(
		IFDPRINT( `           $6 $3, indent')
		IFCALC(   `processor, $6 $3')
		IFSTORE(  `processor, $6 $2, $6 $3')
		IFRESTORE(`processor, $6 $2, $6 $3')
		IFDASM(   `zv, $6 $3')
		IFASM(    `$6 $2, $6 $3')
		TSRMLS_CC
	);
	IFDASM(`
		add_assoc_zval_ex(dst, ZEND_STRS("$4"), zv);
	} while (0);
	')
	popdef(`FUNC_NAME')
	ifelse(`$6', , `FIXPOINTER_EX(`$1', `$2')')
')
dnl }}}
dnl {{{ STRUCT_P(1:type, 2:elm, 3:name=type)
define(`STRUCT_P', `
	DBG(`$0($*)')
	if (src->$2) {
		IFDPRINT(`INDENT()`'fprintf(stderr, "$1:$2 ");')
		STRUCT_P_EX(`$1', `dst->$2', `src->$2', `$2', `$3')
	}
	else {
		IFDPRINT(`INDENT()`'fprintf(stderr, "$1:$2:\tNULL\n");')
		COPYNULL_EX(`dst->$2', `$2')
	}
	DONE(`$2')
')
dnl }}}
dnl {{{ STRUCT(1:type, 2:elm, 3:name=type)
define(`STRUCT', `
	DBG(`$0($*)')
	assert(sizeof($1) == sizeof(src->$2));
	IFDPRINT(`INDENT()`'fprintf(stderr, "$1:$2 ");')
	STRUCT_P_EX(`$1', `dst->$2', `src->$2', `$2', `$3', `&')
	DONE(`$2')
')
dnl }}}
dnl {{{ STRUCT_ARRAY_I(1:count, 2:type, 3:elm, 4:name=type)
define(`STRUCT_ARRAY_I', `
pushdef(`i', `ii')
STRUCT_ARRAY(`$1', `$2', `$3', `$4')
popdef(`i')
')
dnl }}}
dnl {{{ STRUCT_ARRAY(1:count, 2:type, 3:elm, 4:name=type)
define(`STRUCT_ARRAY', `
	if (src->$3) {
		pushdefFUNC_NAME(`$2', `$4')
		IFDASM(`
			zval *arr;
			ALLOC_INIT_ZVAL(arr);
			array_init(arr);
			for (i = 0; i < src->$1; i ++) {
				zval *zv;

				ALLOC_INIT_ZVAL(zv);
				array_init(zv);
				FUNC_NAME (zv, &(src->$3[i]) TSRMLS_CC);
				add_next_index_zval(arr, zv);
			}
			add_assoc_zval_ex(dst, ZEND_STRS("$3"), arr);
		', `
			ALLOC(`dst->$3', `$2', `src->$1')
			ifdef(`AFTER_ALLOC', AFTER_ALLOC)
			for (i = 0; i < src->$1; i ++) {
				DISABLECHECK(`
					ifdef(`BEFORE_LOOP', `BEFORE_LOOP')
					STRUCT(`$2', `$3[i]', `$4')
				')
			}
		')dnl IFDASM
		DONE(`$3')
		popdef(`FUNC_NAME')
	}
	else {
		COPYNULL(`$3')
		ifdef(`AFTER_ALLOC', AFTER_ALLOC)
	}
')
dnl }}}
