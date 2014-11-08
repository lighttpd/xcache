IFAUTOCHECK(`
#define RELAYLINE_DC , int relayline
#define RELAYLINE_CC , __LINE__
', `
#define RELAYLINE_DC
#define RELAYLINE_CC
')

ifdef(`XCACHE_ENABLE_TEST', `
#undef NDEBUG
#include <assert.h>
m4_errprint(`AUTOCHECK INFO: runtime autocheck Enabled (debug build)')
', `
m4_errprint(`AUTOCHECK INFO: runtime autocheck Disabled (optimized build)')
')
ifdef(`DEBUG_SIZE', `static int xc_totalsize = 0;')

#ifndef NDEBUG
#	undef inline
#define inline
#endif

#ifdef NDEBUG
	#define notnullable(ptr) (ptr)
#else
static inline void *notnullable(const void *ptr)
{
	assert(ptr);
	return (void *) ptr;
}
#endif

#ifdef HAVE_XCACHE_DPRINT
static void xc_dprint_indent(int indent) /* {{{ */
{
	int i;
	for (i = 0; i < indent; i ++) {
		fprintf(stderr, "  ");
	}
}
/* }}} */
static void xc_dprint_str_len(const char *str, int len) /* {{{ */
{
	const unsigned char *p = (const unsigned char *) str;
	int i;
	for (i = 0; i < len; i ++) {
		if (p[i] < 32 || p[i] == 127) {
			fprintf(stderr, "\\%03o", (unsigned int) p[i]);
		}
		else {
			fputc(p[i], stderr);
		}
	}
}
/* }}} */
#endif
/* {{{ field name checker */
IFAUTOCHECK(`dnl
static int xc_check_names(const char *file, int line, const char *functionName, const char **assert_names, size_t assert_names_count, HashTable *done_names)
{
	int errors = 0;
	if (assert_names_count) {
		size_t i;
		Bucket *b;

		for (i = 0; i < assert_names_count; ++i) {
			if (!zend_u_hash_exists(done_names, IS_STRING, assert_names[i], (uint) strlen(assert_names[i]) + 1)) {
				fprintf(stderr
					, "Error: missing field at %s `#'%d %s`' : %s\n"
					, file, line, functionName
					, assert_names[i]
					);
				++errors;
			}
		}

		for (b = done_names->pListHead; b != NULL; b = b->pListNext) {
			int known = 0;
			for (i = 0; i < assert_names_count; ++i) {
				if (strcmp(assert_names[i], BUCKET_KEY_S(b)) == 0) {
					known = 1;
					break;
				}
			}
			if (!known) {
				fprintf(stderr
					, "Error: unknown field at %s `#'%d %s`' : %s\n"
					, file, line, functionName
					, BUCKET_KEY_S(b)
					);
				++errors;
			}
		}
	}
	return errors;
}
')
/* }}} */
