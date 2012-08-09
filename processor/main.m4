divert(-1)
dnl ================ start ======================
dnl define(`XCACHE_ENABLE_TEST')
dnl define(`DEBUG_SIZE')
define(`USEMEMCPY')

dnl ================ main

dnl {{{ basic
define(`REDEF', `ifdef(`$1', `undefine(`$1')') define(`$1', `$2')')
define(`MAKE_MACRONAME', `translit(`$1', ` ():
', `_____')')
define(`ONCE', `ifdef(MAKE_MACRONAME(`ONCE $1'), `', `define(MAKE_MACRONAME(`ONCE $1'))$1')')
define(`m4_errprint', `ONCE(`errprint(`$1
')')')
ifdef(`len', `
define(`m4_len', defn(`len'))
undefine(`len')
')
define(`ZEND_STRS', `($1), (sizeof($1))')
define(`ZEND_STRL', `($1), (sizeof($1) - 1)')
define(`DST', `dst->$1')
define(`SRC', `src->$1')
dnl ============
define(`INDENT', `xc_dprint_indent(indent);')
dnl }}}
dnl {{{ ALLOC(1:dst, 2:type, 3:count=1, 4:clean=false, 5:realtype=$2)
define(`ALLOC', `
	pushdef(`COUNT', `ifelse(`$3', `', `1', `$3')')
	pushdef(`SIZE', `sizeof($2)ifelse(`$3', `', `', ` * $3')')
	pushdef(`REALTYPE', `ifelse(`$5', , `$2', `$5')')
	/* allocate */
	IFCALC(`
		IFAUTOCHECK(`
			xc_stack_push(&processor->allocsizes, (void *) (long) (SIZE));
			xc_stack_push(&processor->allocsizes, (void *) (long) (__LINE__));
		')
		processor->size = (size_t) ALIGN(processor->size);
		processor->size += SIZE;
	')
	IFSTORE(`
		IFAUTOCHECK(`{
			if (!xc_stack_count(&processor->allocsizes)) {
				fprintf(stderr, "mismatch `$@' at line %d\n", __LINE__);
			}
			else {
				unsigned long expect = (unsigned long) xc_stack_pop(&processor->allocsizes);
				unsigned long atline = (unsigned long) xc_stack_pop(&processor->allocsizes);
				unsigned long real = SIZE;
				if (expect != real) {
					fprintf(stderr, "mismatch `$@' at line %d(was %lu): real %lu - expect %lu = %lu\n", __LINE__, atline, real, expect, real - expect);
				}
			}
		}')
		ifdef(`DEBUG_SIZE', ` {
			void *oldp = processor->p;
		')
		$1 = (REALTYPE *) (processor->p = (char *) ALIGN(processor->p));
		ifelse(`$4', `', `
				IFAUTOCHECK(`memsetptr($1, (void *) (unsigned long) __LINE__, SIZE);')
			', `
				memset($1, 0, SIZE);
		')
		processor->p += SIZE;

		ifdef(`DEBUG_SIZE', `
			xc_totalsize += (char *) processor->p - (char *) oldp;
			fprintf(stderr, "%d\t%d\t`'SIZE()\n", (char *) processor->p - (char *) oldp, xc_totalsize);
		}
		')
	')
	IFRESTORE(`ifelse(`$4', `', `
			ifelse(
				REALTYPE*COUNT, `zval*1', `ALLOC_ZVAL($1);',
				REALTYPE*COUNT, `HashTable*1', `ALLOC_HASHTABLE($1);',
				`', `', `$1 = (REALTYPE *) emalloc(SIZE);')
			IFAUTOCHECK(`memsetptr($1, (void *) __LINE__, SIZE);')
		', `
			$1 = (REALTYPE *) ecalloc(COUNT, sizeof($2));
		')
	')
	popdef(`REALTYPE')
	popdef(`COUNT')
	popdef(`SIZE')
')
dnl CALLOC(1:dst, 2:type [, 3:count=1, 4:realtype=$2 ])
define(`CALLOC', `ALLOC(`$1', `$2', `$3', `1', `$4')')
dnl }}}
dnl {{{ PROC_CLASS_ENTRY_P(1:elm)
define(`PROC_CLASS_ENTRY_P', `PROC_CLASS_ENTRY_P_EX(`dst->$1', `SRC(`$1')', `$1')`'DONE(`$1')')
dnl PROC_CLASS_ENTRY_P_EX(1:dst, 2:src, 3:elm-name)
define(`PROC_CLASS_ENTRY_P_EX', `
	if ($2) {
		IFSTORE(`$1 = (zend_class_entry *) xc_get_class_num(processor, $2);')
		IFRESTORE(`$1 = xc_get_class(processor, (zend_ulong) $2);')
#ifdef IS_UNICODE
		IFDASM(`add_assoc_unicodel_ex(dst, ZEND_STRS("$3"), ZSTR_U($2->name), $2->name_length, 1);')
#else
		IFDASM(`add_assoc_stringl_ex(dst, ZEND_STRS("$3"), (char *) $2->name, $2->name_length, 1);')
#endif
	}
	else {
		COPYNULL_EX(`$1', `$3')
	}
')
dnl }}}
dnl {{{ IFAUTOCHECKEX
define(`IFAUTOCHECKEX', `ifdef(`XCACHE_ENABLE_TEST', `$1', `$2')')
dnl }}}
dnl {{{ IFAUTOCHECK
define(`IFAUTOCHECK', `IFAUTOCHECKEX(`
#ifndef NDEBUG
		$1
#endif
')')
dnl }}}
dnl {{{ DBG
define(`DBG', `ifdef(`XCACHE_ENABLE_TEST', `
	/* `$1' */
')')
dnl }}}
dnl {{{ EXPORT
define(`EXPORT', `define(`EXPORT_$1')')
dnl }}}
dnl {{{ FIXPOINTER
define(`FIXPOINTER', `FIXPOINTER_EX(`$1', `dst->$2')')
define(`FIXPOINTER_EX', `IFSTORE(`
	$2 = ($1 *) processor->shm->handlers->to_readonly(processor->shm, (char *)$2);
')')
define(`UNFIXPOINTER', `UNFIXPOINTER_EX(`$1', `dst->$2')')
define(`UNFIXPOINTER_EX', `IFSTORE(`
	$2 = ($1 *) processor->shm->handlers->to_readwrite(processor->shm, (char *)$2);
')')
dnl }}}
dnl {{{ COPY
define(`COPY', `IFNOTMEMCPY(`IFCOPY(`dst->$1 = SRC(`$1');')')DONE(`$1')')
dnl }}}
dnl {{{ COPY_N_EX
define(`COPY_N_EX', `
	ALLOC(`dst->$3', `$2', `SRC(`$1')')
	IFCOPY(`
		memcpy(dst->$3, SRC(`$3'), sizeof(dst->$3[0]) * SRC(`$1'));
		')
')
dnl }}}
dnl {{{ COPY_N
define(`COPY_N', `COPY_N_EX(`$1',`$2')DONE(`$1')')
dnl }}}
dnl {{{ COPYPOINTER
define(`COPYPOINTER', `COPY(`$1')')
dnl }}}
dnl {{{ COPYARRAY_EX
define(`COPYARRAY_EX', `IFNOTMEMCPY(`IFCOPY(`memcpy(dst->$1, SRC(`$1'), sizeof(dst->$1));')')')
dnl }}}
dnl {{{ COPYARRAY
define(`COPYARRAY', `COPYARRAY_EX(`$1',`$2')DONE(`$1')')
dnl }}}
dnl {{{ SETNULL_EX
define(`SETNULL_EX', `IFCOPY(`$1 = NULL;')')
define(`SETNULL', `SETNULL_EX(`dst->$1')DONE(`$1')')
dnl }}}
dnl {{{ SETZERO_EX
define(`SETZERO_EX', `IFCOPY(`$1 = 0;')')
define(`SETZERO', `SETZERO_EX(`dst->$1')DONE(`$1')')
dnl }}}
dnl {{{ COPYNULL_EX(1:dst, 2:elm-name)
define(`COPYNULL_EX', `
	IFDASM(`add_assoc_null_ex(dst, ZEND_STRS("$2"));')
	IFNOTMEMCPY(`IFCOPY(`$1 = NULL;')')
	assert(patsubst($1, dst, src) == NULL);
')
dnl }}}
dnl {{{ COPYNULL(1:elm)
define(`COPYNULL', `
	COPYNULL_EX(`dst->$1', `$1')DONE(`$1')
')
dnl }}}
dnl {{{ COPYZERO_EX(1:dst, 2:elm-name)
define(`COPYZERO_EX', `
	IFDASM(`add_assoc_long_ex(dst, ZEND_STRS("$2"), 0);')
	IFNOTMEMCPY(`IFCOPY(`$1 = 0;')')
	assert(patsubst($1, dst, src) == 0);
')
dnl }}}
dnl {{{ COPYZERO(1:elm)
define(`COPYZERO', `
	COPYZERO_EX(`dst->$1', `$1')DONE(`$1')
')
dnl }}}
dnl {{{ LIST_DIFF(1:left-list, 2:right-list)
define(`foreach',
       `pushdef(`$1')_foreach(`$1', `$2', `$3')popdef(`$1')')
define(`_arg1', `$1')
define(`_foreach',                             
       `ifelse(`$2', `()', ,                       
       `define(`$1', _arg1$2)$3`'_foreach(`$1',
                                                       (shift$2),
                                                       `$3')')')
define(`LIST_DIFF', `dnl
foreach(`i', `($1)', `pushdef(`item_'defn(`i'))')dnl allocate variable for items in $1 
foreach(`i', `($2)', `pushdef(`item_'defn(`i'))undefine(`item_'defn(`i'))')dnl allocate variable for items in $2, and undefine it 
foreach(`i', `($1)', `ifdef(`item_'defn(`i'), `defn(`i') ')')dnl see what is still defined
foreach(`i', `($2)', `define(`item_'defn(`i'))popdef(`item_'defn(`i'))')dnl
foreach(`i', `($1)', `popdef(`item_'defn(`i'))')dnl
')
dnl }}}
dnl {{{ DONE_*
define(`DONE_SIZE', `IFAUTOCHECK(`dnl
	xc_autocheck_done_size += $1`';
	xc_autocheck_done_count ++;
')')
define(`DONE', `
	define(`ELEMENTS_DONE', defn(`ELEMENTS_DONE')`,"$1"')
	IFAUTOCHECK(`dnl
		if (zend_u_hash_exists(&xc_autocheck_done_names, IS_STRING, "$1", sizeof("$1"))) {
			fprintf(stderr
				, "duplicate field at %s `#'%d FUNC_NAME`' : %s\n"
				, __FILE__, __LINE__
				, "$1"
				);
		}
		else {
			zend_uchar b = 1;
			zend_hash_add(&xc_autocheck_done_names, "$1", sizeof("$1"), (void*)&b, sizeof(b), NULL);
		}
	')
	DONE_SIZE(`sizeof(SRC(`$1'))')
')
define(`DISABLECHECK', `
	pushdef(`DONE_SIZE')
	pushdef(`DONE')
$1
	popdef(`DONE_SIZE')
	popdef(`DONE')
')
dnl }}}
dnl {{{ IF**
define(`IFCALC', `ifelse(PROCESSOR_TYPE, `calc', `$1', `$2')')
define(`IFSTORE', `ifelse(PROCESSOR_TYPE, `store', `$1', `$2')')
define(`IFCALCSTORE', `IFSTORE(`$1', `IFCALC(`$1', `$2')')')
define(`IFRESTORE', `ifelse(PROCESSOR_TYPE, `restore', `$1', `$2')')
define(`IFCOPY', `IFSTORE(`$1', `IFRESTORE(`$1', `$2')')')
define(`IFCALCCOPY', `IFCALC(`$1', `IFCOPY(`$1', `$2')')')
define(`IFDPRINT', `ifelse(PROCESSOR_TYPE, `dprint', `$1', `$2')')
define(`IFASM', `ifelse(PROCESSOR_TYPE, `asm', `$1', `$2')')
define(`IFDASM', `ifelse(PROCESSOR_TYPE, `dasm', `$1', `$2')')
dnl }}}
EXPORT(`zend_op')
EXPORT(`zend_op_array')
EXPORT(`zend_function')
EXPORT(`HashTable_zend_function')
EXPORT(`zend_class_entry')
EXPORT(`xc_classinfo_t')
EXPORT(`xc_funcinfo_t')
EXPORT(`xc_entry_var_t')
EXPORT(`xc_entry_php_t')
EXPORT(`xc_entry_data_php_t')
EXPORT(`zval')

include(srcdir`/processor/hashtable.m4')
include(srcdir`/processor/string.m4')
include(srcdir`/processor/struct.m4')
include(srcdir`/processor/process.m4')
include(srcdir`/processor/head.m4')

define(`IFNOTMEMCPY', `ifdef(`USEMEMCPY', `', `$1')')
REDEF(`PROCESSOR_TYPE', `calc') include(srcdir`/processor/processor.m4')
pushdef(`xc_get_class_num', ``xc_get_class_num'($@)')
REDEF(`PROCESSOR_TYPE', `store') include(srcdir`/processor/processor.m4')
popdef(`xc_get_class_num')
pushdef(`xc_get_class', ``xc_get_class'($@)')
REDEF(`PROCESSOR_TYPE', `restore') include(srcdir`/processor/processor.m4')
popdef(`xc_get_class')

REDEF(`IFNOTMEMCPY', `$1')
#ifdef HAVE_XCACHE_DPRINT
REDEF(`PROCESSOR_TYPE', `dprint') include(srcdir`/processor/processor.m4')
#endif /* HAVE_XCACHE_DPRINT */
#ifdef HAVE_XCACHE_DISASSEMBLER
REDEF(`PROCESSOR_TYPE', `dasm') include(srcdir`/processor/processor.m4')
#endif /* HAVE_XCACHE_DISASSEMBLER */
#ifdef HAVE_XCACHE_ASSEMBLER
REDEF(`PROCESSOR_TYPE', `asm') include(srcdir`/processor/processor.m4')
#endif /* HAVE_XCACHE_ASSEMBLER */

ifdef(`EXIT_PENDING', `m4exit(EXIT_PENDING)')
