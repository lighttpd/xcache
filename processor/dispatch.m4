dnl DISPATCH(1:type, 2:elm)
define(`DISPATCH', `
	DBG(`$0($*)')
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
	, `$1', `xc_md5sum_t',      `/* is copying enough? */COPY(`$2')'
	, `', `', `m4_errprint(`Unknown type "$1"')'
	)
')
