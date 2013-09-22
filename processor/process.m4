define(`PROCESS_SCALAR', `dnl {{{ (1:elm, 2:format=%d, 3:type=)
	IFNOTMEMCPY(`IFCOPY(`DST(`$1') = SRC(`$1');')')
	IFDPRINT(`
		INDENT()
		fprintf(stderr, "$3:$1:\t%ifelse(`$2',`',`d',`$2')\n", SRC(`$1'));
	')
	IFDASM(`
		ifelse(
			`$3', `zend_bool', `add_assoc_bool_ex(dst, XCACHE_STRS("$1"), SRC(`$1') ? 1 : 0);'
		, `', `', `add_assoc_long_ex(dst, XCACHE_STRS("$1"), SRC(`$1'));'
		)
	')
	DONE(`$1')
')
dnl }}}
define(`PROCESS_xc_ztstring', `dnl {{{ (1:elm)
	pushdef(`REALPTRTYPE', `zend_class_entry')
	PROC_STRING(`$1')
	popdef(`REALPTRTYPE')
')
dnl }}}
define(`PROCESS_xc_zval_type_t', `dnl {{{ (1:elm)
	IFDPRINT(`
		INDENT()
		fprintf(stderr, ":$1:\t%d %s\n", SRC(`$1'), xc_get_data_type(SRC(`$1')));
		DONE(`$1')
	', `PROCESS_SCALAR(`$1')')
')
dnl }}}
define(`PROCESS_xc_op_type', `dnl {{{ (1:elm)
	IFDPRINT(`
		INDENT()
		fprintf(stderr, ":$1:\t%d %s\n", SRC(`$1'), xc_get_op_type(SRC(`$1')));
		DONE(`$1')
	', `PROCESS_SCALAR(`$1')')
')
dnl }}}
define(`PROCESS_xc_opcode', `dnl {{{ (1:elm)
	IFDPRINT(`
		INDENT()
		fprintf(stderr, ":$1:\t%u %s\n", SRC(`$1'), xc_get_opcode(SRC(`$1')));
		DONE(`$1')
	', `PROCESS_SCALAR(`$1')')
')
dnl }}}
define(`PROCESS', `dnl PROCESS(1:type, 2:elm)
	DBG(`$0($*)')
	assert(sizeof($1) == sizeof(SRC(`$2')));
	ifelse(
		`$1', `zend_bool',        `PROCESS_SCALAR(`$2', `u',  `$1')'
	, `$1', `zend_uchar',       `PROCESS_SCALAR(`$2', `u',  `$1')'
	, `$1', `char',             `PROCESS_SCALAR(`$2', `d',  `$1')'
	, `$1', `int32_t',          `PROCESS_SCALAR(`$2', `d',  `$1')'
	, `$1', `unsigned char',    `PROCESS_SCALAR(`$2', `u',  `$1')'
	, `$1', `zend_uint',        `PROCESS_SCALAR(`$2', `u',  `$1')'
	, `$1', `uint',             `PROCESS_SCALAR(`$2', `u',  `$1')'
	, `$1', `unsigned int',     `PROCESS_SCALAR(`$2', `u',  `$1')'
	, `$1', `zend_ulong',       `PROCESS_SCALAR(`$2', `lu', `$1')'
	, `$1', `ulong',            `PROCESS_SCALAR(`$2', `lu', `$1')'
	, `$1', `size_t',           `PROCESS_SCALAR(`$2', `lu', `$1')'
	, `$1', `long',             `PROCESS_SCALAR(`$2', `ld', `$1')'
	, `$1', `time_t',           `PROCESS_SCALAR(`$2', `ld', `$1')'
	, `$1', `zend_ushort',      `PROCESS_SCALAR(`$2', `hu', `$1')'
	, `$1', `int',              `PROCESS_SCALAR(`$2', `d',  `$1')'
	, `$1', `double',           `PROCESS_SCALAR(`$2', `f',  `$1')'
	, `$1', `xc_entry_type_t',  `PROCESS_SCALAR(`$2', `d',  `$1')'
	, `$1', `xc_hash_value_t',  `PROCESS_SCALAR(`$2', `lu', `$1')'
	, `$1', `last_brk_cont_t',  `PROCESS_SCALAR(`$2', `d', `$1')'

	, `$1', `xc_ztstring',       `PROCESS_xc_ztstring(`$2')'
	, `$1', `xc_zval_type_t',    `PROCESS_xc_zval_type_t(`$2')'
	, `$1', `xc_op_type',        `PROCESS_xc_op_type(`$2')'
	, `$1', `xc_opcode',         `PROCESS_xc_opcode(`$2')'
	, `$1', `opcode_handler_t',  `/* is copying enough? */COPY(`$2')'
	, `$1', `xc_md5sum_t',       `COPY(`$2')'
	, `', `', `m4_errprint(`AUTOCHECK ERROR: Unknown type "$1"')define(`EXIT_PENDING', 1)'
	)
')
define(`PROCESS_ARRAY', `dnl {{{ (1:count, 2:type, 3:elm, [4:real_type])
	if (SRC(`$3')) {
		int LOOPCOUNTER;
		IFDASM(`
			zval *arr;
			ALLOC_INIT_ZVAL(arr);
			array_init(arr);

			for (LOOPCOUNTER = 0;
					ifelse(`$1', `', `SRC(`$3[LOOPCOUNTER]')',
					`', `', `LOOPCOUNTER < SRC(`$1')');
					++LOOPCOUNTER) {
				pushdef(`dst', `arr')
				pushdef(`SRC', `ifelse(`$4', `', `', `', `', `($2)')' defn(`SRC') `[LOOPCOUNTER]')
				popdef(`add_assoc_bool_ex', `add_next_index_bool($1, $3)')
				popdef(`add_assoc_string_ex', `add_next_index_string($1, $3)')
				popdef(`add_assoc_long_ex', `add_next_index_long($1, $3)')
				popdef(`add_assoc_zval_ex', `add_next_index_zval($1, $3)')
				DISABLECHECK(`
					PROCESS(`$2', `$3')
				')
				popdef(`add_assoc_zval_ex')
				popdef(`add_assoc_long_ex')
				popdef(`add_assoc_string_ex')
				popdef(`add_assoc_bool_ex')
				popdef(`SRC')
				popdef(`dst')
			}
			add_assoc_zval_ex(dst, XCACHE_STRS("$3"), arr);
		', `
			dnl find count with NULL
			ifelse(`$1', `', `
				size_t count = 0;
				while (SRC(`$3[count]')) {
					++count;
				}
				++count;
				pushdef(`STRUCT_COUNT', `count')
			',
			`', `', `pushdef(`STRUCT_COUNT', `SRC(`$1')')')
			ALLOC(`DST(`$3')', `$2', `STRUCT_COUNT', , `$4')
			popdef(`STRUCT_COUNT')

			for (LOOPCOUNTER = 0;
					ifelse(`$1', `', `SRC(`$3[LOOPCOUNTER]')',
					`', `', `LOOPCOUNTER < SRC(`$1')');
					++LOOPCOUNTER) {
				DISABLECHECK(`
					pushdef(`DST', defn(`DST') `[LOOPCOUNTER]')
					pushdef(`SRC', `ifelse(`$4', `', `', `', `', `($2)')' defn(`SRC') `[LOOPCOUNTER]')
					PROCESS(`$2', `$3')
					popdef(`SRC')
					popdef(`DST')
				')
			}
			dnl the end marker
			ifelse(`$1', `', `IFCOPY(`DST(`$3[LOOPCOUNTER]') = NULL;')')
		')dnl IFDASM
		DONE(`$3')
	}
	else {
		COPYNULL(`$3')
	}
')
dnl }}}
