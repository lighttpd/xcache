divert(-1)
dnl ================ start ======================
define(`XCACHE_ENABLE_TEST')
define(`USEMEMCPY')

dnl ================ main

dnl {{{ basic
define(`REDEF', `undefine(`$1') define(`$1', `$2')')
define(`ONCE', `ifdef(`ONCE $1', `', `define(`ONCE $1')$1')')
define(`m4_errprint', `ONCE(`errprint(`$1
')')')
dnl ============
define(`INDENT', `xc_dprint_indent(indent);')
dnl }}}
dnl {{{ ALLOC(1:dst, 2:type, 3:count=1, 4:clean=false, 5:forcetype=$2)
dnl don't use typeof(dst) as we need the compiler warning
define(`ALLOC', `
	pushdef(`COUNT', `ifelse(`$3', `', `1', `$3')')
	pushdef(`SIZE', `sizeof($2)ifelse(`$3', `', `', ` * $3')')
	pushdef(`FORCETYPE', `ifelse(`$5', , `$2', `$5')')
	/* allocate */
	IFCALC(`
		IFASSERT(`
			xc_stack_push(&processor->allocsizes, (void*)(SIZE));
			xc_stack_push(&processor->allocsizes, (void*)(__LINE__));
		')
		processor->size = (size_t) ALIGN(processor->size);
		processor->size += SIZE;
	')
	IFSTORE(`
		IFASSERT(`{
			if (!xc_stack_size(&processor->allocsizes)) {
				fprintf(stderr, "mismatch `$@' at line %d\n", __LINE__);
			}
			else {
				int expect = (int)xc_stack_pop(&processor->allocsizes);
				int atline = (int)xc_stack_pop(&processor->allocsizes);
				int real = SIZE;
				if (expect != real) {
					fprintf(stderr, "mismatch `$@' at line %d(was %d): real %d - expect %d = %d\n", __LINE__, atline, real, expect, real - expect);
				}
			}
		}')
		$1 = (FORCETYPE *) (processor->p = (char *) ALIGN(processor->p));
		ifelse(`$4', `', `
				IFASSERT(`memset($1, -1, SIZE);')
			', `
				memset($1, 0, SIZE);
		')
		processor->p += SIZE;
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
dnl {{{ PROC_CLASS_ENTRY_P
define(`PROC_CLASS_ENTRY_P', `PROC_CLASS_ENTRY_P_EX(`dst->$1', `src->$1')`'DONE(`$1')')
define(`PROC_CLASS_ENTRY_P_EX', `
	if ($2) {
		IFSTORE(`$1 = (zend_class_entry *) xc_get_class_num(processor, $2);')
		IFRESTORE(`$1 = xc_get_class(processor, (zend_uint) $2);')
		IFDASM(`add_assoc_stringl_ex(dst, ZEND_STRS("patsubst(`$1', `dst->')"), $2->name, strlen($2->name), 1);')
	}
	else {
		COPYNULL_EX($1)
	}
')
dnl }}}
dnl {{{ IFASSERT
define(`IFASSERT', `ifdef(`XCACHE_ENABLE_TEST', `
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
	$2 = ($1 *) xc_shm_to_readonly(processor->xce_src->cache->shm, (char *)$2);
')')
define(`UNFIXPOINTER', `UNFIXPOINTER_EX(`$1', `dst->$2')')
define(`UNFIXPOINTER_EX', `IFSTORE(`
	$2 = ($1 *) xc_shm_to_readwrite(processor->xce_src->cache->shm, (char *)$2);
')')
dnl }}}
dnl {{{ COPY
define(`COPY', `IFNOTMEMCPY(`IFCOPY(`dst->$1 = src->$1;')')DONE(`$1')')
dnl }}}
dnl {{{ SETNULL_EX
define(`SETNULL_EX', `IFCOPY(`$1 = NULL;')')
define(`SETNULL', `SETNULL_EX(`dst->$1')DONE(`$1')')
dnl }}}
dnl {{{ COPYNULL_EX
define(`COPYNULL_EX', `
	IFDASM(`add_assoc_null_ex(dst, ZEND_STRS("patsubst(`$1', `dst->')"));')
	IFNOTMEMCPY(`IFCOPY(`$1 = NULL;')')
')
define(`COPYNULL', `
	COPYNULL_EX(`dst->$1')DONE(`$1')
')
dnl }}}
dnl {{{ DONE_*
define(`DONE_SIZE', `IFASSERT(`
	done_size += $1`';
	done_count ++;
')')
define(`DONE', `
	dnl ifelse(regexp(defn(`ELEMENTS'), ` $1'), -1, m4_errprint(`Unknown $1') m4exit(1))
	define(`ELEMENTS', patsubst(defn(`ELEMENTS'), ` $1\>'))
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
define(`IFCALCSTORE', `IFSTORE(`$1', `
	IFCALC(`$1', `$2')
')')
define(`IFRESTORE', `ifelse(KIND, `restore', `$1', `$2')')
define(`IFCOPY', `IFSTORE(`$1', `
	IFRESTORE(`$1', `$2')
')')
define(`IFCALCCOPY', `IFCALC(`$1', `
	IFCOPY(`$1', `$2')
')')
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
EXPORT(`zval')

include(hashtable.m4)
include(string.m4)
include(struct.m4)
include(dispatch.m4)
include(head.m4)

define(`IFNOTMEMCPY', `ifdef(`USEMEMCPY', `', `$1')')
REDEF(`KIND', `calc') include(processor.m4)
REDEF(`KIND', `store') include(processor.m4)
REDEF(`KIND', `restore') include(processor.m4)

REDEF(`IFNOTMEMCPY', `$1')
#ifdef HAVE_XCACHE_DPRINT
REDEF(`KIND', `dprint') include(processor.m4)
#endif /* HAVE_XCACHE_DPRINT */
#ifdef HAVE_XCACHE_DISASSEMBLER
REDEF(`KIND', `dasm') include(processor.m4)
#endif /* HAVE_XCACHE_DISASSEMBLER */
#ifdef HAVE_XCACHE_ASSEMBLER
REDEF(`KIND', `asm') include(processor.m4)
#endif /* HAVE_XCACHE_ASSEMBLER */

ifdef(`EXIT_PENDING', `m4exit(EXIT_PENDING)')
