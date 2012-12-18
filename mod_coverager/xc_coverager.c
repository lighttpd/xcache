#if 0
#define XCACHE_DEBUG
#endif

#include "xc_coverager.h"

#include <stdio.h>
#include "xcache.h"
#include "ext/standard/flock_compat.h"
#ifdef HAVE_SYS_FILE_H
#	include <sys/file.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "xcache/xc_extension.h"
#include "xcache/xc_ini.h"
#include "xcache/xc_utils.h"
#include "util/xc_stack.h"
#include "util/xc_trace.h"
#include "xcache_globals.h"

#include "ext/standard/info.h"
#include "zend_compile.h"

typedef HashTable *coverager_t;
#define PCOV_HEADER_MAGIC 0x564f4350

static char *xc_coveragedump_dir = NULL;
static zend_compile_file_t *old_compile_file = NULL;

/* dumper */
static void xc_destroy_coverage(void *pDest) /* {{{ */
{
	coverager_t cov = *(coverager_t*) pDest;
	TRACE("destroy %p", cov);
	zend_hash_destroy(cov);
	efree(cov);
}
/* }}} */
static void xcache_mkdirs_ex(char *root, long rootlen, char *path, long pathlen TSRMLS_DC) /* {{{ */
{
	char *fullpath;
	struct stat st;
	ALLOCA_FLAG(use_heap)

	TRACE("mkdirs %s %d %s %d", root, rootlen, path, pathlen);
	fullpath = xc_do_alloca(rootlen + pathlen + 1, use_heap);
	memcpy(fullpath, root, rootlen);
	memcpy(fullpath + rootlen, path, pathlen);
	fullpath[rootlen + pathlen] = '\0';

	if (stat(fullpath, &st) != 0) {
		char *chr;

		chr = strrchr(path, PHP_DIR_SEPARATOR);
		if (chr && chr != path) {
			*chr = '\0';
			xcache_mkdirs_ex(root, rootlen, path, chr - path TSRMLS_CC);
			*chr = PHP_DIR_SEPARATOR;
		}
		TRACE("mkdir %s", fullpath);
#if PHP_MAJOR_VERSION > 5
		php_stream_mkdir(fullpath, 0700, REPORT_ERRORS, NULL);
#else
		mkdir(fullpath, 0700);
#endif
	}
	xc_free_alloca(fullpath, use_heap);
}
/* }}} */
static void xc_coverager_save_cov(char *srcfile, char *outfilename, coverager_t cov TSRMLS_DC) /* {{{ */
{
	long *buf = NULL, *p;
	long covlines, *phits;
	int fd = -1;
	size_t size;
	int newfile;
	struct stat srcstat, outstat;
	HashPosition pos;
	char *contents = NULL;
	long len;

	if (stat(srcfile, &srcstat) != 0) {
		return;
	}

	newfile = 0;
	if (stat(outfilename, &outstat) != 0) {
		newfile = 1;
	}
	else {
		if (srcstat.st_mtime > outstat.st_mtime) {
			newfile = 1;
		}
	}

	fd = open(outfilename, O_RDWR | O_CREAT, 0600);
	if (fd < 0) {
		char *chr;
		chr = strrchr(srcfile, PHP_DIR_SEPARATOR);
		if (chr) {
			*chr = '\0';
			xcache_mkdirs_ex(xc_coveragedump_dir, strlen(xc_coveragedump_dir), srcfile, chr - srcfile TSRMLS_CC);
			*chr = PHP_DIR_SEPARATOR;
		}
		fd = open(outfilename, O_RDWR | O_CREAT, 0600);
		if (fd < 0) {
			goto bailout;
		}
	}
	if (flock(fd, LOCK_EX) != SUCCESS) {
		goto bailout;
	}

	if (newfile) {
		TRACE("%s", "new file");
	}
	else if (outstat.st_size) {
		len = outstat.st_size;
		contents = emalloc(len);
		if (read(fd, (void *) contents, len) != len) {
			goto bailout;
		}
		TRACE("oldsize %d", (int) len);
		do {
			p = (long *) contents;
			len -= sizeof(long);
			if (len < 0) {
				break;
			}
			if (*p++ != PCOV_HEADER_MAGIC) {
				TRACE("wrong magic in file %s", outfilename);
				break;
			}

			p += 2; /* skip covliens */
			len -= sizeof(long) * 2;
			if (len < 0) {
				break;
			}

			for (; len >= (int) sizeof(long) * 2; len -= sizeof(long) * 2, p += 2) {
				if (zend_hash_index_find(cov, p[0], (void**)&phits) == SUCCESS) {
					if (p[1] == -1) {
						/* OPTIMIZE: already marked */
						continue;
					}
					if (*phits != -1) {
						p[1] += *phits;
					}
				}
				zend_hash_index_update(cov, p[0], &p[1], sizeof(p[1]), NULL);
			}
		} while (0);
		efree(contents);
		contents = NULL;
	}


	/* serialize */
	size = (zend_hash_num_elements(cov) + 1) * sizeof(long) * 2 + sizeof(long);
	p = buf = emalloc(size);
	*p++ = PCOV_HEADER_MAGIC;
	p += 2; /* for covlines */
	covlines = 0;

	zend_hash_internal_pointer_reset_ex(cov, &pos);
	while (zend_hash_get_current_data_ex(cov, (void**)&phits, &pos) == SUCCESS) {
		*p++ = pos->h;
		*p++ = *phits;
		if (*phits > 0) {
			covlines ++;
		}
		zend_hash_move_forward_ex(cov, &pos);
	}
	p = buf + 1;
	p[0] = 0;
	p[1] = covlines;

	if (ftruncate(fd, 0) != 0) {
		goto bailout;
	}
	lseek(fd, 0, SEEK_SET);
	if (write(fd, (char *) buf, size) != (ssize_t) size) {
		goto bailout;
	}

bailout:
	if (contents) efree(contents);
	if (fd >= 0) close(fd);
	if (buf) efree(buf);
}
/* }}} */

static void xc_coverager_initenv(TSRMLS_D) /* {{{ */
{
	if (!XG(coverages)) {
		XG(coverages) = emalloc(sizeof(HashTable));
		zend_hash_init(XG(coverages), 0, NULL, xc_destroy_coverage, 0);
	}
}
/* }}} */
static void xc_coverager_clean(TSRMLS_D) /* {{{ */
{
	if (XG(coverages)) {
		HashPosition pos;
		coverager_t *pcov;

		zend_hash_internal_pointer_reset_ex(XG(coverages), &pos);
		while (zend_hash_get_current_data_ex(XG(coverages), (void **) &pcov, &pos) == SUCCESS) {
			long *phits;
			coverager_t cov;
			HashPosition pos2;

			cov = *pcov;

			zend_hash_internal_pointer_reset_ex(cov, &pos2);
			while (zend_hash_get_current_data_ex(cov, (void**)&phits, &pos2) == SUCCESS) {
				long hits = *phits;

				if (hits != -1) {
					hits = -1;
					zend_hash_index_update(cov, pos2->h, &hits, sizeof(hits), NULL);
				}
				zend_hash_move_forward_ex(cov, &pos2);
			}

			zend_hash_move_forward_ex(XG(coverages), &pos);
		}
	}
}
/* }}} */
static void xc_coverager_cleanup(TSRMLS_D) /* {{{ */
{
	if (XG(coverages)) {
		zend_hash_destroy(XG(coverages));
		efree(XG(coverages));
		XG(coverages) = NULL;
	}
}
/* }}} */

static void xc_coverager_start(TSRMLS_D) /* {{{ */
{
	XG(coverager_started) = 1;
}
/* }}} */
static void xc_coverager_stop(TSRMLS_D) /* {{{ */
{
	XG(coverager_started) = 0;
}
/* }}} */

static PHP_RINIT_FUNCTION(xcache_coverager) /* {{{ */
{
	if (XG(coverager)) {
		if (XG(coverager_autostart)) {
			xc_coverager_start(TSRMLS_C);
		}
#ifdef ZEND_COMPILE_EXTENDED_INFO
		CG(compiler_options) |= ZEND_COMPILE_EXTENDED_INFO;
#else
		CG(extended_info) = 1;
#endif
	}
	else {
		XG(coverager_started) = 0;
	}
	return SUCCESS;
}
/* }}} */
static void xc_coverager_autodump(TSRMLS_D) /* {{{ */
{
	coverager_t *pcov;
	zstr s;
	char *outfilename;
	size_t dumpdir_len, outfilelen, alloc_len = 0;
	uint size;
	HashPosition pos;

	if (XG(coverages) && xc_coveragedump_dir) {	
		dumpdir_len = strlen(xc_coveragedump_dir);
		alloc_len = dumpdir_len + 1 + 128;
		outfilename = emalloc(alloc_len);
		strcpy(outfilename, xc_coveragedump_dir);

		zend_hash_internal_pointer_reset_ex(XG(coverages), &pos);
		while (zend_hash_get_current_data_ex(XG(coverages), (void **) &pcov, &pos) == SUCCESS) {
			zend_hash_get_current_key_ex(XG(coverages), &s, &size, NULL, 0, &pos);
			outfilelen = dumpdir_len + size + 5;
			if (alloc_len < outfilelen) {
				alloc_len = outfilelen + 128;
				outfilename = erealloc(outfilename, alloc_len);
			}
			strcpy(outfilename + dumpdir_len, ZSTR_S(s));
			strcpy(outfilename + dumpdir_len + size - 1, ".pcov");

			TRACE("outfilename %s", outfilename);
			xc_coverager_save_cov(ZSTR_S(s), outfilename, *pcov TSRMLS_CC);
			zend_hash_move_forward_ex(XG(coverages), &pos);
		}
		efree(outfilename);
	}
}
/* }}} */
static void xc_coverager_dump(zval *return_value TSRMLS_DC) /* {{{ */
{
	coverager_t *pcov;
	HashPosition pos;

	if (XG(coverages)) {
		array_init(return_value);

		zend_hash_internal_pointer_reset_ex(XG(coverages), &pos);
		while (zend_hash_get_current_data_ex(XG(coverages), (void **) &pcov, &pos) == SUCCESS) {
			zval *lines;
			long *phits;
			coverager_t cov;
			HashPosition pos2;
			zstr filename;
			uint size;

			cov = *pcov;
			zend_hash_get_current_key_ex(XG(coverages), &filename, &size, NULL, 0, &pos);

			MAKE_STD_ZVAL(lines);
			array_init(lines);
			zend_hash_internal_pointer_reset_ex(cov, &pos2);
			while (zend_hash_get_current_data_ex(cov, (void**)&phits, &pos2) == SUCCESS) {
				long hits = *phits;
				add_index_long(lines, pos2->h, hits >= 0 ? hits : 0);
				zend_hash_move_forward_ex(cov, &pos2);
			}
			add_assoc_zval_ex(return_value, ZSTR_S(filename), (uint) strlen(ZSTR_S(filename)) + 1, lines);

			zend_hash_move_forward_ex(XG(coverages), &pos);
		}
	}
	else {
		RETVAL_NULL();
	}
}
/* }}} */
static PHP_RSHUTDOWN_FUNCTION(xcache_coverager) /* {{{ */
{
	if (XG(coverager)) {
		xc_coverager_autodump(TSRMLS_C);
		xc_coverager_cleanup(TSRMLS_C);
	}
	return SUCCESS;
}
/* }}} */

/* helper func to store hits into coverages */
static coverager_t xc_coverager_get(const char *filename TSRMLS_DC) /* {{{ */
{
	uint len = (uint) strlen(filename) + 1;
	coverager_t cov, *pcov;

	if (zend_u_hash_find(XG(coverages), IS_STRING, filename, len, (void **) &pcov) == SUCCESS) {
		TRACE("got coverage %s %p", filename, *pcov);
		return *pcov;
	}
	else {
		cov = emalloc(sizeof(HashTable));
		zend_hash_init(cov, 0, NULL, NULL, 0);
		zend_u_hash_add(XG(coverages), IS_STRING, filename, len, (void **) &cov, sizeof(cov), NULL);
		TRACE("new coverage %s %p", filename, cov);
		return cov;
	}
}
/* }}} */
static void xc_coverager_add_hits(HashTable *cov, long line, long hits TSRMLS_DC) /* {{{ */
{
	long *poldhits;

	if (line == 0) {
		return;
	}
	if (zend_hash_index_find(cov, line, (void**)&poldhits) == SUCCESS) {
		if (hits == -1) {
			/* OPTIMIZE: -1 == init-ing, but it's already initized */
			return;
		}
		if (*poldhits != -1) {
			hits += *poldhits;
		}
	}
	zend_hash_index_update(cov, line, &hits, sizeof(hits), NULL);
}
/* }}} */

static int xc_coverager_get_op_array_size_no_tail(zend_op_array *op_array) /* {{{ */
{
	zend_uint last = op_array->last;
	do {
next_op:
		if (last == 0) {
			break;
		}
		switch (op_array->opcodes[last - 1].opcode) {
#ifdef ZEND_HANDLE_EXCEPTION
			case ZEND_HANDLE_EXCEPTION:
#endif
			case ZEND_RETURN:
			case ZEND_EXT_STMT:
				--last;
				goto next_op;
		}
	} while (0);
	return last;
}
/* }}} */

/* prefill */
static int xc_coverager_init_op_array(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	zend_uint size;
	coverager_t cov;
	zend_uint i;

	if (op_array->type != ZEND_USER_FUNCTION) {
		return 0;
	}

	size = xc_coverager_get_op_array_size_no_tail(op_array);
	cov = xc_coverager_get(op_array->filename TSRMLS_CC);
	for (i = 0; i < size; i ++) {
		switch (op_array->opcodes[i].opcode) {
			case ZEND_EXT_STMT:
#if 0
			case ZEND_EXT_FCALL_BEGIN:
			case ZEND_EXT_FCALL_END:
#endif
				xc_coverager_add_hits(cov, op_array->opcodes[i].lineno, -1 TSRMLS_CC);
				break;
		}
	}
	return 0;
}
/* }}} */
static void xc_coverager_init_compile_result(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	xc_compile_result_t cr;

	xc_compile_result_init_cur(&cr, op_array TSRMLS_CC);
	xc_apply_op_array(&cr, (apply_func_t) xc_coverager_init_op_array TSRMLS_CC);
	xc_compile_result_free(&cr);
}
/* }}} */
static zend_op_array *xc_compile_file_for_coverage(zend_file_handle *h, int type TSRMLS_DC) /* {{{ */
{
	zend_op_array *op_array;

	op_array = old_compile_file(h, type TSRMLS_CC);
	if (op_array) {
		if (XG(coverager)) {
			xc_coverager_initenv(TSRMLS_C);
			xc_coverager_init_compile_result(op_array TSRMLS_CC);
		}
	}
	return op_array;
}
/* }}} */

/* hits */
static void xc_coverager_handle_ext_stmt(zend_op_array *op_array, zend_uchar op) /* {{{ */
{
	TSRMLS_FETCH();

	if (XG(coverages) && XG(coverager_started)) {
		int size = xc_coverager_get_op_array_size_no_tail(op_array);
		int oplineno = (int) ((*EG(opline_ptr)) - op_array->opcodes);
		if (oplineno < size) {
			xc_coverager_add_hits(xc_coverager_get(op_array->filename TSRMLS_CC), (*EG(opline_ptr))->lineno, 1 TSRMLS_CC);
		}
	}
}
/* }}} */

/* user api */
/* {{{ proto array xcache_coverager_decode(string data)
 * decode specified data which is saved by auto dumper to array
 */
PHP_FUNCTION(xcache_coverager_decode)
{
	char *str;
	int len;
	long *p;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &len) == FAILURE) {
		return;
	}

	array_init(return_value);

	p = (long*) str;
	len -= (int) sizeof(long);
	if (len < 0) {
		return;
	}
	if (*p++ != PCOV_HEADER_MAGIC) {
		TRACE("%s", "wrong magic in xcache_coverager_decode");
		return;
	}

	for (; len >= (int) sizeof(long) * 2; len -= (int) sizeof(long) * 2, p += 2) {
		add_index_long(return_value, p[0], p[1] < 0 ? 0 : p[1]);
	}
}
/* }}} */
/* {{{ proto void xcache_coverager_start([bool clean = true])
 * starts coverager data collecting
 */
PHP_FUNCTION(xcache_coverager_start)
{
	zend_bool clean = 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &clean) == FAILURE) {
		return;
	}

	if (clean) {
		xc_coverager_clean(TSRMLS_C);
	}

	if (XG(coverager)) {
		xc_coverager_start(TSRMLS_C);
	}
	else {
		php_error(E_WARNING, "You can only start coverager after you set 'xcache.coverager' to 'On' in ini");
	}
}
/* }}} */
/* {{{ proto void xcache_coverager_stop([bool clean = false])
 * stop coverager data collecting
 */
PHP_FUNCTION(xcache_coverager_stop)
{
	zend_bool clean = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &clean) == FAILURE) {
		return;
	}

	xc_coverager_stop(TSRMLS_C);
	if (clean) {
		xc_coverager_clean(TSRMLS_C);
	}
}
/* }}} */
/* {{{ proto array xcache_coverager_get([bool clean = false])
 * get coverager data collected
 */
PHP_FUNCTION(xcache_coverager_get)
{
	zend_bool clean = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &clean) == FAILURE) {
		return;
	}

	xc_coverager_dump(return_value TSRMLS_CC);
	if (clean) {
		xc_coverager_clean(TSRMLS_C);
	}
}
/* }}} */
static zend_function_entry xcache_coverager_functions[] = /* {{{ */
{
	PHP_FE(xcache_coverager_decode,  NULL)
	PHP_FE(xcache_coverager_start,   NULL)
	PHP_FE(xcache_coverager_stop,    NULL)
	PHP_FE(xcache_coverager_get,     NULL)
	PHP_FE_END
};
/* }}} */

static int xc_coverager_zend_startup(zend_extension *extension) /* {{{ */
{
	old_compile_file = zend_compile_file;
	zend_compile_file = xc_compile_file_for_coverage;

	return SUCCESS;
}
/* }}} */
static void xc_coverager_zend_shutdown(zend_extension *extension) /* {{{ */
{
	/* empty */
}
/* }}} */
static void xc_statement_handler(zend_op_array *op_array) /* {{{ */
{
	xc_coverager_handle_ext_stmt(op_array, ZEND_EXT_STMT);
}
/* }}} */
static void xc_fcall_begin_handler(zend_op_array *op_array) /* {{{ */
{
#if 0
	xc_coverager_handle_ext_stmt(op_array, ZEND_EXT_FCALL_BEGIN);
#endif
}
/* }}} */
static void xc_fcall_end_handler(zend_op_array *op_array) /* {{{ */
{
#if 0
	xc_coverager_handle_ext_stmt(op_array, ZEND_EXT_FCALL_END);
#endif
}
/* }}} */
/* {{{ zend extension definition structure */
static zend_extension xc_coverager_zend_extension_entry = {
	XCACHE_NAME " Coverager",
	XCACHE_VERSION,
	XCACHE_AUTHOR,
	XCACHE_URL,
	XCACHE_COPYRIGHT,
	xc_coverager_zend_startup,
	xc_coverager_zend_shutdown,
	NULL,           /* activate_func_t */
	NULL,           /* deactivate_func_t */
	NULL,           /* message_handler_func_t */
	NULL,           /* statement_handler_func_t */
	xc_statement_handler,
	xc_fcall_begin_handler,
	xc_fcall_end_handler,
	NULL,           /* op_array_ctor_func_t */
	NULL,           /* op_array_dtor_func_t */
	STANDARD_ZEND_EXTENSION_PROPERTIES
};
/* }}} */
/* {{{ PHP_INI */
PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("xcache.coverager",              "0", PHP_INI_SYSTEM|PHP_INI_PERDIR, OnUpdateBool, coverager,           zend_xcache_globals, xcache_globals)
	STD_PHP_INI_BOOLEAN("xcache.coverager_autostart",    "1", PHP_INI_SYSTEM|PHP_INI_PERDIR, OnUpdateBool, coverager_autostart, zend_xcache_globals, xcache_globals)
	PHP_INI_ENTRY1     ("xcache.coveragedump_directory",  "", PHP_INI_SYSTEM, xcache_OnUpdateDummy, NULL)
PHP_INI_END()
/* }}} */
static PHP_MINFO_FUNCTION(xcache_coverager) /* {{{ */
{
	char *covdumpdir;

	php_info_print_table_start();
	php_info_print_table_row(2, "XCache Coverager Module", "enabled");
	if (cfg_get_string("xcache.coveragedump_directory", &covdumpdir) != SUCCESS || !covdumpdir[0]) {
		covdumpdir = NULL;
	}
	php_info_print_table_row(2, "Coverage Started", XG(coverager_started) && covdumpdir ? "On" : "Off");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */
static PHP_MINIT_FUNCTION(xcache_coverager) /* {{{ */
{
	REGISTER_INI_ENTRIES();

	if (cfg_get_string("xcache.coveragedump_directory", &xc_coveragedump_dir) == SUCCESS && xc_coveragedump_dir) {
		size_t len;
		xc_coveragedump_dir = pestrdup(xc_coveragedump_dir, 1);
		len = strlen(xc_coveragedump_dir);
		if (len) {
			if (xc_coveragedump_dir[len - 1] == '/') {
				xc_coveragedump_dir[len - 1] = '\0';
			}
		}
		if (!strlen(xc_coveragedump_dir)) {
			pefree(xc_coveragedump_dir, 1);
			xc_coveragedump_dir = NULL;
		}
	}

	return xcache_zend_extension_add(&xc_coverager_zend_extension_entry, 0);
}
/* }}} */
static PHP_MSHUTDOWN_FUNCTION(xcache_coverager) /* {{{ */
{
	if (old_compile_file && zend_compile_file == xc_compile_file_for_coverage) {
		zend_compile_file = old_compile_file;
		old_compile_file = NULL;
	}
	if (xc_coveragedump_dir) {
		pefree(xc_coveragedump_dir, 1);
		xc_coveragedump_dir = NULL;
	}
	UNREGISTER_INI_ENTRIES();
	return xcache_zend_extension_remove(&xc_coverager_zend_extension_entry);
}
/* }}} */

static zend_module_entry xcache_coverager_module_entry = { /* {{{ */
	STANDARD_MODULE_HEADER,
	XCACHE_NAME " Coverager",
	xcache_coverager_functions,
	PHP_MINIT(xcache_coverager),
	PHP_MSHUTDOWN(xcache_coverager),
	PHP_RINIT(xcache_coverager),
	PHP_RSHUTDOWN(xcache_coverager),
	PHP_MINFO(xcache_coverager),
	XCACHE_VERSION,
#ifdef PHP_GINIT
	NO_MODULE_GLOBALS,
#endif
#ifdef ZEND_ENGINE_2
	NULL,
#else
	NULL,
	NULL,
#endif
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */
int xc_coverager_startup_module() /* {{{ */
{
	return zend_startup_module(&xcache_coverager_module_entry);
}
/* }}} */
