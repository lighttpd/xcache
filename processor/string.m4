
dnl {{{ PROC_STRING_N_EX(1:dst, 2:src, 3:size, 4:name, 5:type=char)
define(`PROC_STRING_N_EX', `
	pushdef(`STRTYPE', `ifelse(`$5',,`char',`$5')')
	pushdef(`ISTYPE', ifelse(STRTYPE,`char',IS_STRING,IS_UNICODE))
	if ($2 == NULL) {
		IFNOTMEMCPY(`IFCOPY(`
			$1 = NULL;
		')')
		IFDASM(`
			add_assoc_null_ex(dst, ZEND_STRS("$4"));
		')
	}
	else {
		IFDPRINT(`INDENT()
			ifelse(STRTYPE, `UChar', `
#ifdef IS_UNICODE
			do {
				zval zv;
				zval reszv;
				int usecopy;

				INIT_ZVAL(zv);
				ZVAL_UNICODEL(&zv, (UChar *) ($2), $3 - 1, 1);
				zend_make_printable_zval(&zv, &reszv, &usecopy);
				fprintf(stderr, "string:%s:\t\"%s\" len=%d\n", "$1", reszv.value.str.val, $3 - 1);
				if (usecopy) {
					zval_dtor(&reszv);
				}
				zval_dtor(&zv);
			} while (0);
#endif
			', `
			fprintf(stderr, "string:%s:\t\"%s\" len=%d\n", "$1", $2, $3 - 1);
			')
		')
		IFCALC(`xc_calc_string_n(processor, ISTYPE, (void *) $2, `$3' IFASSERT(`, __LINE__'));')
		IFSTORE(`$1 = (STRTYPE *) xc_store_string_n(processor, ISTYPE, (char *) $2, `$3' IFASSERT(`, __LINE__'));')
		IFRESTORE(`
			ALLOC(`$1', `STRTYPE', `sizeof(STRTYPE) * ($3)')
			memcpy($1, $2, sizeof(STRTYPE) * ($3));
		')
		FIXPOINTER_EX(`STRTYPE', `$1')
		IFDASM(`
				ifelse(STRTYPE,UChar, `
					add_assoc_unicodel_ex(dst, ZEND_STRS("$4"), $2, $3-1, 1);
					', ` dnl else
					add_assoc_stringl_ex(dst, ZEND_STRS("$4"), $2, $3-1, 1);')
				')
	}
	popdef(`STRTYPE')
	popdef(`ISTYPE')
')
dnl }}}
dnl PROC_STRING_N(1:name, 2:size, 3:type)
define(`PROC_STRING_N', `DBG(`$0($*)') DONE(`$1')`'PROC_STRING_N_EX(`dst->$1', `src->$1', `src->$2', `$1', `char')')

define(`PROC_STRING_L', `DBG(`$0($*)') PROC_STRING_N(`$1', `$2 + 1')')
define(`PROC_STRING',   `DBG(`$0($*)') DONE(`$1')`'PROC_STRING_N_EX(`dst->$1', `src->$1', `strlen(src->$1) + 1', `$1', `char')')

dnl {{{ PROC_USTRING_N(1:type, 2:name, 3:size, 4:size_type)
define(`PROC_USTRING_N', `
	DBG(`$0($*)')
#ifdef IS_UNICODE
	pushdef(`NSIZE', ifelse(
			`$4', `strlen', `strlen(src->$2) + 1',
			`$4', `len',    `src->$3 + 1',
			`',   `',       `src->$3',
			))
	DONE(`$2')
	ifelse(`$1', `1', `PROC_STRING_N_EX(`dst->$2', `src->$2', NSIZE, `$2', `UChar')
	', `
		if (ifelse(`$1', `', `UG(unicode)', `src->$1')) {
			PROC_STRING_N_EX(`dst->$2', `src->$2', NSIZE, `$2', `UChar')
		}
		else {
			PROC_STRING_N_EX(`dst->$2', `src->$2', NSIZE, `$2', `char')
		}
	')
#else
	DONE(`$2')
	PROC_STRING_N_EX(`dst->$2', `src->$2', NSIZE, `$2', `char')
#endif
	popdef(`NSIZE')
')
// }}}
define(`PROC_USTRING_L', `DBG(`$0($*)') PROC_USTRING_N(`$1', `$2', `$3', `len')')
define(`PROC_USTRING', `DBG(`$0($*)') PROC_USTRING_N(`$1', `$2', , `strlen')')
