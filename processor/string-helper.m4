#define MAX_DUP_STR_LEN 256

define(`STRING_HELPER_T', `
HashTable strings;
')

define(`STRING_HELPERS', `
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
static inline void xc_calc_string_n(xc_processor_t *processor, zend_uchar type, const_zstr str, long size RELAYLINE_DC TSRMLS_DC) { /* {{{ */
	pushdef(`PROCESSOR_TYPE', `calc')
	pushdef(`__LINE__', `relayline')
	size_t realsize = UNISW(size, (type == IS_UNICODE) ? UBYTES(size) : size);
	long dummy = 1;

	if (realsize > MAX_DUP_STR_LEN) {
		ALLOC(, char, realsize)
	}
	else if (zend_u_hash_add(&processor->strings, type, str, (uint) size, (void *) &dummy, sizeof(dummy), NULL) == SUCCESS) {
		/* new string */
		ALLOC(, char, realsize)
	} 
	IFAUTOCHECK(`
		else {
			dnl fprintf(stderr, "dupstr %s\n", ZSTR_S(str));
		}
	')
	popdef(`__LINE__')
	popdef(`PROCESSOR_TYPE')
}
/* }}} */
static inline zstr xc_store_string_n(xc_processor_t *processor, zend_uchar type, const_zstr str, long size RELAYLINE_DC) { /* {{{ */
	pushdef(`PROCESSOR_TYPE', `store')
	pushdef(`__LINE__', `relayline')
	size_t realsize = UNISW(size, (type == IS_UNICODE) ? UBYTES(size) : size);
	zstr ret, *pret;

	if (realsize > MAX_DUP_STR_LEN) {
		ALLOC(ZSTR_V(ret), char, realsize)
		memcpy(ZSTR_V(ret), ZSTR_V(str), realsize);
		return ret;
	}

	if (zend_u_hash_find(&processor->strings, type, str, (uint) size, (void **) &pret) == SUCCESS) {
		TRACE("found old string %s:%ld %p", str, size, *pret);
		return *pret;
	}

	/* new string */
	ALLOC(ZSTR_V(ret), char, realsize)
	memcpy(ZSTR_V(ret), ZSTR_V(str), realsize);
	zend_u_hash_add(&processor->strings, type, str, (uint) size, (void *) &ret, sizeof(zstr), NULL);
	TRACE("stored new string %s:%ld %p", str, size, ret);
	return ret;

	popdef(`__LINE__')
	popdef(`PROCESSOR_TYPE')
}
/* }}} */
')
