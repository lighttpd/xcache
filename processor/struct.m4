define(`pushdefFUNC_NAME', `
	pushdef(`FUNC_NAME', `xc_`'KIND`'_`'ifelse(`$2', `', `$1', `$2')')
')
dnl {{{ DECL_STRUCT_P_FUNC(1:type, 2:name, 3:comma=;)
define(`DECL_STRUCT_P_FUNC', `patsubst(
	pushdefFUNC_NAME(`$1', `$2')
	define(`DEFINED_'ifelse(`$2', `', `$1', `$2'), `')
	ifdef(`EXPORT_'ifelse(`$2', `', `$1', `$2'), `void', `static void inline')
	FUNC_NAME`'(
		IFDPRINT( `const $1 * const src, int indent')
		IFCALC(   `processor_t *processor, const $1 * const src')
		IFSTORE(  `processor_t *processor, $1 *dst, const $1 * const src')
		IFRESTORE(`processor_t *processor, $1 *dst, const $1 * const src')
		IFDASM(   `zval *dst, const $1 * const src')
		IFASM(    `$1 *dst, const $1 * const src')
		TSRMLS_DC
	)ifelse(`$3', `', `;')
	popdef(`FUNC_NAME')dnl
, `[
	 ]+', ` ')')
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
		IFASSERT(`
			/* {{{ init assert */
			ifdef(`SIZEOF_$1', , `m4_errprint(`missing SIZEOF_$1, safe to ignore')define(`SIZEOF_$1', 0)')
			ifdef(`COUNTOF_$1', , `m4_errprint(`missing COUNTOF_$1, safe to ignore')define(`COUNTOF_$1', 0)')
			int assert_size = SIZEOF_$1, assert_count = COUNTOF_$1;
			int done_size = 0, done_count = 0;
			ifdef(`ELEMENTSOF_$1', `
				define(`ELEMENTS', defn(`ELEMENTSOF_$1'))
			')
			/* }}} */
			IFRESTORE(`assert(xc_is_shm(src));')
			IFCALCSTORE(`assert(!xc_is_shm(src));')
		')

		ifdef(`USEMEMCPY', `IFCOPY(`
			memcpy(dst, src, sizeof($1));
		')')

		IFDPRINT(`
			fprintf(stderr, "%s", " {\n");
			indent ++;
		')
		$3`'
		IFDPRINT(`
			indent --;
			INDENT()fprintf(stderr, "}\n");
		')
		ifdef(`SKIPASSERT_ONCE', `undefine(`SKIPASSERT_ONCE')', `IFASSERT(`
				/* {{{ check assert */
				if (done_count != assert_count) {
					fprintf(stderr
						, "count assertion failed at %s `#'%d FUNC_NAME`' : unexpected %d - expected %d = %d != 0\n"
						, __FILE__, __LINE__
						, done_count, assert_count, done_count - assert_count
						);
					abort();
				}
				if (done_size != assert_size) {
					fprintf(stderr
						, "size assertion failed at %s `#'%d FUNC_NAME`' : %d - %d = %d != 0\n"
						, __FILE__, __LINE__
						, done_size, assert_size, done_size - assert_size
						);
					abort();
				}
				ifdef(`ELEMENTSOF_$1', `
					ifelse(ELEMENTS, , , `
						m4_errprint(`====' KIND `$1 =================')
						m4_errprint(`expected:' ELEMENTSOF_$1)
						m4_errprint(`missing :' ELEMENTS)
						define(`EXIT_PENDING', 1)
					')
				')
				undefine(`ELEMENTS')
				/* }}} */
		')')
	}
/* }`}'} */
	popdef(`FUNC_NAME')
')
dnl }}}
dnl {{{ STRUCT_P_EX(1:type, 2:dst, 3:src, 4:name=type, 5:&)
define(`STRUCT_P_EX', `
	DBG(`$0($*)')
	pushdefFUNC_NAME(`$1', `$4')
	ifdef(`DEFINED_'ifelse(`$4', `', `$1', `$4'), `', `m4_errprint(`Unknown struct "'ifelse(`$4', `', `$1', `$4')`"')')
	assert(sizeof($1) == sizeof(($5 $3)[0]));
	ifelse(`$5', `', `ALLOC(`$2', `$1')')
	IFDASM(`do {
		zval *zv;
		ALLOC_INIT_ZVAL(zv);
		array_init(zv);
	')
	FUNC_NAME`'(
		IFDPRINT( `           $5 $3, indent')
		IFCALC(   `processor, $5 $3')
		IFSTORE(  `processor, $5 $2, $5 $3')
		IFRESTORE(`processor, $5 $2, $5 $3')
		IFDASM(   `zv, $5 $3')
		IFASM(    `$5 $2, $5 $3')
		TSRMLS_CC
	);
	IFDASM(`
		add_assoc_zval_ex(dst, ZEND_STRS("patsubst(`$2', `dst->')"), zv);
	} while (0);
	')
	popdef(`FUNC_NAME')
	ifelse(`$5', , `FIXPOINTER_EX(`$1', `$2')')
')
dnl }}}
dnl {{{ STRUCT_P(1:type, 2:elm, 3:name=type)
define(`STRUCT_P', `
	DBG(`$0($*)')
	if (src->$2) {
		STRUCT_P_EX(`$1', `dst->$2', `src->$2', `$3')
		IFDPRINT(`INDENT()`'fprintf(stderr, "$1:$2");')
	}
	else {
		COPYNULL_EX(dst->$2)
		IFDPRINT(`INDENT()`'fprintf(stderr, "$1:$2:\tNULL\n");')
	}
	DONE(`$2')
')
dnl }}}
dnl {{{ STRUCT(1:type, 2:elm, 3:name=type)
define(`STRUCT', `
	DBG(`$0($*)')
	assert(sizeof($1) == sizeof(src->$2));
	IFDPRINT(`INDENT()`'fprintf(stderr, "$1:$2");')
	STRUCT_P_EX(`$1', `dst->$2', `src->$2', `$3', `&')
	DONE(`$2')
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
