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
#include "coverage.h"
#include "utils.h"
typedef HashTable *coverage_t;
#define PCOV_HEADER_MAGIC 0x564f4350

char *xc_coveragedump_dir = NULL;
static zend_compile_file_t *origin_compile_file;

#undef DEBUG
/* dumper */
static void xc_destroy_coverage(void *pDest) /* {{{ */
{
	coverage_t cov = *(coverage_t*) pDest;
#ifdef DEBUG
	fprintf(stderr, "destroy %p\n", cov);
#endif
	zend_hash_destroy(cov);
	efree(cov);
}
/* }}} */
void xcache_mkdirs_ex(char *root, int rootlen, char *path, int pathlen TSRMLS_DC) /* {{{ */
{
	char *fullpath;
	struct stat st;

#ifdef DEBUG
	fprintf(stderr, "mkdirs %s %d %s %d\n", root, rootlen, path, pathlen);
#endif
	fullpath = do_alloca(rootlen + pathlen + 1);
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
#ifdef DEBUG
		fprintf(stderr, "mkdir %s\n", fullpath);
#endif
		php_stream_mkdir(fullpath, 0700, REPORT_ERRORS, NULL);
	}
	free_alloca(fullpath);
}
/* }}} */
static void xc_coverage_save_cov(char *srcfile, char *outfilename, coverage_t cov TSRMLS_DC) /* {{{ */
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
#ifdef DEBUG
		fprintf(stderr, "new file\n");
#endif
	}
	else if (outstat.st_size) {
		len = outstat.st_size;
		contents = emalloc(len);
		if (read(fd, (void *) contents, len) != len) {
			goto bailout;
		}
#ifdef DEBUG
		fprintf(stderr, "oldsize %d\n", (int) len);
#endif
		do {
			p = (long *) contents;
			len -= sizeof(long);
			if (len < 0) {
				break;
			}
			if (*p++ != PCOV_HEADER_MAGIC) {
#ifdef DEBUG
				fprintf(stderr, "wrong magic in file %s\n", outfilename);
#endif
				break;
			}

			p += 2; /* skip covliens */
			len -= sizeof(long) * 2;
			if (len < 0) {
				break;
			}

			for (; len >= sizeof(long) * 2; len -= sizeof(long) * 2, p += 2) {
				if (zend_hash_index_find(cov, p[0], (void**)&phits) == SUCCESS) {
					if (p[1] <= 0) {
						/* already marked */
						continue;
					}
					if (*phits > 0) {
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
		if (*phits <= 0) {
			*p++ = 0;
		}
		else {
			*p++ = *phits;
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
void xc_coverage_request_init(TSRMLS_D) /* {{{ */
{
	if (XG(coveragedumper)) {
		XG(coverages) = emalloc(sizeof(HashTable));
		zend_hash_init(XG(coverages), 0, NULL, xc_destroy_coverage, 0);
	}
}
/* }}} */
void xc_coverage_request_shutdown(TSRMLS_D) /* {{{ */
{
	coverage_t *pcov;
	zstr s;
	char *outfilename;
	int dumpdir_len, outfilelen, size, alloc_len = 0;

	if (!XG(coverages)) {
		return;
	}
	if (XG(coveragedumper)) {	
		dumpdir_len = strlen(xc_coveragedump_dir);
		alloc_len = dumpdir_len + 1 + 128;
		outfilename = emalloc(alloc_len);
		strcpy(outfilename, xc_coveragedump_dir);

		zend_hash_internal_pointer_reset(XG(coverages));
		while (zend_hash_get_current_data(XG(coverages), (void **) &pcov) == SUCCESS) {
			zend_hash_get_current_key_ex(XG(coverages), &s, &size, NULL, 0, NULL);
			outfilelen = dumpdir_len + size + 5;
			if (alloc_len < outfilelen) {
				alloc_len = outfilelen + 128;
				outfilename = erealloc(outfilename, alloc_len);
			}
			strcpy(outfilename + dumpdir_len, ZSTR_S(s));
			strcpy(outfilename + dumpdir_len + size - 1, ".pcov");

#ifdef DEBUG
			fprintf(stderr, "outfilename %s\n", outfilename);
#endif
			xc_coverage_save_cov(ZSTR_S(s), outfilename, *pcov TSRMLS_CC);
			zend_hash_move_forward(XG(coverages));
		}
	}

	zend_hash_destroy(XG(coverages));
	efree(XG(coverages));
	XG(coverages) = NULL;
}
/* }}} */

/* helper func to store hits into coverages */
static coverage_t xc_coverage_get(char *filename TSRMLS_DC) /* {{{ */
{
	int len = strlen(filename) + 1;
	coverage_t cov, *pcov;

	if (zend_hash_find(XG(coverages), filename, len, (void **) &pcov) == SUCCESS) {
#ifdef DEBUG
		fprintf(stderr, "got coverage %s %p\n", filename, *pcov);
#endif
		return *pcov;
	}
	else {
		cov = emalloc(sizeof(HashTable));
		zend_hash_init(cov, 0, NULL, NULL, 0);
		zend_hash_add(XG(coverages), filename, len, (void **) &cov, sizeof(cov), NULL);
#ifdef DEBUG
		fprintf(stderr, "new coverage %s %p\n", filename, cov);
#endif
		return cov;
	}
}
/* }}} */
static void xc_coverage_add_hits(HashTable *cov, long line, long hits TSRMLS_DC) /* {{{ */
{
	long *poldhits;

	if (line == 0) {
		return;
	}
	if (zend_hash_index_find(cov, line, (void**)&poldhits) == SUCCESS) {
		if (hits == -1) {
			/* already marked */
			return;
		}
		if (*poldhits != -1) {
			hits += *poldhits;
		}
	}
	zend_hash_index_update(cov, line, &hits, sizeof(hits), NULL);
}
/* }}} */

static int xc_coverage_get_op_array_size_no_tail(zend_op_array *op_array) /* {{{ */
{
	zend_uint size;

	size = op_array->size;
#ifdef ZEND_ENGINE_2
	if (op_array->opcodes[size - 1].opcode == ZEND_HANDLE_EXCEPTION) {
		size --;
#endif
		if (op_array->opcodes[size - 1].opcode == ZEND_RETURN) {
			size --;
			/* it's not real php statement */
			if (op_array->opcodes[size - 1].opcode == ZEND_EXT_STMT) {
				size --;
			}
		}   
#ifdef ZEND_ENGINE_2
	}
#endif
	return size;
}
/* }}} */

/* prefill */
static int xc_coverage_init_op_array(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	zend_uint size;
	coverage_t cov;
	zend_uint i;

	if (op_array->type != ZEND_USER_FUNCTION) {
		return 0;
	}

	size = xc_coverage_get_op_array_size_no_tail(op_array);
	cov = xc_coverage_get(op_array->filename TSRMLS_CC);
	for (i = 0; i < size; i ++) {
		switch (op_array->opcodes[i].opcode) {
			case ZEND_EXT_STMT:
#if 0
			case ZEND_EXT_FCALL_BEGIN:
			case ZEND_EXT_FCALL_END:
#endif
				xc_coverage_add_hits(cov, op_array->opcodes[i].lineno, -1 TSRMLS_CC);
				break;
		}
	}
	return 0;
}
/* }}} */
static void xc_coverage_init_compile_result(zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	xc_compile_result_t cr;

	xc_compile_result_init_cur(&cr, op_array TSRMLS_CC);
	xc_apply_op_array(&cr, (apply_func_t) xc_coverage_init_op_array TSRMLS_CC);
	xc_compile_result_free(&cr);
}
/* }}} */
static zend_op_array *xc_compile_file_for_coverage(zend_file_handle *h, int type TSRMLS_DC) /* {{{ */
{
	zend_op_array *op_array;

	op_array = origin_compile_file(h, type TSRMLS_CC);
	if (XG(coveragedumper) && XG(coverages)) {
		xc_coverage_init_compile_result(op_array TSRMLS_CC);
	}
	return op_array;
}
/* }}} */

/* hits */
void xc_coverage_handle_ext_stmt(zend_op_array *op_array, zend_uchar op) /* {{{ */
{
	TSRMLS_FETCH();

	if (XG(coveragedumper) && XG(coverages)) {
		int size = xc_coverage_get_op_array_size_no_tail(op_array);
		int oplineno = (*EG(opline_ptr)) - op_array->opcodes;
		if (oplineno < size) {
			xc_coverage_add_hits(xc_coverage_get(op_array->filename TSRMLS_CC), (*EG(opline_ptr))->lineno, 1 TSRMLS_CC);
		}
	}
}
/* }}} */

/* init/destroy */
int xc_coverage_init(int module_number TSRMLS_DC) /* {{{ */
{
	if (xc_coveragedump_dir) {
		int len = strlen(xc_coveragedump_dir);
		if (len) {
			if (xc_coveragedump_dir[len - 1] == '/') {
				xc_coveragedump_dir[len - 1] = '\0';
			}
		}
	}
	if (xc_coveragedump_dir && xc_coveragedump_dir[0]) {
		origin_compile_file = zend_compile_file;
		zend_compile_file = xc_compile_file_for_coverage;
		CG(extended_info) = 1;
	}
	return SUCCESS;
}
/* }}} */
void xc_coverage_destroy() /* {{{ */
{
	if (origin_compile_file == xc_compile_file_for_coverage) {
		zend_compile_file = origin_compile_file;
	}
	if (xc_coveragedump_dir) {
		pefree(xc_coveragedump_dir, 1);
		xc_coveragedump_dir = NULL;
	}
}
/* }}} */

/* user api */
PHP_FUNCTION(xcache_coverage_decode) /* {{{ */
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
#ifdef DEBUG
		fprintf(stderr, "wrong magic in xcache_coverage_decode");
#endif
		return;
	}

	for (; len >= sizeof(long) * 2; len -= sizeof(long) * 2, p += 2) {
		add_index_long(return_value, p[0], p[1]);
	}
}
/* }}} */

