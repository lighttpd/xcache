define(`SIZEOF_zend_uint', `sizeof(zend_uint)')
define(`COUNTOF_zend_uint', `1')
define(`SIZEOF_int', `sizeof(int)')
define(`COUNTOF_int', `1')
define(`SIZEOF_zend_function', `sizeof(zend_function)')
define(`COUNTOF_zend_function', `1')
define(`SIZEOF_zval_ptr', `sizeof(zval_ptr)')
define(`COUNTOF_zval_ptr', `1')
define(`SIZEOF_zval_ptr_nullable', `sizeof(zval_ptr_nullable)')
define(`COUNTOF_zval_ptr_nullable', `1')
define(`SIZEOF_zend_trait_alias_ptr', `sizeof(zend_trait_alias)')
define(`COUNTOF_zend_trait_alias_ptr', `1')
define(`SIZEOF_zend_trait_precedence_ptr', `sizeof(zend_trait_precedence)')
define(`COUNTOF_zend_trait_precedence_ptr', `1')
define(`SIZEOF_xc_entry_name_t', `sizeof(xc_entry_name_t)')
define(`COUNTOF_xc_entry_name_t', `1')
define(`SIZEOF_xc_ztstring', `sizeof(xc_ztstring)')
define(`COUNTOF_xc_ztstring', `1')

typedef zval *zval_ptr;
typedef zval *zval_ptr_nullable;
typedef char *xc_ztstring;
#ifdef ZEND_ENGINE_2_4
typedef zend_trait_alias *zend_trait_alias_ptr;
typedef zend_trait_precedence *zend_trait_precedence_ptr;
#endif
#ifdef ZEND_ENGINE_2_3
typedef int last_brk_cont_t;
#else
typedef zend_uint last_brk_cont_t;
#endif

typedef zend_uchar xc_zval_type_t;
typedef int xc_op_type;
typedef zend_uchar xc_opcode;
#ifdef IS_UNICODE
typedef UChar zstr_uchar;
#endif
typedef char  zstr_char;

EXPORT(`typedef struct _xc_dasm_t { const zend_op_array *active_op_array_src; } xc_dasm_t;')

