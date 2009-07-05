#include <stdio.h>
#include "xcache.h"
#include "ext/standard/flock_compat.h"
#ifdef HAVE_SYS_FILE_H
#	include <sys/file.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "stack.h"
#include "xcache_globals.h"
#include "coverager.h"
#include "utils.h"
typedef HashTable *coverager_t;
#define PCOV_HEADER_MAGIC 0x564f4350

static char *xc_coveragedump_dir = NULL;
static zend_compile_file_t *old_compile_file = NULL;

#if 0
#define DEBUG
#endif

/* dumper */
static void xc_destroy_coverage(void *pDest) /* {{{ */
{
	coverager_t cov = *(coverager_t*) pDest;
	TRACE("destroy %p", cov);
	zend_hash_destroy(cov);
	efree(cov);
}
/* }}} */
void xcache_mkdirs_ex(char *root, int rootlen, char *path, int pathlen TSRMLS_DC) /* {{{ */
{
	char *fullpath;
	struct stat st;
	ALLOCA_FLAG(use_heap)

	TRACE("mkdirs %s %d %s %d", root, rootlen, path, pathlen);
	fullpath = my_do_alloca(rootlen + pathlen + 1, use_heap);
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
	my_free_alloca(fullpath, use_heap);
}
/* }}} */
static void xc_coverager_save_cov(char *srcfile, char *outfilename, coverager_t cov TSRMLS_DC) /* {{{ */
{
	long *buf = NULL, *p;
	long covlines, *phits;
	int fd = -1;
	int size;
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

			for (; len >= sizeof(long) * 2; len -= sizeof(long) * 2, p += 2) {
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

	ftruncate(fd, 0);
	lseek(fd, 0, SEEK_SET);
	write(fd, (char *) buf, size);

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

static void xc_coverager_enable(TSRMLS_D) /* {{{ */
{
	XG(coverage_enabled) = 1;
}
/* }}} */
static void xc_coverager_disable(TSRMLS_D) /* {{{ */
{
	XG(coverage_enabled) = 0;
}
/* }}} */

void xc_coverager_request_init(TSRMLS_D) /* {{{ */
{
	if (XG(coverager)) {
		xc_coverager_enable(TSRMLS_C);
		CG(extended_info) = 1;
	}
	else {
		XG(coverage_enabled) = 0;
	}
}
/* }}} */
static void xc_coverager_autodump(TSRMLS_D) /* {{{ */
{
	coverager_t *pcov;
	zstr s;
	char *outfilename;
	int dumpdir_len, outfilelen, alloc_len = 0;
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
			add_assoc_zval_ex(return_value, ZSTR_S(filename), strlen(ZSTR_S(filename)) + 1, lines);

			zend_hash_move_forward_ex(XG(coverages), &pos);
		}
	}
	else {
		RETVAL_NULL();
	}
}
/* }}} */
void xc_coverager_request_shutdown(TSRMLS_D) /* {{{ */
{
	if (XG(coverager)) {
		xc_coverager_autodump(TSRMLS_C);
		xc_coverager_cleanup(TSRMLS_C);
	}
}
/* }}} */

/* helper func to store hits into coverages */
static coverager_t xc_coverager_get(char *filename TSRMLS_DC) /* {{{ */
{
	int len = strlen(filename) + 1;
	coverager_t cov, *pcov;

	if (zend_hash_find(XG(coverages), filename, len, (void **) &pcov) == SUCCESS) {
		TRACE("got coverage %s %p", filename, *pcov);
		return *pcov;
	}
	else {
		cov = emalloc(sizeof(HashTable));
		zend_hash_init(cov, 0, NULL, NULL, 0);
		zend_hash_add(XG(coverages), filename, len, (void **) &cov, sizeof(cov), NULL);
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
	zend_uint size;

	size = op_array->size;
	do {
next_op:
		if (size == 0) {
			break;
		}
		switch (op_array->opcodes[size - 1].opcode) {
#ifdef ZEND_HANDLE_EXCEPTION
			case ZEND_HANDLE_EXCEPTION:
#endif
			case ZEND_RETURN:
			case ZEND_EXT_STMT:
				size --;
				goto next_op;
		}
	} while (0);
	return size;
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
void xc_coverager_handle_ext_stmt(zend_op_array *op_array, zend_uchar op) /* {{{ */
{
	TSRMLS_FETCH();

	if (XG(coverages) && XG(coverage_enabled)) {
		int size = xc_coverager_get_op_array_size_no_tail(op_array);
		int oplineno = (*EG(opline_ptr)) - op_array->opcodes;
		if (oplineno < size) {
			xc_coverager_add_hits(xc_coverager_get(op_array->filename TSRMLS_CC), (*EG(opline_ptr))->lineno, 1 TSRMLS_CC);
		}
	}
}
/* }}} */

/* init/destroy */
int xc_coverager_init(int module_number TSRMLS_DC) /* {{{ */
{
	old_compile_file = zend_compile_file;
	zend_compile_file = xc_compile_file_for_coverage;

	if (cfg_get_string("xcache.coveragedump_directory", &xc_coveragedump_dir) == SUCCESS && xc_coveragedump_dir) {
		int len = strlen(xc_coveragedump_dir);
		if (len) {
			if (xc_coveragedump_dir[len - 1] == '/') {
				xc_coveragedump_dir[len - 1] = '\0';
			}
		}
		if (!strlen(xc_coveragedump_dir)) {
			xc_coveragedump_dir = NULL;
		}
	}

	return SUCCESS;
}
/* }}} */
void xc_coverager_destroy() /* {{{ */
{
	if (old_compile_file == xc_compile_file_for_coverage) {
		zend_compile_file = old_compile_file;
	}
	if (xc_coveragedump_dir) {
		xc_coveragedump_dir = NULL;
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
	len -= sizeof(long);
	if (len < 0) {
		return;
	}
	if (*p++ != PCOV_HEADER_MAGIC) {
		TRACE("%s", "wrong magic in xcache_coverager_decode");
		return;
	}

	for (; len >= sizeof(long) * 2; len -= sizeof(long) * 2, p += 2) {
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
		xc_coverager_enable(TSRMLS_C);
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

	xc_coverager_disable(TSRMLS_C);
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
