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
