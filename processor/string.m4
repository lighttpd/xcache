
dnl {{{ PROC_STRING_N_EX(1:dst, 2:src, 3:size, 4:name, 5:type=char)
define(`PROC_STRING_N_EX', `
	pushdef(`STRTYPE', `ifelse(`$5',,`char',`$5')')
	pushdef(`PTRTYPE', ifelse(
			STRTYPE, `char',      `char',
			STRTYPE, `zstr_char', `char',
			`',      `',          `UChar'))
	pushdef(`ISTYPE', ifelse(PTRTYPE,`UChar',IS_UNICODE,IS_STRING))
	pushdef(`UNI_STRLEN', ifelse(
			STRTYPE, `zstr_uchar', `xc_zstrlen_uchar',
			STRTYPE, `zstr_char',  `xc_zstrlen_char',
			`',      `',           `strlen'))
	pushdef(`SRCSTR', ifelse(STRTYPE,`char',`ZSTR($2)',STRTYPE,`UChar',`ZSTR($2)',`$2'))
	pushdef(`SRCPTR', ifelse(
			STRTYPE, `zstr_uchar', `ZSTR_U($2)',
			STRTYPE, `zstr_char',  `ZSTR_S($2)',
			`',      `',           `$2'))
	pushdef(`DSTPTR', ifelse(
			STRTYPE, `zstr_uchar', `ZSTR_U($1)',
			STRTYPE, `zstr_char',  `ZSTR_S($1)',
			`',      `',           `$1'))
	pushdef(`STRDUP', ifelse(
			PTRTYPE, `char',  `estrndup',
			PTRTYPE, `UChar', `eustrndup'))
	if (SRCPTR == NULL) {
		IFNOTMEMCPY(`IFCOPY(`
			DSTPTR = NULL;
		')')
		IFDASM(`
			add_assoc_null_ex(dst, XCACHE_STRS("$4"));
		')
	}
	else {
		IFDPRINT(`INDENT()
			ifelse(STRTYPE, `zstr_uchar', `
#ifdef IS_UNICODE
			do {
				zval zv;
				zval reszv;
				int usecopy;

				INIT_ZVAL(zv);
				ZVAL_UNICODEL(&zv, ZSTR_U($2), $3 - 1, 1);
				zend_make_printable_zval(&zv, &reszv, &usecopy);
				fprintf(stderr, "string:%s:\t\"", "$1");
				xc_dprint_str_len(Z_STRVAL(reszv), Z_STRLEN(reszv));
				fprintf(stderr, "\" len=%lu\n", (unsigned long) $3 - 1);
				if (usecopy) {
					zval_dtor(&reszv);
				}
				zval_dtor(&zv);
			} while (0);
#endif
			', `
			fprintf(stderr, "string:%s:\t\"", "$1");
			xc_dprint_str_len(SRCPTR, $3 - 1);
			fprintf(stderr, "\" len=%lu\n", (unsigned long) $3 - 1);
			')
		')
		IFCALC(`xc_calc_string_n(processor, ISTYPE, SRCSTR, $3 C_RELAYLINE);')
		IFSTORE(`DSTPTR = ifdef(`REALPTRTYPE', `(REALPTRTYPE() *)') ifelse(PTRTYPE,`char',`ZSTR_S',`ZSTR_U')(xc_store_string_n(processor, ISTYPE, SRCSTR, $3 C_RELAYLINE));')
		IFRESTORE(`
			DSTPTR = ifdef(`REALPTRTYPE', `(REALPTRTYPE() *)') STRDUP() (SRCPTR, ($3) - 1);
		')
		FIXPOINTER_EX(ifdef(`REALPTRTYPE', `REALPTRTYPE()', `PTRTYPE'), DSTPTR)
		IFDASM(`
			ifelse(STRTYPE,zstr_uchar, `
				add_assoc_unicodel_ex(dst, XCACHE_STRS("$4"), ZSTR_U($2), $3-1, 1);
				', ` dnl else
				ifelse(STRTYPE,zstr_char, `
					add_assoc_stringl_ex(dst, XCACHE_STRS("$4"), (char *) ZSTR_S($2), $3-1, 1);
					', `
					add_assoc_stringl_ex(dst, XCACHE_STRS("$4"), (char *) $2, $3-1, 1);
				')
			')
		')
	}
	popdef(`STRDUP')
	popdef(`DSTPTR')
	popdef(`SRCPTR')
	popdef(`SRCSTR')
	popdef(`UNI_STRLEN')
	popdef(`STRTYPE')
	popdef(`ISTYPE')
')
dnl }}}
dnl PROC_STRING_N(1:name, 2:size, 3:type)
define(`PROC_STRING_N', `DBG(`$0($*)') DONE(`$1')`'PROC_STRING_N_EX(`DST(`$1')', `SRC(`$1')', `SRC(`$2')', `$1', `char')')
define(`PROC_USTRING_N', `DBG(`$0($*)') DONE(`$1')`'PROC_STRING_N_EX(`DST(`$1')', `SRC(`$1')', `SRC(`$2')', `$1', `UChar')')

define(`PROC_STRING_L', `DBG(`$0($*)') PROC_STRING_N(`$1', `$2 + 1')')
define(`PROC_USTRING_L', `DBG(`$0($*)') PROC_USTRING_N(`$1', `$2 + 1')')
define(`PROC_STRING',   `DBG(`$0($*)') DONE(`$1')`'PROC_STRING_N_EX(`DST(`$1')', `SRC(`$1')', `strlen(SRC(`$1')) + 1', `$1', `char')')
define(`PROC_USTRING',  `DBG(`$0($*)') DONE(`$1')`'PROC_STRING_N_EX(`DST(`$1')', `SRC(`$1')', `strlen(SRC(`$1')) + 1', `$1', `UChar')')

dnl {{{ PROC_ZSTRING_N(1:type, 2:name, 3:size, 4:size_type)
define(`PROC_ZSTRING_N', `
	DBG(`$0($*)')
#ifdef IS_UNICODE
	pushdef(`NSIZE', ifelse(
			`$4', `strlen', `UNI_STRLEN (SRC(`$2')) + 1',
			`$4', `len',    `SRC(`$3') + 1',
			`',   `',       `SRC(`$3')',
			))
	DONE(`$2')
	ifelse(`$1', `1', `PROC_STRING_N_EX(`DST(`$2')', `SRC(`$2')', defn(`NSIZE'), `$2', `zstr_uchar')
	', `
		if (ifelse(`$1', `', `UG(unicode)', `SRC(`$1') == IS_UNICODE')) {
			PROC_STRING_N_EX(`DST(`$2')', `SRC(`$2')', defn(`NSIZE'), `$2', `zstr_uchar')
		}
		else {
			PROC_STRING_N_EX(`DST(`$2')', `SRC(`$2')', defn(`NSIZE'), `$2', `zstr_char')
		}
	')
#else
	DONE(`$2')
	PROC_STRING_N_EX(`DST(`$2')', `SRC(`$2')', NSIZE, `$2', `zstr_char')
#endif
	popdef(`NSIZE')
')
dnl }}}
define(`PROC_ZSTRING_L', `DBG(`$0($*)') PROC_ZSTRING_N(`$1', `$2', `$3', `len')')
define(`PROC_ZSTRING', `DBG(`$0($*)') PROC_ZSTRING_N(`$1', `$2', , `strlen')')
