#define MAX_DUP_STR_LEN 256

static inline size_t xc_zstrlen_char(const_zstr s) /* {{{ */
{
	return strlen(ZSTR_S(s));
}
/* }}} */
#ifdef IS_UNICODE
static inline size_t xc_zstrlen_uchar(zstr s) /* {{{ */
{
	return u_strlen(ZSTR_U(s));
}
/* }}} */
static inline size_t xc_zstrlen(int type, const_zstr s) /* {{{ */
{
	return type == IS_UNICODE ? xc_zstrlen_uchar(s) : xc_zstrlen_char(s);
}
/* }}} */
#else
/* {{{ xc_zstrlen */
#define xc_zstrlen(dummy, s) xc_zstrlen_char(s)
/* }}} */
#endif
