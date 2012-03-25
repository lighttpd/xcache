dnl DISPATCH(1:type, 2:elm)
define(`DISPATCH', `
	DBG(`$0($*)')
	assert(sizeof($1) == sizeof(src->$2));
	ifelse(
		`$1', `zend_bool',        `PROC_INT(`$2', `u',  `$1')'
	, `$1', `zend_uchar',       `PROC_INT(`$2', `u',  `$1')'
	, `$1', `char',             `PROC_INT(`$2', `d',  `$1')'
	, `$1', `int32_t',          `PROC_INT(`$2', `d',  `$1')'
	, `$1', `unsigned char',    `PROC_INT(`$2', `u',  `$1')'
	, `$1', `zend_uint',        `PROC_INT(`$2', `u',  `$1')'
	, `$1', `uint',             `PROC_INT(`$2', `u',  `$1')'
	, `$1', `unsigned int',     `PROC_INT(`$2', `u',  `$1')'
	, `$1', `zend_ulong',       `PROC_INT(`$2', `lu', `$1')'
	, `$1', `ulong',            `PROC_INT(`$2', `lu', `$1')'
	, `$1', `size_t',           `PROC_INT(`$2', `u', `$1')'
	, `$1', `long',             `PROC_INT(`$2', `ld', `$1')'
	, `$1', `time_t',           `PROC_INT(`$2', `ld', `$1')'
	, `$1', `zend_ushort',      `PROC_INT(`$2', `hu', `$1')'
	, `$1', `int',              `PROC_INT(`$2', `d',  `$1')'
	, `$1', `double',           `PROC_INT(`$2', `f',  `$1')'
	, `$1', `opcode_handler_t', `/* is copying enough? */COPY(`$2')'
	, `$1', `zval_data_type',   `PROC_INT(`$2', `u',  `$1')'
	, `$1', `xc_entry_type_t',  `PROC_INT(`$2', `d',  `$1')'
	, `$1', `xc_hash_value_t',  `PROC_INT(`$2', `lu', `$1')'
	, `$1', `xc_md5sum_t',      `COPY(`$2')'
	, `', `', `m4_errprint(`Unknown type "$1"')'
	)
')
dnl {{{ DISPATCH_ARRAY(1:count, 2:type, 3:elm)
define(`DISPATCH_ARRAY', `
	if (src->$3) {
		int i;
		IFDASM(`
			zval *arr;
			ALLOC_INIT_ZVAL(arr);
			array_init(arr);
			for (i = 0; i < src->$1; i ++) {
				ifelse(
					`$2', `zend_bool', `add_assoc_bool_ex(arr, ZEND_STRS("$3"), src->$3[i] ? 1 : 0);'
				, `', `', `add_next_index_long(arr, src->$3[i]);')
			}
			add_assoc_zval_ex(dst, ZEND_STRS("$3"), arr);
		', `
			COPY_N_EX($@)
			for (i = 0; i < src->$1; i ++) {
				DISABLECHECK(`
					DISPATCH(`$2', `$3[i]', `$4')
				')
			}
		')dnl IFDASM
		DONE(`$3')
	}
	else {
		COPYNULL(`$3')
	}
')
dnl }}}
