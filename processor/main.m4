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
dnl ============
define(`INDENT', `xc_dprint_indent(indent);')
dnl }}}
dnl {{{ ALLOC(1:dst, 2:type, 3:count=1, 4:clean=false, 5:forcetype=$2)
define(`ALLOC', `
	pushdef(`COUNT', `ifelse(`$3', `', `1', `$3')')
	pushdef(`SIZE', `sizeof($2)ifelse(`$3', `', `', ` * $3')')
	pushdef(`FORCETYPE', `ifelse(`$5', , `$2', `$5')')
	/* allocate */
	IFCALC(`
		IFASSERT(`
			xc_stack_push(&processor->allocsizes, (void *) (long) (SIZE));
			xc_stack_push(&processor->allocsizes, (void *) (long) (__LINE__));
		')
		processor->size = (size_t) ALIGN(processor->size);
		processor->size += SIZE;
	')
	IFSTORE(`
		IFASSERT(`{
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
		$1 = (FORCETYPE *) (processor->p = (char *) ALIGN(processor->p));
		ifelse(`$4', `', `
				IFASSERT(`memset($1, -1, SIZE);')
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
				FORCETYPE*COUNT, `zval*1', `ALLOC_ZVAL($1);',
				FORCETYPE*COUNT, `HashTable*1', `ALLOC_HASHTABLE($1);',
				`', `', `$1 = (FORCETYPE *) emalloc(SIZE);')
			IFASSERT(`memset($1, -1, SIZE);')
		', `
			$1 = (FORCETYPE *) ecalloc(COUNT, sizeof($2));
		')
	')
	popdef(`COUNT')
	popdef(`SIZE')
')
dnl CALLOC(1:dst, 2:type [, 3:count=1 ])
define(`CALLOC', `ALLOC(`$1', `$2', `$3', `1')')
dnl }}}
dnl {{{ PROC_INT(1:elm, 2:format=%d, 3:type=, 4:spec=)
define(`PROC_INT', `
	IFNOTMEMCPY(`IFCOPY(`dst->$1 = src->$1;')')
	IFDPRINT(`
		INDENT()
		ifelse(
			`$3 $1', `zval_data_type type', `fprintf(stderr, "$3:$1:\t%d %s\n", src->$1, xc_get_data_type(src->$1));'
		, `$3 $1', `int op_type', `fprintf(stderr, "$3:$1:\t%d %s\n", src->$1, xc_get_op_type(src->$1));'
		, `$3 $1', `zend_uchar opcode', `fprintf(stderr, "$3:$1:\t%d %s\n", src->$1, xc_get_opcode(src->$1));'
		, `', `', `fprintf(stderr, "$3:$1:\t%ifelse(`$2',`',`d',`$2')\n", src->$1);')
	')
	IFDASM(`
		ifelse(
			`$3', `zend_bool', `add_assoc_bool_ex(dst, ZEND_STRS("$1"), src->$1 ? 1 : 0);'
		, `', `', `add_assoc_long_ex(dst, ZEND_STRS("$1"), src->$1);'
		)
	')
	DONE(`$1')
')
dnl }}}
dnl {{{ PROC_CLASS_ENTRY_P(1:elm)
define(`PROC_CLASS_ENTRY_P', `PROC_CLASS_ENTRY_P_EX(`dst->$1', `src->$1', `$1')`'DONE(`$1')')
dnl PROC_CLASS_ENTRY_P_EX(1:dst, 2:src, 3:elm-name)
define(`PROC_CLASS_ENTRY_P_EX', `
	if ($2) {
		IFSTORE(`$1 = (zend_class_entry *) xc_get_class_num(processor, $2);')
		IFRESTORE(`$1 = xc_get_class(processor, (zend_ulong) $2);')
#ifdef IS_UNICODE
		IFDASM(`add_assoc_unicodel_ex(dst, ZEND_STRS("$3"), ZSTR_U($2->name), $2->name_length, 1);')
#else
		IFDASM(`add_assoc_stringl_ex(dst, ZEND_STRS("$3"), $2->name, $2->name_length, 1);')
#endif
	}
	else {
		COPYNULL_EX(`$1', `$3')
	}
')
dnl }}}
dnl {{{ IFASSERTEX
define(`IFASSERTEX', `ifdef(`XCACHE_ENABLE_TEST', `$1', `$2')')
dnl }}}
dnl {{{ IFASSERT
define(`IFASSERT', `IFASSERTEX(`
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
	$2 = ($1 *) processor->cache->shm->handlers->to_readonly(processor->cache->shm, (char *)$2);
')')
define(`UNFIXPOINTER', `UNFIXPOINTER_EX(`$1', `dst->$2')')
define(`UNFIXPOINTER_EX', `IFSTORE(`
	$2 = ($1 *) processor->cache->shm->handlers->to_readwrite(processor->cache->shm, (char *)$2);
')')
dnl }}}
dnl {{{ COPY
define(`COPY', `IFNOTMEMCPY(`IFCOPY(`dst->$1 = src->$1;')')DONE(`$1')')
dnl }}}
dnl {{{ COPY_N_EX
define(`COPY_N_EX', `
	ALLOC(`dst->$3', `$2', `src->$1')
	IFCOPY(`
		memcpy(dst->$3, src->$3, sizeof(dst->$3[0]) * src->$1);
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
define(`COPYARRAY_EX', `IFNOTMEMCPY(`IFCOPY(`memcpy(dst->$1, src->$1, sizeof(dst->$1));')')')
dnl }}}
dnl {{{ COPYARRAY
define(`COPYARRAY', `COPYARRAY_EX(`$1',`$2')DONE(`$1')')
dnl }}}
dnl {{{ SETNULL_EX
define(`SETNULL_EX', `IFCOPY(`$1 = NULL;')')
define(`SETNULL', `SETNULL_EX(`dst->$1')DONE(`$1')')
dnl }}}
dnl {{{ COPYNULL_EX(1:dst, 2:elm-name)
define(`COPYNULL_EX', `
	IFDASM(`add_assoc_null_ex(dst, ZEND_STRS("$2"));')
	IFNOTMEMCPY(`IFCOPY(`$1 = NULL;')')
')
dnl }}}
dnl {{{ COPYNULL(1:elm)
define(`COPYNULL', `
	COPYNULL_EX(`dst->$1', `$2')DONE(`$1')
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
define(`DONE_SIZE', `IFASSERT(`dnl
	done_size += $1`';
	done_count ++;
')')
define(`DONE', `
	define(`ELEMENTS_DONE', defn(`ELEMENTS_DONE')`,"$1"')
	IFASSERT(`dnl
		if (zend_hash_exists(&done_names, "$1", sizeof("$1"))) {
			fprintf(stderr
				, "duplicate field at %s `#'%d FUNC_NAME`' : %s\n"
				, __FILE__, __LINE__
				, "$1"
				);
		}
		else {
			zend_uchar b = 1;
			zend_hash_add(&done_names, "$1", sizeof("$1"), (void*)&b, sizeof(b), NULL);
		}
	')
	DONE_SIZE(`sizeof(src->$1)')
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
define(`IFCALC', `ifelse(KIND, `calc', `$1', `$2')')
define(`IFSTORE', `ifelse(KIND, `store', `$1', `$2')')
define(`IFCALCSTORE', `IFSTORE(`$1', `IFCALC(`$1', `$2')')')
define(`IFRESTORE', `ifelse(KIND, `restore', `$1', `$2')')
define(`IFCOPY', `IFSTORE(`$1', `IFRESTORE(`$1', `$2')')')
define(`IFCALCCOPY', `IFCALC(`$1', `IFCOPY(`$1', `$2')')')
define(`IFDPRINT', `ifelse(KIND, `dprint', `$1', `$2')')
define(`IFASM', `ifelse(KIND, `asm', `$1', `$2')')
define(`IFDASM', `ifelse(KIND, `dasm', `$1', `$2')')
dnl }}}
EXPORT(`zend_op')
EXPORT(`zend_op_array')
EXPORT(`zend_function')
EXPORT(`HashTable_zend_function')
EXPORT(`zend_class_entry')
EXPORT(`xc_classinfo_t')
EXPORT(`xc_funcinfo_t')
EXPORT(`xc_entry_t')
EXPORT(`xc_entry_data_php_t')
EXPORT(`zval')

include(srcdir`/processor/hashtable.m4')
include(srcdir`/processor/string.m4')
include(srcdir`/processor/struct.m4')
include(srcdir`/processor/dispatch.m4')
include(srcdir`/processor/head.m4')

define(`IFNOTMEMCPY', `ifdef(`USEMEMCPY', `', `$1')')
REDEF(`KIND', `calc') include(srcdir`/processor/processor.m4')
pushdef(`xc_get_class_num', ``xc_get_class_num'($@)')
REDEF(`KIND', `store') include(srcdir`/processor/processor.m4')
popdef(`xc_get_class_num')
pushdef(`xc_get_class', ``xc_get_class'($@)')
REDEF(`KIND', `restore') include(srcdir`/processor/processor.m4')
popdef(`xc_get_class')

REDEF(`IFNOTMEMCPY', `$1')
#ifdef HAVE_XCACHE_DPRINT
REDEF(`KIND', `dprint') include(srcdir`/processor/processor.m4')
#endif /* HAVE_XCACHE_DPRINT */
#ifdef HAVE_XCACHE_DISASSEMBLER
REDEF(`KIND', `dasm') include(srcdir`/processor/processor.m4')
#endif /* HAVE_XCACHE_DISASSEMBLER */
#ifdef HAVE_XCACHE_ASSEMBLER
REDEF(`KIND', `asm') include(srcdir`/processor/processor.m4')
#endif /* HAVE_XCACHE_ASSEMBLER */

ifdef(`EXIT_PENDING', `m4exit(EXIT_PENDING)')
