
#undef DEBUG

/* {{{ macros */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <signal.h>

#include "php.h"
#include "ext/standard/info.h"
#include "ext/standard/md5.h"
#include "ext/standard/php_math.h"
#include "zend_extensions.h"
#include "SAPI.h"

#include "xcache.h"
#include "optimizer.h"
#include "coverager.h"
#include "disassembler.h"
#include "align.h"
#include "stack.h"
#include "xcache_globals.h"
#include "processor.h"
#include "utils.h"
#include "const_string.h"
#include "opcode_spec.h"

#ifdef DEBUG
#	undef NDEBUG
#	undef inline
#	define inline
#else
#	ifndef NDEBUG
#		define NDEBUG
#	endif
#endif
#include <assert.h>

#define VAR_ENTRY_EXPIRED(pentry) ((pentry)->ttl && XG(request_time) > pentry->ctime + (pentry)->ttl)
#define CHECK(x, e) do { if ((x) == NULL) { zend_error(E_ERROR, "XCache: " e); goto err; } } while (0)
#define LOCK(x) xc_lock(x->lck)
#define UNLOCK(x) xc_unlock(x->lck)

#define ENTER_LOCK_EX(x) \
	xc_lock(x->lck); \
	zend_try { \
		do
#define LEAVE_LOCK_EX(x) \
		while (0); \
	} zend_catch { \
		catched = 1; \
	} zend_end_try(); \
	xc_unlock(x->lck)

#define ENTER_LOCK(x) do { \
	int catched = 0; \
	ENTER_LOCK_EX(x)
#define LEAVE_LOCK(x) \
	LEAVE_LOCK_EX(x); \
	if (catched) { \
		zend_bailout(); \
	} \
} while(0)

/* }}} */

/* {{{ globals */
static char *xc_mmap_path = NULL;
static char *xc_coredump_dir = NULL;

static xc_hash_t xc_php_hcache = {0};
static xc_hash_t xc_php_hentry = {0};
static xc_hash_t xc_var_hcache = {0};
static xc_hash_t xc_var_hentry = {0};

static zend_ulong xc_php_ttl    = 0;
static zend_ulong xc_var_maxttl = 0;

enum { xc_deletes_gc_interval = 120 };
static zend_ulong xc_php_gc_interval = 0;
static zend_ulong xc_var_gc_interval = 0;

/* total size */
static zend_ulong xc_php_size  = 0;
static zend_ulong xc_var_size  = 0;

static xc_cache_t **xc_php_caches = NULL;
static xc_cache_t **xc_var_caches = NULL;

static zend_bool xc_initized = 0;
static zend_compile_file_t *origin_compile_file;

static zend_bool xc_test = 0;
static zend_bool xc_readonly_protection = 0;

static zend_bool xc_module_gotup = 0;
static zend_bool xc_zend_extension_gotup = 0;
#if !COMPILE_DL_XCACHE
#	define zend_extension_entry xcache_zend_extension_entry
#endif
ZEND_DLEXPORT zend_extension zend_extension_entry;
ZEND_DECLARE_MODULE_GLOBALS(xcache);
/* }}} */

/* any function in *_dmz is only safe be called within locked(single thread) area */

static inline int xc_entry_equal_dmz(xc_entry_t *a, xc_entry_t *b) /* {{{ */
{
	/* this function isn't required but can be in dmz */

	if (a->type != b->type) {
		return 0;
	}
	switch (a->type) {
		case XC_TYPE_PHP:
#ifdef HAVE_INODE
			do {
				xc_entry_data_php_t *ap = a->data.php;
				xc_entry_data_php_t *bp = b->data.php;
				return ap->inode == bp->inode
					&& ap->device == bp->device;
			} while(0);
#endif
			/* fall */

		case XC_TYPE_VAR:
			do {
#ifdef IS_UNICODE
				if (a->name_type == IS_UNICODE) {
					if (a->name.ustr.len != b->name.ustr.len) {
						return 0;
					}
					return memcmp(a->name.ustr.val, b->name.ustr.val, (a->name.ustr.len + 1) * sizeof(UChar)) == 0;
				}
				else {
					return memcmp(a->name.str.val, b->name.str.val, a->name.str.len + 1) == 0;
				}
#else
				return memcmp(a->name.str.val, b->name.str.val, a->name.str.len + 1) == 0;
#endif

			} while(0);
		default:
			assert(0);
	}
	return 0;
}
/* }}} */
static void xc_entry_free_dmz(volatile xc_entry_t *xce) /* {{{ */
{
	xc_mem_free(xce->cache->mem, (xc_entry_t *)xce);
}
/* }}} */
static void xc_entry_add_dmz(xc_entry_t *xce) /* {{{ */
{
	xc_entry_t **head = &(xce->cache->entries[xce->hvalue]);
	xce->next = *head;
	*head = xce;
	xce->cache->entries_count ++;
}
/* }}} */
static xc_entry_t *xc_entry_store_dmz(xc_entry_t *xce TSRMLS_DC) /* {{{ */
{
	xc_entry_t *stored_xce;

	xce->hits  = 0;
	xce->ctime = XG(request_time);
	xce->atime = XG(request_time);
	stored_xce = xc_processor_store_xc_entry_t(xce TSRMLS_CC);
	if (stored_xce) {
		xc_entry_add_dmz(stored_xce);
		return stored_xce;
	}
	else {
		xce->cache->ooms ++;
		return NULL;
	}
}
/* }}} */
static void xc_entry_remove_real_dmz(xc_entry_t *xce, xc_entry_t **pp TSRMLS_DC) /* {{{ */
{
	*pp = xce->next;
	xce->cache->entries_count --;
	if (xce->refcount == 0) {
		xc_entry_free_dmz(xce);
	}
	else {
		xce->next = xce->cache->deletes;
		xce->cache->deletes = xce;
		xce->dtime = XG(request_time);
		xce->cache->deletes_count ++;
	}
	return;
}
/* }}} */
static void xc_entry_remove_dmz(xc_entry_t *xce TSRMLS_DC) /* {{{ */
{
	xc_entry_t **pp = &(xce->cache->entries[xce->hvalue]);
	xc_entry_t *p;
	for (p = *pp; p; pp = &(p->next), p = p->next) {
		if (xc_entry_equal_dmz(xce, p)) {
			xc_entry_remove_real_dmz(xce, pp TSRMLS_CC);
			return;
		}
	}
	assert(0);
}
/* }}} */
static xc_entry_t *xc_entry_find_dmz(xc_entry_t *xce TSRMLS_DC) /* {{{ */
{
	xc_entry_t *p;
	for (p = xce->cache->entries[xce->hvalue]; p; p = p->next) {
		if (xc_entry_equal_dmz(xce, p)) {
			if (p->type == XC_TYPE_VAR || /* PHP */ p->data.php->mtime == xce->data.php->mtime) {
				p->hits ++;
				p->atime = XG(request_time);
				return p;
			}
			else {
				xc_entry_remove_dmz(p TSRMLS_CC);
				return NULL;
			}
		}
	}
	return NULL;
}
/* }}} */
static void xc_entry_hold_php_dmz(xc_entry_t *xce TSRMLS_DC) /* {{{ */
{
	xce->refcount ++;
	xc_stack_push(&XG(php_holds)[xce->cache->cacheid], (void *)xce);
}
/* }}} */
#if 0
static void xc_entry_hold_var_dmz(xc_entry_t *xce TSRMLS_DC) /* {{{ */
{
	xce->refcount ++;
	xc_stack_push(&XG(var_holds)[xce->cache->cacheid], (void *)xce);
}
/* }}} */
#endif

/* helper function that loop through each entry */
#define XC_ENTRY_APPLY_FUNC(name) int name(xc_entry_t *entry TSRMLS_DC)
typedef XC_ENTRY_APPLY_FUNC((*cache_apply_dmz_func_t));
static void xc_entry_apply_dmz(xc_cache_t *cache, cache_apply_dmz_func_t apply_func TSRMLS_DC) /* {{{ */
{
	xc_entry_t *p, **pp;
	int i, c;

	for (i = 0, c = cache->hentry->size; i < c; i ++) {
		pp = &(cache->entries[i]);
		for (p = *pp; p; p = p->next) {
			if (apply_func(p TSRMLS_CC)) {
				xc_entry_remove_real_dmz(p, pp TSRMLS_CC);
			}
			else {
				pp = &(p->next);
			}
		}
	}
}
/* }}} */

#define XC_CACHE_APPLY_FUNC(name) void name(xc_cache_t *cache TSRMLS_DC)
/* call graph:
 * xc_gc_expires_php -> xc_gc_expires_one -> xc_entry_apply_dmz -> xc_gc_expires_php_entry_dmz
 * xc_gc_expires_var -> xc_gc_expires_one -> xc_entry_apply_dmz -> xc_gc_expires_var_entry_dmz
 */
static XC_ENTRY_APPLY_FUNC(xc_gc_expires_php_entry_dmz) /* {{{ */
{
#ifdef DEBUG
	fprintf(stderr, "ttl %d, %d %d\n", XG(request_time), entry->atime, xc_php_ttl);
#endif
	if (XG(request_time) > entry->atime + xc_php_ttl) {
		return 1;
	}
	return 0;
}
/* }}} */
static XC_ENTRY_APPLY_FUNC(xc_gc_expires_var_entry_dmz) /* {{{ */
{
	if (VAR_ENTRY_EXPIRED(entry)) {
		return 1;
	}
	return 0;
}
/* }}} */
static void xc_gc_expires_one(xc_cache_t *cache, zend_ulong gc_interval, cache_apply_dmz_func_t apply_func TSRMLS_DC) /* {{{ */
{
#ifdef DEBUG
	fprintf(stderr, "interval %d, %d %d\n", XG(request_time), cache->last_gc_expires, gc_interval);
#endif
	if (XG(request_time) - cache->last_gc_expires >= gc_interval) {
		ENTER_LOCK(cache) {
			if (XG(request_time) - cache->last_gc_expires >= gc_interval) {
				cache->last_gc_expires = XG(request_time);
				xc_entry_apply_dmz(cache, apply_func TSRMLS_CC);
			}
		} LEAVE_LOCK(cache);
	}
}
/* }}} */
static void xc_gc_expires_php(TSRMLS_D) /* {{{ */
{
	int i, c;

	if (!xc_php_ttl || !xc_php_gc_interval) {
		return;
	}

	for (i = 0, c = xc_php_hcache.size; i < c; i ++) {
		xc_gc_expires_one(xc_php_caches[i], xc_php_gc_interval, xc_gc_expires_php_entry_dmz TSRMLS_CC);
	}
}
/* }}} */
static void xc_gc_expires_var(TSRMLS_D) /* {{{ */
{
	int i, c;

	if (!xc_var_gc_interval) {
		return;
	}

	for (i = 0, c = xc_var_hcache.size; i < c; i ++) {
		xc_gc_expires_one(xc_var_caches[i], xc_var_gc_interval, xc_gc_expires_var_entry_dmz TSRMLS_CC);
	}
}
/* }}} */

static XC_CACHE_APPLY_FUNC(xc_gc_delete_dmz) /* {{{ */
{
	xc_entry_t *p, **pp;

	pp = &cache->deletes;
	for (p = *pp; p; p = p->next) {
		if (XG(request_time) - p->dtime > 3600) {
			p->refcount = 0;
			/* issue warning here */
		}
		if (p->refcount == 0) {
			*pp = p->next;
			cache->deletes_count --;
			xc_entry_free_dmz(p);
		}
		else {
			pp = &(p->next);
		}
	}
}
/* }}} */
static XC_CACHE_APPLY_FUNC(xc_gc_deletes_one) /* {{{ */
{
	if (cache->deletes && XG(request_time) - cache->last_gc_deletes > xc_deletes_gc_interval) {
		ENTER_LOCK(cache) {
			if (cache->deletes && XG(request_time) - cache->last_gc_deletes > xc_deletes_gc_interval) {
				xc_gc_delete_dmz(cache TSRMLS_CC);
			}
		} LEAVE_LOCK(cache);
	}
}
/* }}} */
static void xc_gc_deletes(TSRMLS_D) /* {{{ */
{
	int i, c;

	for (i = 0, c = xc_php_hcache.size; i < c; i ++) {
		xc_gc_deletes_one(xc_php_caches[i] TSRMLS_CC);
	}

	for (i = 0, c = xc_var_hcache.size; i < c; i ++) {
		xc_gc_deletes_one(xc_var_caches[i] TSRMLS_CC);
	}
}
/* }}} */

/* helper functions for user functions */
static void xc_fillinfo_dmz(int cachetype, xc_cache_t *cache, zval *return_value TSRMLS_DC) /* {{{ */
{
	zval *blocks;
	const xc_block_t *b;
#ifndef NDEBUG
	xc_memsize_t avail = 0;
#endif
	xc_mem_t *mem = cache->mem;
	zend_ulong interval = (cachetype == XC_TYPE_PHP) ? xc_php_gc_interval : xc_var_gc_interval;

	add_assoc_long_ex(return_value, ZEND_STRS("slots"),     cache->hentry->size);
	add_assoc_long_ex(return_value, ZEND_STRS("compiling"), cache->compiling);
	add_assoc_long_ex(return_value, ZEND_STRS("misses"),    cache->misses);
	add_assoc_long_ex(return_value, ZEND_STRS("hits"),      cache->hits);
	add_assoc_long_ex(return_value, ZEND_STRS("clogs"),     cache->clogs);
	add_assoc_long_ex(return_value, ZEND_STRS("ooms"),      cache->ooms);

	add_assoc_long_ex(return_value, ZEND_STRS("cached"),    cache->entries_count);
	add_assoc_long_ex(return_value, ZEND_STRS("deleted"),   cache->deletes_count);
	if (interval) {
		add_assoc_long_ex(return_value, ZEND_STRS("gc"),    (cache->last_gc_expires + interval) - XG(request_time));
	}
	else {
		add_assoc_null_ex(return_value, ZEND_STRS("gc"));
	}

	MAKE_STD_ZVAL(blocks);
	array_init(blocks);

	add_assoc_long_ex(return_value, ZEND_STRS("size"),  xc_mem_size(mem));
	add_assoc_long_ex(return_value, ZEND_STRS("avail"), xc_mem_avail(mem));
	add_assoc_bool_ex(return_value, ZEND_STRS("can_readonly"), xc_readonly_protection);

	for (b = xc_mem_freeblock_first(mem); b; b = xc_mem_freeblock_next(b)) {
		zval *bi;

		MAKE_STD_ZVAL(bi);
		array_init(bi);

		add_assoc_long_ex(bi, ZEND_STRS("size"),   xc_mem_block_size(b));
		add_assoc_long_ex(bi, ZEND_STRS("offset"), xc_mem_block_offset(mem, b));
		add_next_index_zval(blocks, bi);
#ifndef NDEBUG
		avail += b->size;
#endif
	}
	add_assoc_zval_ex(return_value, ZEND_STRS("free_blocks"), blocks);
	assert(avail == xc_mem_avail(mem));
}
/* }}} */
static void xc_fillentry_dmz(xc_entry_t *entry, int del, zval *list TSRMLS_DC) /* {{{ */
{
	zval* ei;
	xc_entry_data_php_t *php;
	xc_entry_data_var_t *var;

	ALLOC_INIT_ZVAL(ei);
	array_init(ei);

	add_assoc_long_ex(ei, ZEND_STRS("size"),     entry->size);
	add_assoc_long_ex(ei, ZEND_STRS("refcount"), entry->refcount);
	add_assoc_long_ex(ei, ZEND_STRS("hits"),     entry->hits);
	add_assoc_long_ex(ei, ZEND_STRS("ctime"),    entry->ctime);
	add_assoc_long_ex(ei, ZEND_STRS("atime"),    entry->atime);
	add_assoc_long_ex(ei, ZEND_STRS("dtime"),    entry->dtime);
#ifdef IS_UNICODE
	do {
		zval *zv;
		ALLOC_INIT_ZVAL(zv);
		switch (entry->name_type) {
			case IS_UNICODE:
					ZVAL_UNICODEL(zv, entry->name.ustr.val, entry->name.ustr.len, 1);
				break;
			case IS_STRING:
				ZVAL_STRINGL(zv, entry->name.str.val, entry->name.str.len, 1);
				break;
			default:
				assert(0);
		}
		zv->type = entry->name_type;
		add_assoc_zval_ex(ei, ZEND_STRS("name"), zv);
	} while (0);
#else
	add_assoc_stringl_ex(ei, ZEND_STRS("name"), entry->name.str.val, entry->name.str.len, 1);
#endif
	switch (entry->type) {
		case XC_TYPE_PHP:
			php = entry->data.php;
			add_assoc_long_ex(ei, ZEND_STRS("sourcesize"),    php->sourcesize);
#ifdef HAVE_INODE
			add_assoc_long_ex(ei, ZEND_STRS("device"),        php->device);
			add_assoc_long_ex(ei, ZEND_STRS("inode"),         php->inode);
#endif
			add_assoc_long_ex(ei, ZEND_STRS("mtime"),         php->mtime);

#ifdef HAVE_XCACHE_CONSTANT
			add_assoc_long_ex(ei, ZEND_STRS("constinfo_cnt"), php->constinfo_cnt);
#endif
			add_assoc_long_ex(ei, ZEND_STRS("function_cnt"),  php->funcinfo_cnt);
			add_assoc_long_ex(ei, ZEND_STRS("class_cnt"),     php->classinfo_cnt);
			break;
		case XC_TYPE_VAR:
			var = entry->data.var;
			break;

		default:
			assert(0);
	}

	add_next_index_zval(list, ei);
}
/* }}} */
static void xc_filllist_dmz(xc_cache_t *cache, zval *return_value TSRMLS_DC) /* {{{ */
{
	zval* list;
	int i, c;
	xc_entry_t *e;

	ALLOC_INIT_ZVAL(list);
	array_init(list);

	for (i = 0, c = cache->hentry->size; i < c; i ++) {
		for (e = cache->entries[i]; e; e = e->next) {
			xc_fillentry_dmz(e, 0, list TSRMLS_CC);
		}
	}
	add_assoc_zval(return_value, "cache_list", list);

	ALLOC_INIT_ZVAL(list);
	array_init(list);
	for (e = cache->deletes; e; e = e->next) {
		xc_fillentry_dmz(e, 1, list TSRMLS_CC);
	}
	add_assoc_zval(return_value, "deleted_list", list);
}
/* }}} */

static zend_op_array *xc_entry_install(xc_entry_t *xce, zend_file_handle *h TSRMLS_DC) /* {{{ */
{
	zend_uint i;
	xc_entry_data_php_t *p = xce->data.php;
#ifndef ZEND_ENGINE_2
	/* new ptr which is stored inside CG(class_table) */
	xc_cest_t **new_cest_ptrs = (xc_cest_t **)do_alloca(sizeof(xc_cest_t*) * p->classinfo_cnt);
#endif

#ifdef HAVE_XCACHE_CONSTANT
	/* install constant */
	for (i = 0; i < p->constinfo_cnt; i ++) {
		xc_constinfo_t *ci = &p->constinfos[i];
		xc_install_constant(xce->name.str.val, &ci->constant,
				UNISW(0, ci->type), ci->key, ci->key_size TSRMLS_CC);
	}
#endif

	/* install function */
	for (i = 0; i < p->funcinfo_cnt; i ++) {
		xc_funcinfo_t  *fi = &p->funcinfos[i];
		xc_install_function(xce->name.str.val, &fi->func,
				UNISW(0, fi->type), fi->key, fi->key_size TSRMLS_CC);
	}

	/* install class */
	for (i = 0; i < p->classinfo_cnt; i ++) {
		xc_classinfo_t *ci = &p->classinfos[i];
#ifndef ZEND_ENGINE_2
		zend_class_entry *ce = CestToCePtr(ci->cest);
		/* fix pointer to the be which inside class_table */
		if (ce->parent) {
			zend_uint class_idx = (/* class_num */ (int) ce->parent) - 1;
			assert(class_idx < i);
			ci->cest.parent = new_cest_ptrs[class_idx];
		}
		new_cest_ptrs[i] =
#endif
		xc_install_class(xce->name.str.val, &ci->cest,
				UNISW(0, ci->type), ci->key, ci->key_size TSRMLS_CC);
	}

	i = 1;
	zend_hash_add(&EG(included_files), xce->name.str.val, xce->name.str.len+1, (void *)&i, sizeof(int), NULL);
	if (h) {
		zend_llist_add_element(&CG(open_files), h);
	}

#ifndef ZEND_ENGINE_2
	free_alloca(new_cest_ptrs);
#endif
	return p->op_array;
}
/* }}} */

static inline void xc_entry_unholds_real(xc_stack_t *holds, xc_cache_t **caches, int cachecount TSRMLS_DC) /* {{{ */
{
	int i;
	xc_stack_t *s;
	xc_cache_t *cache;
	xc_entry_t *xce;

	for (i = 0; i < cachecount; i ++) {
		s = &holds[i];
		if (xc_stack_size(s)) {
			cache = ((xc_entry_t *)xc_stack_top(s))->cache;
			ENTER_LOCK(cache) {
				while (xc_stack_size(holds)) {
					xce = (xc_entry_t*) xc_stack_pop(holds);
					xce->refcount --;
					assert(xce->refcount >= 0);
				}
			} LEAVE_LOCK(cache);
		}
	}
}
/* }}} */
static void xc_entry_unholds(TSRMLS_D) /* {{{ */
{
	xc_entry_unholds_real(XG(php_holds), xc_php_caches, xc_php_hcache.size TSRMLS_CC);
	xc_entry_unholds_real(XG(var_holds), xc_var_caches, xc_var_hcache.size TSRMLS_CC);
}
/* }}} */
static int xc_stat(const char *filename, const char *include_path, struct stat *pbuf TSRMLS_DC) /* {{{ */
{
	char filepath[1024];
	char *paths, *path;
	char *tokbuf;
	int size = strlen(include_path) + 1;
	char tokens[] = { DEFAULT_DIR_SEPARATOR, '\0' };

	paths = (char *)do_alloca(size);
	memcpy(paths, include_path, size);

	for (path = php_strtok_r(paths, tokens, &tokbuf); path; path = php_strtok_r(NULL, tokens, &tokbuf)) {
		if (strlen(path) + strlen(filename) + 1 > 1024) {
			continue;
		}
		snprintf(filepath, sizeof(filepath), "%s/%s", path, filename);
		if (VCWD_STAT(filepath, pbuf) == 0) {
			free_alloca(paths);
			return 0;
		}
	}

	free_alloca(paths);

	return 1;
}
/* }}} */

#define HASH(i) (i)
#define HASH_USTR_L(t, s, l) HASH(zend_u_inline_hash_func(t, s, (l + 1) * sizeof(UChar)))
#define HASH_STR_L(s, l) HASH(zend_inline_hash_func(s, l + 1))
#define HASH_STR(s) HASH_STR_L(s, strlen(s) + 1)
#define HASH_NUM(n) HASH(n)
static inline xc_hash_value_t xc_entry_hash_var(xc_entry_t *xce) /* {{{ */
{
	return UNISW(NOTHING, UG(unicode) ? HASH_USTR_L(xce->name_type, xce->name.uni.val, xce->name.uni.len) :)
		HASH_STR_L(xce->name.str.val, xce->name.str.len);
}
/* }}} */
static inline xc_hash_value_t xc_entry_hash_php(xc_entry_t *xce) /* {{{ */
{
#ifdef HAVE_INODE
	return HASH(xce->data.php->device + xce->data.php->inode);
#else
	return xc_entry_hash_var(xce);
#endif
}
/* }}} */
static int xc_entry_init_key_php(xc_entry_t *xce, char *filename TSRMLS_DC) /* {{{ */
{
	struct stat buf, *pbuf;
	xc_hash_value_t hv;
	int cacheid;
	xc_entry_data_php_t *php;
	char *ptr;

	if (!filename || !SG(request_info).path_translated) {
		return 0;
	}

	do {
		if (strcmp(SG(request_info).path_translated, filename) == 0) {
			/* sapi has already done this stat() for us */
			pbuf = sapi_get_stat(TSRMLS_C);
			if (pbuf) {
				break;
			}
		}

		/* absolute path */
		pbuf = &buf;
		if (IS_ABSOLUTE_PATH(filename, strlen(filename))) {
			if (VCWD_STAT(filename, pbuf) != 0) {
				return 0;
			}
			break;
		}

		/* relative path */
		if (*filename == '.' && (IS_SLASH(filename[1]) || filename[1] == '.')) {
			ptr = filename + 1;
			if (*ptr == '.') {
				while (*(++ptr) == '.');
				if (!IS_SLASH(*ptr)) {
					goto not_relative_path;
				}   
			}

			if (VCWD_STAT(filename, pbuf) != 0) {
				return 0;
			}
			break;
		}
not_relative_path:

		/* use include_path */
		if (xc_stat(filename, PG(include_path), pbuf TSRMLS_CC) != 0) {   
			return 0;
		}
	} while (0);

	if (XG(request_time) - pbuf->st_mtime < 2) {
		return 0;
	}

	UNISW(NOTHING, xce->name_type = IS_STRING;)
	xce->name.str.val = filename;
	xce->name.str.len = strlen(filename);

	php = xce->data.php;
	php->mtime        = pbuf->st_mtime;
#ifdef HAVE_INODE
	php->device       = pbuf->st_dev;
	php->inode        = pbuf->st_ino;
#endif
	php->sourcesize   = pbuf->st_size;


	hv = xc_entry_hash_php(xce);
	cacheid = (hv & xc_php_hcache.mask);
	xce->cache = xc_php_caches[cacheid];
	hv >>= xc_php_hcache.bits;
	xce->hvalue = (hv & xc_php_hentry.mask);

	xce->type = XC_TYPE_PHP;
	return 1;
}
/* }}} */
static zend_op_array *xc_compile_file(zend_file_handle *h, int type TSRMLS_DC) /* {{{ */
{
	xc_sandbox_t sandbox;
	zend_op_array *op_array;
	xc_entry_t xce, *stored_xce;
	xc_entry_data_php_t php;
	xc_cache_t *cache;
	zend_bool clogged = 0;
	zend_bool catched = 0;
	char *filename;
	int old_constinfo_cnt, old_funcinfo_cnt, old_classinfo_cnt;

	if (!xc_initized) {
		assert(0);
	}

	if (!XG(cacher)) {
		op_array = origin_compile_file(h, type TSRMLS_CC);
#ifdef HAVE_XCACHE_OPTIMIZER
		if (XG(optimizer)) {
			xc_optimize(op_array TSRMLS_CC);
		}
#endif
		return op_array;
	}

	/* {{{ prepare key
	 * include_once() and require_once() gives us opened_path
	 * however, include() and require() non-absolute path which break
	 * included_files, and may confuse with (include|require)_once
	 * -- Xuefer
	 */

	filename = h->opened_path ? h->opened_path : h->filename;
	xce.data.php = &php;
	if (!xc_entry_init_key_php(&xce, filename TSRMLS_CC)) {
		return origin_compile_file(h, type TSRMLS_CC);
	}
	cache = xce.cache;
	/* }}} */
	/* {{{ restore */
	/* stale precheck */
	if (cache->compiling) {
		cache->clogs ++; /* is it safe here? */
		return origin_compile_file(h, type TSRMLS_CC);
	}

	stored_xce = NULL;
	op_array = NULL;
	ENTER_LOCK_EX(cache) {
		/* clogged */
		if (cache->compiling) {
			cache->clogs ++;
			op_array = NULL;
			clogged = 1;
			break;
		}

		stored_xce = xc_entry_find_dmz(&xce TSRMLS_CC);
		/* found */
		if (stored_xce) {
#ifdef DEBUG
			fprintf(stderr, "found %s, catch it\n", stored_xce->name.str.val);
#endif
			xc_entry_hold_php_dmz(stored_xce TSRMLS_CC);
			cache->hits ++;
			break;
		}

		cache->compiling = XG(request_time);
		cache->misses ++;
	} LEAVE_LOCK_EX(cache);

	if (catched) {
		cache->compiling = 0;
		zend_bailout();
	}

	/* found */
	if (stored_xce) {
		goto restore;
	}

	/* clogged */
	if (clogged) {
		return origin_compile_file(h, type TSRMLS_CC);
	}
	/* }}} */

	/* {{{ compile */
#ifdef DEBUG
	fprintf(stderr, "compiling %s\n", filename);
#endif

	/* make compile inside sandbox */
	xc_sandbox_init(&sandbox, filename TSRMLS_CC);

	old_classinfo_cnt = zend_hash_num_elements(CG(class_table));
	old_funcinfo_cnt  = zend_hash_num_elements(CG(function_table));
	old_constinfo_cnt  = zend_hash_num_elements(EG(zend_constants));

	zend_try {
		op_array = origin_compile_file(h, type TSRMLS_CC);
	} zend_catch {
		catched = 1;
	} zend_end_try();

	if (catched) {
		goto err_bailout;
	}

	if (op_array == NULL) {
		goto err_oparray;
	}

	filename = h->opened_path ? h->opened_path : h->filename;
	if (xce.name.str.val != filename) {
		xce.name.str.val = filename;
		xce.name.str.len = strlen(filename);
	}

#ifdef HAVE_XCACHE_OPTIMIZER
	if (XG(optimizer)) {
		xc_optimize(op_array TSRMLS_CC);
	}
#endif

	php.op_array      = op_array;

#ifdef HAVE_XCACHE_CONSTANT
	php.constinfo_cnt = zend_hash_num_elements(EG(zend_constants)) - old_constinfo_cnt;
#endif
	php.funcinfo_cnt  = zend_hash_num_elements(CG(function_table)) - old_funcinfo_cnt;
	php.classinfo_cnt = zend_hash_num_elements(CG(class_table))    - old_classinfo_cnt;

#define X_ALLOC_N(var, cnt) do {     \
	if (php.cnt) {                   \
		ECALLOC_N(php.var, php.cnt); \
		if (!php.var) {              \
			goto err_##var;          \
		}                            \
	}                                \
	else {                           \
		php.var = NULL;              \
	}                                \
} while (0)

#ifdef HAVE_XCACHE_CONSTANT
	X_ALLOC_N(constinfos, constinfo_cnt);
#endif
	X_ALLOC_N(funcinfos,  funcinfo_cnt);
	X_ALLOC_N(classinfos, classinfo_cnt);
#undef X_ALLOC
	/* }}} */
	/* {{{ shallow copy, pointers only */ {
		Bucket *b;
		unsigned int i;

#define COPY_H(vartype, var, cnt, name, datatype) do {        \
	for (i = 0; b; i ++, b = b->pListNext) {                  \
		vartype *data = &php.var[i];                          \
                                                              \
		if (i < old_##cnt) {                                  \
			continue;                                         \
		}                                                     \
                                                              \
		assert(i < old_##cnt + php.cnt);                      \
		assert(b->pData);                                     \
		memcpy(&data->name, b->pData, sizeof(datatype));      \
		UNISW(NOTHING, data->type = b->key.type;)             \
		if (UNISW(1, b->key.type == IS_STRING)) {             \
			ZSTR_S(data->key)      = BUCKET_KEY(b);           \
		}                                                     \
		else {                                                \
			ZSTR_U(data->key)      = BUCKET_UKEY(b);          \
		}                                                     \
		data->key_size   = b->nKeyLength;                     \
	}                                                         \
} while(0)

#ifdef HAVE_XCACHE_CONSTANT
		b = EG(zend_constants)->pListHead; COPY_H(xc_constinfo_t, constinfos, constinfo_cnt, constant, zend_constant);
#endif
		b = CG(function_table)->pListHead; COPY_H(xc_funcinfo_t,  funcinfos,  funcinfo_cnt,  func,     zend_function);
		b = CG(class_table)->pListHead;    COPY_H(xc_classinfo_t, classinfos, classinfo_cnt, cest,     xc_cest_t);

#undef COPY_H
		/* for ZE1, cest need to fix inside store */
	}
	/* }}} */
	ENTER_LOCK_EX(cache) { /* {{{ store/add entry */
		stored_xce = xc_entry_store_dmz(&xce TSRMLS_CC);
	} LEAVE_LOCK_EX(cache);
	/* }}} */
#ifdef DEBUG
	fprintf(stderr, "stored\n");
#endif

#define X_FREE(var) \
	if (xce.data.php->var) { \
		efree(xce.data.php->var); \
	} \
err_##var:

	X_FREE(classinfos)
	X_FREE(funcinfos)
#ifdef HAVE_XCACHE_CONSTANT
	X_FREE(constinfos)
#endif
#undef X_FREE

err_oparray:
err_bailout:

	if (xc_test && stored_xce) {
		/* free it, no install. restore now */
		xc_sandbox_free(&sandbox, 0 TSRMLS_CC);
	}
	else {
		xc_sandbox_free(&sandbox, 1 TSRMLS_CC);
	}

	ENTER_LOCK(cache) {
		cache->compiling = 0;
	} LEAVE_LOCK(cache);
	if (catched) {
		zend_bailout();
	}
	if (xc_test && stored_xce) {
#ifdef ZEND_ENGINE_2
		destroy_op_array(op_array TSRMLS_CC);
#else
		destroy_op_array(op_array);
#endif
		efree(op_array);
		h = NULL;
		goto restore;
	}
	return op_array;

restore:
	if (php_check_open_basedir(stored_xce->name.str.val TSRMLS_CC) != 0) {
		return NULL;
	}

#ifdef DEBUG
	fprintf(stderr, "restoring\n");
#endif
	xc_processor_restore_xc_entry_t(&xce, stored_xce, xc_readonly_protection TSRMLS_CC);

	catched = 0;
	zend_try {
		op_array = xc_entry_install(&xce, h TSRMLS_CC);
	} zend_catch {
		catched = 1;
	} zend_end_try();

#define X_FREE(var) \
	if (xce.data.php->var) { \
		efree(xce.data.php->var); \
	}
	X_FREE(classinfos)
	X_FREE(funcinfos)
#ifdef HAVE_XCACHE_CONSTANT
	X_FREE(constinfos)
#endif
#undef X_FREE
	efree(xce.data.php);

	if (catched) {
		zend_bailout();
	}
#ifdef DEBUG
	fprintf(stderr, "restored\n");
#endif
	return op_array;
}
/* }}} */

/* gdb helper functions, but N/A for coredump */
int xc_is_rw(const void *p) /* {{{ */
{
	int i;
	if (!xc_initized) {
		return 0;
	}
	for (i = 0; i < xc_php_hcache.size; i ++) {
		if (xc_shm_is_readwrite(xc_php_caches[i]->shm, p)) {
			return 1;
		}
	}
	for (i = 0; i < xc_var_hcache.size; i ++) {
		if (xc_shm_is_readwrite(xc_var_caches[i]->shm, p)) {
			return 1;
		}
	}
	return 0;
}
/* }}} */
int xc_is_ro(const void *p) /* {{{ */
{
	int i;
	if (!xc_initized) {
		return 0;
	}
	for (i = 0; i < xc_php_hcache.size; i ++) {
		if (xc_shm_is_readonly(xc_php_caches[i]->shm, p)) {
			return 1;
		}
	}
	for (i = 0; i < xc_var_hcache.size; i ++) {
		if (xc_shm_is_readonly(xc_var_caches[i]->shm, p)) {
			return 1;
		}
	}
	return 0;
}
/* }}} */
int xc_is_shm(const void *p) /* {{{ */
{
	return xc_is_ro(p) || xc_is_rw(p);
}
/* }}} */

/* module helper function */
static int xc_init_constant(int module_number TSRMLS_DC) /* {{{ */
{
	typedef struct {
		const char *prefix;
		zend_uchar (*getsize)();
		const char *(*get)(zend_uchar i);
	} xc_meminfo_t;
	xc_meminfo_t nameinfos[] = {
		{ "",        xc_get_op_type_count,   xc_get_op_type   },
		{ "",        xc_get_data_type_count, xc_get_data_type },
		{ "",        xc_get_opcode_count,    xc_get_opcode    },
		{ "OPSPEC_", xc_get_op_spec_count,   xc_get_op_spec   },
		{ NULL, NULL, NULL }
	};
	xc_meminfo_t* p;
	zend_uchar i, count;
	char const_name[96];
	int const_name_len;
	int undefdone = 0;

	for (p = nameinfos; p->getsize; p ++) {
		count = p->getsize();
		for (i = 0; i < count; i ++) {
			const char *name = p->get(i);
			if (!name) continue;
			if (strcmp(name, "UNDEF") == 0) {
				if (undefdone) continue;
				undefdone = 1;
			}
			const_name_len = snprintf(const_name, sizeof(const_name), "XC_%s%s", p->prefix, name);
			zend_register_long_constant(const_name, const_name_len+1, i, CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC);
		}
	}

	zend_register_long_constant(ZEND_STRS("XC_SIZEOF_TEMP_VARIABLE"), sizeof(temp_variable), CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("XC_TYPE_PHP"), XC_TYPE_PHP, CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("XC_TYPE_VAR"), XC_TYPE_VAR, CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC);
	zend_register_stringl_constant(ZEND_STRS("XCACHE_VERSION"), ZEND_STRL(XCACHE_VERSION), CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC);
	zend_register_stringl_constant(ZEND_STRS("XCACHE_MODULES"), ZEND_STRL(XCACHE_MODULES), CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC);
	return 0;
}
/* }}} */
static xc_shm_t *xc_cache_destroy(xc_cache_t **caches, xc_hash_t *hcache) /* {{{ */
{
	int i;
	xc_cache_t *cache;
	xc_shm_t *shm;

	if (!caches) {
		return NULL;
	}
	shm = NULL;
	for (i = 0; i < hcache->size; i ++) {
		cache = caches[i];
		if (cache) {
			if (cache->lck) {
				xc_lock_destroy(cache->lck);
			}
			/* do NOT free
			if (cache->entries) {
				xc_mem_free(cache->mem, cache->entries);
			}
			xc_mem_free(cache->mem, cache);
			*/
			xc_mem_destroy(cache->mem);
			shm = cache->shm;
		}
	}
	free(caches);
	return shm;
}
/* }}} */
static xc_cache_t **xc_cache_init(xc_shm_t *shm, char *ptr, xc_hash_t *hcache, xc_hash_t *hentry, xc_shmsize_t shmsize) /* {{{ */
{
	xc_cache_t **caches = NULL, *cache;
	xc_mem_t *mem;
	time_t now = time(NULL);
	int i;
	xc_memsize_t memsize;

	memsize = shmsize / hcache->size;

	/* Don't let it break out of mem after ALIGNed
	 * This is important for 
	 * Simply loop until it fit our need
	 */
	while (ALIGN(memsize) * hcache->size > shmsize && ALIGN(memsize) != memsize) {
		if (memsize < ALIGN(1)) {
			CHECK(NULL, "cache too small");
		}
		memsize --;
	}

	CHECK(caches = calloc(hcache->size, sizeof(xc_cache_t *)), "caches OOM");

	for (i = 0; i < hcache->size; i ++) {
		CHECK(mem            = xc_mem_init(ptr, memsize), "Failed init memory allocator");
		ptr += memsize;
		CHECK(cache          = xc_mem_calloc(mem, 1, sizeof(xc_cache_t)), "cache OOM");
		CHECK(cache->entries = xc_mem_calloc(mem, hentry->size, sizeof(xc_entry_t*)), "entries OOM");
		CHECK(cache->lck     = xc_lock_init(NULL), "can't create lock");

		cache->hcache  = hcache;
		cache->hentry  = hentry;
		cache->shm     = shm;
		cache->mem     = mem;
		cache->cacheid = i;
		cache->last_gc_deletes = now;
		cache->last_gc_expires = now;
		caches[i] = cache;
	}
	assert(ptr <= (char*)xc_shm_ptr(shm) + shmsize);
	return caches;

err:
	if (caches) {
		xc_cache_destroy(caches, hcache);
	}
	return NULL;
}
/* }}} */
static void xc_destroy() /* {{{ */
{
	xc_shm_t *shm = NULL;

	if (origin_compile_file) {
		zend_compile_file = origin_compile_file;
		origin_compile_file = NULL;
	}

	if (xc_php_caches) {
		shm = xc_cache_destroy(xc_php_caches, &xc_php_hcache);
		xc_php_caches = NULL;
	}
	if (xc_var_caches) {
		shm = xc_cache_destroy(xc_var_caches, &xc_var_hcache);
		xc_var_caches = NULL;
	}
	if (shm) {
		xc_shm_destroy(shm);
	}
}
/* }}} */
static int xc_init(int module_number TSRMLS_DC) /* {{{ */
{
	xc_shm_t *shm;
	char *ptr;

	xc_php_caches = xc_var_caches = NULL;

	if (xc_php_size || xc_var_size) {
		CHECK(shm = xc_shm_init(xc_mmap_path, ALIGN(xc_php_size) + ALIGN(xc_var_size), xc_readonly_protection), "Cannot create shm");
		if (!xc_shm_can_readonly(shm)) {
			xc_readonly_protection = 0;
		}

		ptr = (char *)xc_shm_ptr(shm);
		if (xc_php_size) {
			origin_compile_file = zend_compile_file;
			zend_compile_file = xc_compile_file;

			CHECK(xc_php_caches = xc_cache_init(shm, ptr, &xc_php_hcache, &xc_php_hentry, xc_php_size), "failed init opcode cache");
			ptr += ALIGN(xc_php_size);
		}

		if (xc_var_size) {
			CHECK(xc_var_caches = xc_cache_init(shm, ptr, &xc_var_hcache, &xc_var_hentry, xc_var_size), "failed init variable cache");
		}
	}
	return 1;

err:
	if (xc_php_caches || xc_var_caches) {
		xc_destroy();
		/* shm destroied */
	}
	else if (shm) {
		xc_shm_destroy(shm);
	}
	return 0;
}
/* }}} */
static void xc_request_init(TSRMLS_D) /* {{{ */
{
	int i;

	if (xc_php_hcache.size && !XG(php_holds)) {
		XG(php_holds) = calloc(xc_php_hcache.size, sizeof(xc_stack_t));
		for (i = 0; i < xc_php_hcache.size; i ++) {
			xc_stack_init(&XG(php_holds[i]));
		}
	}

	if (xc_var_hcache.size && !XG(var_holds)) {
		XG(var_holds) = calloc(xc_var_hcache.size, sizeof(xc_stack_t));
		for (i = 0; i < xc_var_hcache.size; i ++) {
			xc_stack_init(&XG(var_holds[i]));
		}
	}

	if (XG(cacher)) {
#if PHP_API_VERSION <= 20041225
		XG(request_time) = time(NULL);
#else
		XG(request_time) = sapi_get_request_time(TSRMLS_C);
#endif
	}
#ifdef HAVE_XCACHE_COVERAGER
	xc_coverager_request_init(TSRMLS_C);
#endif
#if defined(SUHOSIN_PATCH) && SUHOSIN_PATCH && defined(ZEND_ENGINE_2)
	{
		static zend_bool suhosin_trick_done = 0;
		if (!suhosin_trick_done) {
			zend_class_entry *tmpclass = emalloc(sizeof(zend_class_entry));

			suhosin_trick_done = 1;
			tmpclass->type = ZEND_USER_CLASS;
			tmpclass->name = emalloc(5);
			tmpclass->name_length = 4;
			memcpy(tmpclass->name, "test", tmpclass->name_length);

			zend_initialize_class_data(tmpclass, 1 TSRMLS_CC);
			destroy_zend_class(&tmpclass);
		}
	}
#endif
}
/* }}} */
static void xc_request_shutdown(TSRMLS_D) /* {{{ */
{
	xc_entry_unholds(TSRMLS_C);
	xc_gc_expires_php(TSRMLS_C);
	xc_gc_expires_var(TSRMLS_C);
	xc_gc_deletes(TSRMLS_C);
#ifdef HAVE_XCACHE_COVERAGER
	xc_coverager_request_shutdown(TSRMLS_C);
#endif
}
/* }}} */
/* {{{ PHP_GINIT_FUNCTION(xcache) */
static
#ifdef PHP_GINIT_FUNCTION
PHP_GINIT_FUNCTION(xcache)
#else
void xc_init_globals(zend_xcache_globals* xcache_globals TSRMLS_DC)
#endif
{
	memset(xcache_globals, 0, sizeof(zend_xcache_globals));
}
/* }}} */
/* {{{ PHP_GSHUTDOWN_FUNCTION(xcache) */
static
#ifdef PHP_GSHUTDOWN_FUNCTION
PHP_GSHUTDOWN_FUNCTION(xcache)
#else
void xc_shutdown_globals(zend_xcache_globals* xcache_globals TSRMLS_DC)
#endif
{
	int i;

	if (xcache_globals->php_holds != NULL) {
		for (i = 0; i < xc_php_hcache.size; i ++) {
			xc_stack_destroy(&xcache_globals->php_holds[i]);
		}
		free(xcache_globals->php_holds);
		xcache_globals->php_holds = NULL;
	}

	if (xcache_globals->var_holds != NULL) {
		for (i = 0; i < xc_var_hcache.size; i ++) {
			xc_stack_destroy(&xcache_globals->var_holds[i]);
		}
		free(xcache_globals->var_holds);
		xcache_globals->var_holds = NULL;
	}
}
/* }}} */

/* user functions */
static int xcache_admin_auth_check(TSRMLS_D) /* {{{ */
{
	zval **server = NULL;
	zval **user = NULL;
	zval **pass = NULL;
	char *admin_user = NULL;
	char *admin_pass = NULL;
	HashTable *ht;

	if (cfg_get_string("xcache.admin.user", &admin_user) == FAILURE || !admin_user[0]) {
		admin_user = NULL;
	}
	if (cfg_get_string("xcache.admin.pass", &admin_pass) == FAILURE || !admin_pass[0]) {
		admin_pass = NULL;
	}

	if (admin_user == NULL || admin_pass == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "xcache.admin.user and xcache.admin.pass is required");
		zend_bailout();
	}
	if (strlen(admin_pass) != 32) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "unexpect %d bytes of xcache.admin.pass, expected 32 bytes, the password after md5()", strlen(admin_pass));
		zend_bailout();
	}

#ifdef ZEND_ENGINE_2_1
	zend_is_auto_global("_SERVER", sizeof("_SERVER") - 1 TSRMLS_CC);
#endif
	if (zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &server) != SUCCESS || Z_TYPE_PP(server) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "_SERVER is corrupted");
		zend_bailout();
	}
	ht = HASH_OF((*server));

	if (zend_hash_find(ht, "PHP_AUTH_USER", sizeof("PHP_AUTH_USER"), (void **) &user) == FAILURE) {
	 	user = NULL;
	}
	else if (Z_TYPE_PP(user) != IS_STRING) {
		user = NULL;
	}

	if (zend_hash_find(ht, "PHP_AUTH_PW", sizeof("PHP_AUTH_PW"), (void **) &pass) == FAILURE) {
	 	pass = NULL;
	}
	else if (Z_TYPE_PP(pass) != IS_STRING) {
		pass = NULL;
	}

	if (user != NULL && pass != NULL && strcmp(admin_user, Z_STRVAL_PP(user)) == 0) {
		PHP_MD5_CTX context;
		char md5str[33];
		unsigned char digest[16];

		PHP_MD5Init(&context);
		PHP_MD5Update(&context, (unsigned char *) Z_STRVAL_PP(pass), Z_STRLEN_PP(pass));
		PHP_MD5Final(digest, &context);

		md5str[0] = '\0';
		make_digest(md5str, digest);
		if (strcmp(admin_pass, md5str) == 0) {
			return 1;
		}
	}

#define STR "WWW-authenticate: basic realm='XCache Administration'"
	sapi_add_header_ex(STR, sizeof(STR) - 1, 1, 1 TSRMLS_CC);
#undef STR
#define STR "HTTP/1.0 401 Unauthorized"
	sapi_add_header_ex(STR, sizeof(STR) - 1, 1, 1 TSRMLS_CC);
#undef STR
	ZEND_PUTS("XCache Auth Failed. User and Password is case sense\n");

	zend_bailout();
	return 0;
}
/* }}} */
/* {{{ xcache_admin_operate */
typedef enum { XC_OP_COUNT, XC_OP_INFO, XC_OP_LIST, XC_OP_CLEAR } xcache_op_type;
static void xcache_admin_operate(xcache_op_type optype, INTERNAL_FUNCTION_PARAMETERS)
{
	long type;
	int size;
	xc_cache_t **caches, *cache;
	long id = 0;

	if (!xc_initized) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "XCache is not initized");
		RETURN_FALSE;
	}

	xcache_admin_auth_check(TSRMLS_C);

	if (optype == XC_OP_COUNT) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &type) == FAILURE) {
			return;
		}
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &type, &id) == FAILURE) {
		return;
	}

	switch (type) {
		case XC_TYPE_PHP:
			size = xc_php_hcache.size;
			caches = xc_php_caches;
			break;

		case XC_TYPE_VAR:
			size = xc_var_hcache.size;
			caches = xc_var_caches;
			break;

		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown type %ld", type);
			RETURN_FALSE;
	}

	switch (optype) {
		case XC_OP_COUNT:
			RETURN_LONG(size)
			break;

		case XC_OP_INFO:
		case XC_OP_LIST:
			if (id < 0 || id >= size) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cache not exists");
				RETURN_FALSE;
			}

			array_init(return_value);

			cache = caches[id];
			ENTER_LOCK(cache) {
				if (optype == XC_OP_INFO) {
					xc_fillinfo_dmz(type, cache, return_value TSRMLS_CC);
				}
				else {
					xc_filllist_dmz(cache, return_value TSRMLS_CC);
				}
			} LEAVE_LOCK(cache);
			break;
		case XC_OP_CLEAR:
			{
				xc_entry_t *e;
				int i, c;

				if (id < 0 || id >= size) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cache not exists");
					RETURN_FALSE;
				}

				cache = caches[id];
				ENTER_LOCK(cache) {
					for (i = 0, c = cache->hentry->size; i < c; i ++) {
						for (e = cache->entries[i]; e; e = e->next) {
							xc_entry_remove_dmz(e TSRMLS_CC);
						}
						cache->entries[i] = NULL;
					}
				} LEAVE_LOCK(cache);
				xc_gc_deletes(TSRMLS_C);
			}
			break;

		default:
			assert(0);
	}
}
/* }}} */
/* {{{ proto int xcache_count(int type)
   Return count of cache on specified cache type */
PHP_FUNCTION(xcache_count)
{
	xcache_admin_operate(XC_OP_COUNT, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto array xcache_info(int type, int id)
   Get cache info by id on specified cache type */
PHP_FUNCTION(xcache_info)
{
	xcache_admin_operate(XC_OP_INFO, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto array xcache_list(int type, int id)
   Get cache entries list by id on specified cache type */
PHP_FUNCTION(xcache_list)
{
	xcache_admin_operate(XC_OP_LIST, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto array xcache_clear_cache(int type, int id)
   Clear cache by id on specified cache type */
PHP_FUNCTION(xcache_clear_cache)
{
	xcache_admin_operate(XC_OP_CLEAR, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

static int xc_entry_init_key_var(xc_entry_t *xce, zval *name TSRMLS_DC) /* {{{ */
{
	xc_hash_value_t hv;
	int cacheid;

	switch (Z_TYPE_P(name)) {
#ifdef IS_UNICODE
		case IS_UNICODE:
#endif
		case IS_STRING:
			break;
		default:
#ifdef IS_UNICODE
			convert_to_text(name);
#else
			convert_to_string(name);
#endif
	}
#ifdef IS_UNICODE
	xce->name_type = name->type;
#endif
	xce->name = name->value;

	hv = xc_entry_hash_var(xce);

	cacheid = (hv & xc_var_hcache.mask);
	xce->cache = xc_var_caches[cacheid];
	hv >>= xc_var_hcache.bits;
	xce->hvalue = (hv & xc_var_hentry.mask);

	xce->type = XC_TYPE_VAR;
	return SUCCESS;
}
/* }}} */
/* {{{ proto mixed xcache_get(string name)
   Get cached data by specified name */
PHP_FUNCTION(xcache_get)
{
	xc_entry_t xce, *stored_xce;
	xc_entry_data_var_t var;
	zval *name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &name) == FAILURE) {
		return;
	}
	xce.data.var = &var;
	xc_entry_init_key_var(&xce, name TSRMLS_CC);

	ENTER_LOCK(xce.cache) {
		stored_xce = xc_entry_find_dmz(&xce TSRMLS_CC);
		if (stored_xce) {
			if (XG(request_time) <= stored_xce->ctime + stored_xce->ttl) {
				xc_processor_restore_zval(return_value, stored_xce->data.var->value TSRMLS_CC);
				/* return */
				break;
			}
			else {
				xc_entry_remove_dmz(stored_xce TSRMLS_CC);
			}
		}

		RETVAL_NULL();
	} LEAVE_LOCK(xce.cache);
}
/* }}} */
/* {{{ proto bool  xcache_set(string name, mixed value [, int ttl])
   Store data to cache by specified name */
PHP_FUNCTION(xcache_set)
{
	xc_entry_t xce, *stored_xce;
	xc_entry_data_var_t var;
	zval *name;
	zval *value;

	xce.ttl = XG(var_ttl);
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|l", &name, &value, &xce.ttl) == FAILURE) {
		return;
	}

	/* max ttl */
	if (xc_var_maxttl && (!xce.ttl || xce.ttl > xc_var_maxttl)) {
		xce.ttl = xc_var_maxttl;
	}

	xce.data.var = &var;
	xc_entry_init_key_var(&xce, name TSRMLS_CC);

	ENTER_LOCK(xce.cache) {
		stored_xce = xc_entry_find_dmz(&xce TSRMLS_CC);
		if (stored_xce) {
			xc_entry_remove_dmz(stored_xce TSRMLS_CC);
		}
		var.value = value;
		RETVAL_BOOL(xc_entry_store_dmz(&xce TSRMLS_CC) != NULL ? 1 : 0);
	} LEAVE_LOCK(xce.cache);
}
/* }}} */
/* {{{ proto bool  xcache_isset(string name)
   Check if an entry exists in cache by specified name */
PHP_FUNCTION(xcache_isset)
{
	xc_entry_t xce, *stored_xce;
	xc_entry_data_var_t var;
	zval *name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &name) == FAILURE) {
		return;
	}
	xce.data.var = &var;
	xc_entry_init_key_var(&xce, name TSRMLS_CC);

	ENTER_LOCK(xce.cache) {
		stored_xce = xc_entry_find_dmz(&xce TSRMLS_CC);
		if (stored_xce) {
			if (!VAR_ENTRY_EXPIRED(stored_xce)) {
				RETVAL_TRUE;
				/* return */
				break;
			}
			else {
				xc_entry_remove_dmz(stored_xce TSRMLS_CC);
			}
		}

		RETVAL_FALSE;
	} LEAVE_LOCK(xce.cache);
}
/* }}} */
/* {{{ proto bool  xcache_unset(string name)
   Unset existing data in cache by specified name */
PHP_FUNCTION(xcache_unset)
{
	xc_entry_t xce, *stored_xce;
	xc_entry_data_var_t var;
	zval *name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &name) == FAILURE) {
		return;
	}
	xce.data.var = &var;
	xc_entry_init_key_var(&xce, name TSRMLS_CC);

	ENTER_LOCK(xce.cache) {
		stored_xce = xc_entry_find_dmz(&xce TSRMLS_CC);
		if (stored_xce) {
			xc_entry_remove_dmz(stored_xce TSRMLS_CC);
			RETVAL_TRUE;
		}
		else {
			RETVAL_FALSE;
		}
	} LEAVE_LOCK(xce.cache);
}
/* }}} */
static inline void xc_var_inc_dec(int inc, INTERNAL_FUNCTION_PARAMETERS) /* {{{ */
{
	xc_entry_t xce, *stored_xce;
	xc_entry_data_var_t var, *stored_var;
	zval *name;
	long count = 1;
	long value = 0;
	zval oldzval;

	xce.ttl = XG(var_ttl);
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|ll", &name, &count, &xce.ttl) == FAILURE) {
		return;
	}

	/* max ttl */
	if (xc_var_maxttl && (!xce.ttl || xce.ttl > xc_var_maxttl)) {
		xce.ttl = xc_var_maxttl;
	}

	xce.data.var = &var;
	xc_entry_init_key_var(&xce, name TSRMLS_CC);

	ENTER_LOCK(xce.cache) {
		stored_xce = xc_entry_find_dmz(&xce TSRMLS_CC);
		if (stored_xce) {
#ifdef DEBUG
			fprintf(stderr, "incdec: gotxce %s\n", xce.name.str.val);
#endif
			/* timeout */
			if (VAR_ENTRY_EXPIRED(stored_xce)) {
#ifdef DEBUG
				fprintf(stderr, "incdec: expired\n");
#endif
				xc_entry_remove_dmz(stored_xce TSRMLS_CC);
				stored_xce = NULL;
			}
			else {
				/* do it in place */
				stored_var = stored_xce->data.var;
				if (Z_TYPE_P(stored_var->value) == IS_LONG) {
					stored_xce->ctime = XG(request_time);
					stored_xce->ttl   = xce.ttl;
#ifdef DEBUG
					fprintf(stderr, "incdec: islong\n");
#endif
					value = Z_LVAL_P(stored_var->value);
					value += (inc == 1 ? count : - count);
					RETVAL_LONG(value);
					Z_LVAL_P(stored_var->value) = value;
					break; /* leave lock */
				}
				else {
#ifdef DEBUG
					fprintf(stderr, "incdec: notlong\n");
#endif
					xc_processor_restore_zval(&oldzval, stored_xce->data.var->value TSRMLS_CC);
					convert_to_long(&oldzval);
					value = Z_LVAL(oldzval);
					zval_dtor(&oldzval);
				}
			}
		}
#ifdef DEBUG
		else {
			fprintf(stderr, "incdec: %s not found\n", xce.name.str.val);
		}
#endif

		value += (inc == 1 ? count : - count);
		RETVAL_LONG(value);
		var.value = return_value;

		if (stored_xce) {
			xce.atime = stored_xce->atime;
			xce.ctime = stored_xce->ctime;
			xce.hits  = stored_xce->hits;
			xc_entry_remove_dmz(stored_xce TSRMLS_CC);
		}
		xc_entry_store_dmz(&xce TSRMLS_CC);

	} LEAVE_LOCK(xce.cache);
}
/* }}} */
/* {{{ proto int xcache_inc(string name [, int value [, int ttl]])
   Increase an int counter in cache by specified name, create it if not exists */
PHP_FUNCTION(xcache_inc)
{
	xc_var_inc_dec(1, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto int xcache_dec(string name [, int value [, int ttl]])
   Decrease an int counter in cache by specified name, create it if not exists */
PHP_FUNCTION(xcache_dec)
{
	xc_var_inc_dec(-1, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto string xcache_asm(string filename)
 */
#ifdef HAVE_XCACHE_ASSEMBLER
PHP_FUNCTION(xcache_asm)
{
}
#endif
/* }}} */
#ifdef HAVE_XCACHE_DISASSEMBLER
/* {{{ proto array  xcache_dasm_file(string filename)
   Disassemble file into opcode array by filename */
PHP_FUNCTION(xcache_dasm_file)
{
	char *filename;
	long filename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, &filename_len) == FAILURE) {
		return;
	}
	if (!filename_len) RETURN_FALSE;

	xc_dasm_file(return_value, filename TSRMLS_CC);
}
/* }}} */
/* {{{ proto array  xcache_dasm_string(string code)
   Disassemble php code into opcode array */
PHP_FUNCTION(xcache_dasm_string)
{
	zval *code;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &code) == FAILURE) {
		return;
	}
	xc_dasm_string(return_value, code TSRMLS_CC);
}
/* }}} */
#endif
/* {{{ proto string xcache_encode(string filename)
   Encode php file into XCache opcode encoded format */
#ifdef HAVE_XCACHE_ENCODER
PHP_FUNCTION(xcache_encode)
{
}
#endif
/* }}} */
/* {{{ proto bool xcache_decode_file(string filename)
   Decode(load) opcode from XCache encoded format file */
#ifdef HAVE_XCACHE_DECODER
PHP_FUNCTION(xcache_decode_file)
{
}
#endif
/* }}} */
/* {{{ proto bool xcache_decode_string(string data)
   Decode(load) opcode from XCache encoded format data */
#ifdef HAVE_XCACHE_DECODER
PHP_FUNCTION(xcache_decode_string)
{
}
#endif
/* }}} */
/* {{{ xc_call_getter */
typedef const char *(xc_name_getter_t)(zend_uchar type);
static void xc_call_getter(xc_name_getter_t getter, int count, INTERNAL_FUNCTION_PARAMETERS)
{
	long spec;
	const char *name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &spec) == FAILURE) {
		return;
	}
	if (spec >= 0 && spec < count) {
		name = getter((zend_uchar) spec);
		if (name) {
			/* RETURN_STRING */
			int len = strlen(name);
			return_value->value.str.len = len;
			return_value->value.str.val = estrndup(name, len);
			return_value->type = IS_STRING; 
			return;
		}
	}
	RETURN_NULL();
}
/* }}} */
/* {{{ proto string xcache_get_op_type(int op_type) */
PHP_FUNCTION(xcache_get_op_type)
{
	xc_call_getter(xc_get_op_type, xc_get_op_type_count(), INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto string xcache_get_data_type(int type) */
PHP_FUNCTION(xcache_get_data_type)
{
	xc_call_getter(xc_get_data_type, xc_get_data_type_count(), INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto string xcache_get_opcode(int opcode) */
PHP_FUNCTION(xcache_get_opcode)
{
	xc_call_getter(xc_get_opcode, xc_get_opcode_count(), INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto string xcache_get_op_spec(int op_type) */
PHP_FUNCTION(xcache_get_op_spec)
{
	xc_call_getter(xc_get_op_spec, xc_get_op_spec_count(), INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
#ifdef HAVE_XCACHE_OPCODE_SPEC_DEF
/* {{{ proto string xcache_get_opcode_spec(int opcode) */
PHP_FUNCTION(xcache_get_opcode_spec)
{
	long spec;
	const xc_opcode_spec_t *opspec;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &spec) == FAILURE) {
		return;
	}
	if ((zend_uchar) spec <= xc_get_opcode_spec_count()) {
		opspec = xc_get_opcode_spec((zend_uchar) spec);
		if (opspec) {
			array_init(return_value);
			add_assoc_long_ex(return_value, ZEND_STRS("ext"), opspec->ext);
			add_assoc_long_ex(return_value, ZEND_STRS("op1"), opspec->op1);
			add_assoc_long_ex(return_value, ZEND_STRS("op2"), opspec->op2);
			add_assoc_long_ex(return_value, ZEND_STRS("res"), opspec->res);
			return;
		}
	}
	RETURN_NULL();
}
/* }}} */
#endif
/* {{{ proto mixed xcache_get_special_value(zval value) */
PHP_FUNCTION(xcache_get_special_value)
{
	zval *value;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &value) == FAILURE) {
		return;
	}

	if (value->type == IS_CONSTANT) {
		*return_value = *value;
		zval_copy_ctor(return_value);
		return_value->type = UNISW(IS_STRING, UG(unicode) ? IS_UNICODE : IS_STRING);
		return;
	}

	if (value->type == IS_CONSTANT_ARRAY) {
		*return_value = *value;
		zval_copy_ctor(return_value);
		return_value->type = IS_ARRAY;
		return;
	}

	RETURN_NULL();
}
/* }}} */
/* {{{ proto string xcache_coredump(int op_type) */
PHP_FUNCTION(xcache_coredump)
{
	if (xc_test) {
		raise(SIGSEGV);
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "xcache.test must be enabled to test xcache_coredump()");
	}
}
/* }}} */
/* {{{ proto string xcache_is_autoglobal(string name) */
PHP_FUNCTION(xcache_is_autoglobal)
{
	char *name;
	long name_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len) == FAILURE) {
		return;
	}

	RETURN_BOOL(zend_hash_exists(CG(auto_globals), name, name_len + 1));
}
/* }}} */
static function_entry xcache_functions[] = /* {{{ */
{
	PHP_FE(xcache_count,             NULL)
	PHP_FE(xcache_info,              NULL)
	PHP_FE(xcache_list,              NULL)
	PHP_FE(xcache_clear_cache,       NULL)
	PHP_FE(xcache_coredump,          NULL)
#ifdef HAVE_XCACHE_ASSEMBLER
	PHP_FE(xcache_asm,               NULL)
#endif
#ifdef HAVE_XCACHE_DISASSEMBLER
	PHP_FE(xcache_dasm_file,         NULL)
	PHP_FE(xcache_dasm_string,       NULL)
#endif
#ifdef HAVE_XCACHE_ENCODER
	PHP_FE(xcache_encode,            NULL)
#endif
#ifdef HAVE_XCACHE_DECODER
	PHP_FE(xcache_decode_file,       NULL)
	PHP_FE(xcache_decode_string,     NULL)
#endif
#ifdef HAVE_XCACHE_COVERAGER
	PHP_FE(xcache_coverager_decode,  NULL)
#endif
	PHP_FE(xcache_get_special_value, NULL)
	PHP_FE(xcache_get_op_type,       NULL)
	PHP_FE(xcache_get_data_type,     NULL)
	PHP_FE(xcache_get_opcode,        NULL)
#ifdef HAVE_XCACHE_OPCODE_SPEC_DEF
	PHP_FE(xcache_get_opcode_spec,   NULL)
#endif
	PHP_FE(xcache_is_autoglobal,     NULL)
	PHP_FE(xcache_inc,               NULL)
	PHP_FE(xcache_dec,               NULL)
	PHP_FE(xcache_get,               NULL)
	PHP_FE(xcache_set,               NULL)
	PHP_FE(xcache_isset,             NULL)
	PHP_FE(xcache_unset,             NULL)
	{NULL, NULL,                     NULL}
};
/* }}} */

/* old signal handlers {{{ */
typedef void (*xc_sighandler_t)(int);
#define FOREACH_SIG(sig) static xc_sighandler_t old_##sig##_handler = NULL
#include "foreachcoresig.h"
#undef FOREACH_SIG
/* }}} */
static void xcache_signal_handler(int sig);
static void xcache_restore_signal_handler() /* {{{ */
{
#define FOREACH_SIG(sig) do { \
	if (old_##sig##_handler != xcache_signal_handler) { \
		signal(sig, old_##sig##_handler); \
	} \
	else { \
		signal(sig, SIG_DFL); \
	} \
} while (0)
#include "foreachcoresig.h"
#undef FOREACH_SIG
}
/* }}} */
static void xcache_init_signal_handler() /* {{{ */
{
#define FOREACH_SIG(sig) \
	old_##sig##_handler = signal(sig, xcache_signal_handler)
#include "foreachcoresig.h"
#undef FOREACH_SIG
}
/* }}} */
static void xcache_signal_handler(int sig) /* {{{ */
{
	xcache_restore_signal_handler();
	if (xc_coredump_dir && xc_coredump_dir[0]) {
		chdir(xc_coredump_dir);
	}
	raise(sig);
}
/* }}} */

/* {{{ PHP_INI */

static PHP_INI_MH(xc_OnUpdateBool)
{
	zend_bool *p = (zend_bool *)mh_arg1;

	if (strncasecmp("on", new_value, sizeof("on"))) {
		*p = (zend_bool) atoi(new_value);
	}
	else {
		*p = (zend_bool) 1;
	}
	return SUCCESS;
}

static PHP_INI_MH(xc_OnUpdateString)
{
	char **p = (char**)mh_arg1;
	if (*p) {
		pefree(*p, 1);
	}
	*p = pemalloc(strlen(new_value) + 1, 1);
	strcpy(*p, new_value);
	return SUCCESS;
}

#ifndef ZEND_ENGINE_2
#define OnUpdateLong OnUpdateInt
#endif

#ifdef ZEND_WIN32
#	define DEFAULT_PATH "xcache"
#else
#	define DEFAULT_PATH "/dev/zero"
#endif
PHP_INI_BEGIN()
	PHP_INI_ENTRY1     ("xcache.mmap_path",     DEFAULT_PATH, PHP_INI_SYSTEM, xc_OnUpdateString,   &xc_mmap_path)
	PHP_INI_ENTRY1     ("xcache.coredump_directory",      "", PHP_INI_SYSTEM, xc_OnUpdateString,   &xc_coredump_dir)
	PHP_INI_ENTRY1     ("xcache.test",                   "0", PHP_INI_SYSTEM, xc_OnUpdateBool,     &xc_test)
	PHP_INI_ENTRY1     ("xcache.readonly_protection",    "0", PHP_INI_SYSTEM, xc_OnUpdateBool,     &xc_readonly_protection)

	STD_PHP_INI_BOOLEAN("xcache.cacher",                 "1", PHP_INI_ALL,    OnUpdateBool,        cacher,            zend_xcache_globals, xcache_globals)
#ifdef HAVE_XCACHE_OPTIMIZER
	STD_PHP_INI_BOOLEAN("xcache.optimizer",              "0", PHP_INI_ALL,    OnUpdateBool,        optimizer,         zend_xcache_globals, xcache_globals)
#endif
	STD_PHP_INI_BOOLEAN("xcache.var_ttl",                "0", PHP_INI_ALL,    OnUpdateLong,        var_ttl,   zend_xcache_globals, xcache_globals)
#ifdef HAVE_XCACHE_COVERAGER
	PHP_INI_ENTRY1     ("xcache.coveragedump_directory", "/tmp/pcov/", PHP_INI_SYSTEM, xc_OnUpdateString,   &xc_coveragedump_dir)
	STD_PHP_INI_BOOLEAN("xcache.coveragedumper" ,                 "0", PHP_INI_ALL,    OnUpdateBool,        coveragedumper,    zend_xcache_globals, xcache_globals)
#endif
PHP_INI_END()
/* }}} */
static int xc_config_long_disp(char *name, char *default_value) /* {{{ */
{
	char *value;
	char buf[100];

	if (cfg_get_string(name, &value) != SUCCESS) {
		sprintf(buf, "%s (default)", default_value);
		php_info_print_table_row(2, name, buf);
	}
	else {
		php_info_print_table_row(2, name, value);
	}

	return SUCCESS;
}
/* }}} */
#define xc_config_hash_disp xc_config_long_disp
/* {{{ PHP_MINFO_FUNCTION(xcache) */
static PHP_MINFO_FUNCTION(xcache)
{
	char buf[100];
	char *ptr;

	php_info_print_table_start();
	php_info_print_table_header(2, "XCache Support", XCACHE_MODULES);
	php_info_print_table_row(2, "Version", XCACHE_VERSION);
	php_info_print_table_row(2, "Modules Built", XCACHE_MODULES);
	php_info_print_table_row(2, "Readonly Protection", xc_readonly_protection ? "enabled" : "N/A");

	if (xc_php_size) {
		ptr = _php_math_number_format(xc_php_size, 0, '.', ',');
		sprintf(buf, "enabled, %s bytes, %d split(s), with %d slots each", ptr, xc_php_hcache.size, xc_php_hentry.size);
		php_info_print_table_row(2, "Opcode Cache", buf);
		efree(ptr);
	}
	else {
		php_info_print_table_row(2, "Opcode Cache", "disabled");
	}
	if (xc_var_size) {
		ptr = _php_math_number_format(xc_var_size, 0, '.', ',');
		sprintf(buf, "enabled, %s bytes, %d split(s), with %d slots each", ptr, xc_var_hcache.size, xc_var_hentry.size);
		php_info_print_table_row(2, "Variable Cache", buf);
		efree(ptr);
	}
	else {
		php_info_print_table_row(2, "Variable Cache", "disabled");
	}
#ifdef HAVE_XCACHE_COVERAGER
	php_info_print_table_row(2, "Coverage Dumper", XG(coveragedumper) && xc_coveragedump_dir && xc_coveragedump_dir[0] ? "enabled" : "disabled");
#endif
	php_info_print_table_end();

	php_info_print_table_start();
	php_info_print_table_header(2, "Directive ", "Value");
	xc_config_long_disp("xcache.size",        "0");
	xc_config_hash_disp("xcache.count",       "1");
	xc_config_hash_disp("xcache.slots",      "8K");
	xc_config_hash_disp("xcache.ttl",         "0");
	xc_config_hash_disp("xcache.gc_interval", "0");

	xc_config_long_disp("xcache.var_size",           "0");
	xc_config_hash_disp("xcache.var_count",          "1");
	xc_config_hash_disp("xcache.var_slots",         "8K");
	xc_config_hash_disp("xcache.var_maxttl",         "0");
	xc_config_hash_disp("xcache.var_gc_interval",  "300");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */
/* {{{ extension startup */
static void xc_zend_extension_register(zend_extension *new_extension, DL_HANDLE handle)
{
    zend_extension extension;

    extension = *new_extension;
    extension.handle = handle;

    zend_extension_dispatch_message(ZEND_EXTMSG_NEW_EXTENSION, &extension);

    zend_llist_add_element(&zend_extensions, &extension);
#ifdef DEBUG
	fprintf(stderr, "registered\n");
#endif
}

/* dirty check */
#if defined(COMPILE_DL_XCACHE) && (defined(ZEND_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__))
#	define zend_append_version_info(x) do { } while (0)
#else
extern void zend_append_version_info(zend_extension *extension);
#endif
static int xc_zend_extension_startup(zend_extension *extension)
{
    if (extension->startup) {
        if (extension->startup(extension) != SUCCESS) {
			return FAILURE;
        }
        zend_append_version_info(extension);
    }
	return SUCCESS;
}
/* }}} */
static int xc_config_hash(xc_hash_t *p, char *name, char *default_value) /* {{{ */
{
	int bits, size;
	char *value;

	if (cfg_get_string(name, &value) != SUCCESS) {
		value = default_value;
	}

	p->size = zend_atoi(value, strlen(value));
	for (size = 1, bits = 1; size < p->size; bits ++, size <<= 1) {
		/* empty body */
	}
	p->size = size;
	p->bits = bits;
	p->mask = size - 1;

	return SUCCESS;
}
/* }}} */
static int xc_config_long(zend_ulong *p, char *name, char *default_value) /* {{{ */
{
	char *value;

	if (cfg_get_string(name, &value) != SUCCESS) {
		value = default_value;
	}

	*p = zend_atoi(value, strlen(value));
	return SUCCESS;
}
/* }}} */
/* {{{ PHP_MINIT_FUNCTION(xcache) */
static PHP_MINIT_FUNCTION(xcache)
{
	char *env;

	xc_module_gotup = 1;
	if (!xc_zend_extension_gotup) {
		if (zend_get_extension(XCACHE_NAME) == NULL) {
			xc_zend_extension_register(&zend_extension_entry, 0);
			xc_zend_extension_startup(&zend_extension_entry);
		}
	}

#ifndef PHP_GINIT
	ZEND_INIT_MODULE_GLOBALS(xcache, xc_init_globals, xc_shutdown_globals);
#endif
	REGISTER_INI_ENTRIES();

	if (strcmp(sapi_module.name, "cli") == 0) {
		if ((env = getenv("XCACHE_TEST")) != NULL) {
			zend_alter_ini_entry("xcache.test", sizeof("xcache.test"), env, strlen(env) + 1, PHP_INI_SYSTEM, PHP_INI_STAGE_STARTUP);
		}
		if (!xc_test) {
			/* disable cache for cli except for test */
			xc_php_size = xc_var_size = 0;
		}
	}

	xc_config_long(&xc_php_size,       "xcache.size",        "0");
	xc_config_hash(&xc_php_hcache,     "xcache.count",       "1");
	xc_config_hash(&xc_php_hentry,     "xcache.slots",      "8K");
	xc_config_long(&xc_php_ttl,        "xcache.ttl",         "0");
	xc_config_long(&xc_php_gc_interval, "xcache.gc_interval", "0");

	xc_config_long(&xc_var_size,       "xcache.var_size",          "0");
	xc_config_hash(&xc_var_hcache,     "xcache.var_count",         "1");
	xc_config_hash(&xc_var_hentry,     "xcache.var_slots",        "8K");
	xc_config_long(&xc_var_maxttl,     "xcache.var_maxttl",        "0");
	xc_config_long(&xc_var_gc_interval, "xcache.var_gc_interval", "120");

	if (xc_php_size <= 0) {
		xc_php_size = xc_php_hcache.size = 0;
	}
	if (xc_var_size <= 0) {
		xc_var_size = xc_var_hcache.size = 0;
	}

	if (xc_coredump_dir && xc_coredump_dir[0]) {
		xcache_init_signal_handler();
	}

	xc_init_constant(module_number TSRMLS_CC);

	if ((xc_php_size || xc_var_size) && xc_mmap_path && xc_mmap_path[0]) {
		if (!xc_init(module_number TSRMLS_CC)) {
			zend_error(E_ERROR, "XCache: Cannot init");
			goto err_init;
		}
		xc_initized = 1;
	}

#ifdef HAVE_XCACHE_COVERAGER
	xc_coverager_init(module_number TSRMLS_CC);
#endif

	return SUCCESS;

err_init:
	return FAILURE;
}
/* }}} */
/* {{{ PHP_MSHUTDOWN_FUNCTION(xcache) */
static PHP_MSHUTDOWN_FUNCTION(xcache)
{
	if (xc_initized) {
		xc_destroy();
		xc_initized = 0;
	}
	if (xc_mmap_path) {
		pefree(xc_mmap_path, 1);
		xc_mmap_path = NULL;
	}

#ifdef HAVE_XCACHE_COVERAGER
	xc_coverager_destroy();
#endif

	if (xc_coredump_dir && xc_coredump_dir[0]) {
		xcache_restore_signal_handler();
	}
	if (xc_coredump_dir) {
		pefree(xc_coredump_dir, 1);
		xc_coredump_dir = NULL;
	}
#ifndef PHP_GINIT
#	ifdef ZTS
	ts_free_id(xcache_globals_id);
#	else
	xc_shutdown_globals(&xcache_globals TSRMLS_CC);
#	endif
#endif

	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */
/* {{{ PHP_RINIT_FUNCTION(xcache) */
static PHP_RINIT_FUNCTION(xcache)
{
	xc_request_init(TSRMLS_C);
	return SUCCESS;
}
/* }}} */
/* {{{ PHP_RSHUTDOWN_FUNCTION(xcache) */
#ifndef ZEND_ENGINE_2
static PHP_RSHUTDOWN_FUNCTION(xcache)
#else
static ZEND_MODULE_POST_ZEND_DEACTIVATE_D(xcache)
#endif
{
#ifdef ZEND_ENGINE_2
	TSRMLS_FETCH();
#endif

	xc_request_shutdown(TSRMLS_C);
	return SUCCESS;
}
/* }}} */
/* {{{ module definition structure */

zend_module_entry xcache_module_entry = {
	STANDARD_MODULE_HEADER,
	"XCache",
	xcache_functions,
	PHP_MINIT(xcache),
	PHP_MSHUTDOWN(xcache),
	PHP_RINIT(xcache),
#ifndef ZEND_ENGINE_2
	PHP_RSHUTDOWN(xcache),
#else
	NULL,
#endif
	PHP_MINFO(xcache),
	XCACHE_VERSION,
#ifdef PHP_GINIT
	PHP_MODULE_GLOBALS(xcache),
	PHP_GINIT(xcache),
	PHP_GSHUTDOWN(xcache),
#endif
#ifdef ZEND_ENGINE_2
	ZEND_MODULE_POST_ZEND_DEACTIVATE_N(xcache),
#else
	NULL,
	NULL,
#endif
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_XCACHE
ZEND_GET_MODULE(xcache)
#endif
/* }}} */
ZEND_DLEXPORT int xcache_zend_startup(zend_extension *extension) /* {{{ */
{
	if (xc_zend_extension_gotup) {
		return FAILURE;
	}
	xc_zend_extension_gotup = 1;
	if (!xc_module_gotup) {
		return zend_startup_module(&xcache_module_entry);
	}
	return SUCCESS;
}
/* }}} */
ZEND_DLEXPORT void xcache_zend_shutdown(zend_extension *extension) /* {{{ */
{
	/* empty */
}
/* }}} */
ZEND_DLEXPORT void xcache_statement_handler(zend_op_array *op_array) /* {{{ */
{
#ifdef HAVE_XCACHE_COVERAGER
	xc_coverager_handle_ext_stmt(op_array, ZEND_EXT_STMT);
#endif
}
/* }}} */
ZEND_DLEXPORT void xcache_fcall_begin_handler(zend_op_array *op_array) /* {{{ */
{
#if 0
	xc_coverager_handle_ext_stmt(op_array, ZEND_EXT_FCALL_BEGIN);
#endif
}
/* }}} */
ZEND_DLEXPORT void xcache_fcall_end_handler(zend_op_array *op_array) /* {{{ */
{
#if 0
	xc_coverager_handle_ext_stmt(op_array, ZEND_EXT_FCALL_END);
#endif
}
/* }}} */
/* {{{ zend extension definition structure */
ZEND_DLEXPORT zend_extension zend_extension_entry = {
	XCACHE_NAME,
	XCACHE_VERSION,
	XCACHE_AUTHOR,
	XCACHE_URL,
	XCACHE_COPYRIGHT,
	xcache_zend_startup,
	xcache_zend_shutdown,
	NULL,           /* activate_func_t */
	NULL,           /* deactivate_func_t */
	NULL,           /* message_handler_func_t */
	NULL,           /* op_array_handler_func_t */
	xcache_statement_handler,
	xcache_fcall_begin_handler,
	xcache_fcall_end_handler,
	NULL,           /* op_array_ctor_func_t */
	NULL,           /* op_array_dtor_func_t */
	STANDARD_ZEND_EXTENSION_PROPERTIES
};

#ifndef ZEND_EXT_API
#	define ZEND_EXT_API ZEND_DLEXPORT
#endif
#if COMPILE_DL_XCACHE
ZEND_EXTENSION();
#endif
/* }}} */
