#if 0
#define SHOW_DPRINT
#endif

/* {{{ macros */
#include "xc_cacher.h"
#include "xc_cache.h"
#include "xcache.h"
#include "xc_processor.h"
#include "xcache_globals.h"
#include "xcache/xc_extension.h"
#include "xcache/xc_ini.h"
#include "xcache/xc_utils.h"
#include "xcache/xc_sandbox.h"
#include "util/xc_trace.h"
#include "util/xc_vector.h"
#include "util/xc_align.h"

#include "php.h"
#include "ext/standard/info.h"
#include "ext/standard/md5.h"
#ifdef ZEND_ENGINE_2_1
#	include "ext/date/php_date.h"
#endif
#ifdef ZEND_WIN32
#	include <process.h>
#endif
#include "ext/standard/php_math.h"
#include "SAPI.h"

#define ECALLOC_N(x, n) ((x) = ecalloc(n, sizeof((x)[0])))
#define ECALLOC_ONE(x) ECALLOC_N(x, 1)
#define VAR_ENTRY_EXPIRED(pentry) ((pentry)->ttl && XG(request_time) > (pentry)->ctime + (time_t) (pentry)->ttl)
#define CHECK(x, e) do { if ((x) == NULL) { zend_error(E_ERROR, "XCache: " e); goto err; } } while (0)
#define LOCK(x) xc_mutex_lock((x)->mutex)
#define UNLOCK(x) xc_mutex_unlock((x)->mutex)

#define ENTER_LOCK_EX(x) \
	LOCK((x)); \
	zend_try { \
		do
#define LEAVE_LOCK_EX(x) \
		while (0); \
	} zend_catch { \
		catched = 1; \
	} zend_end_try(); \
	UNLOCK((x))

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

struct _xc_hash_t { /* {{{ */
	size_t bits;
	size_t size;
	xc_hash_value_t mask;
};
/* }}} */
struct _xc_cached_t { /* {{{ stored in shm */
	int cacheid;

	time_t     compiling;
	time_t     disabled;
	zend_ulong updates;
	zend_ulong hits;
	zend_ulong skips;
	zend_ulong ooms;
	zend_ulong errors;

	xc_entry_t **entries;
	int entries_count;
	xc_entry_data_php_t **phps;
	int phps_count;
	xc_entry_t *deletes;
	int deletes_count;

	time_t     last_gc_deletes;
	time_t     last_gc_expires;

	time_t     hits_by_hour_cur_time;
	zend_uint  hits_by_hour_cur_slot;
	zend_ulong hits_by_hour[24];
	time_t     hits_by_second_cur_time;
	zend_uint  hits_by_second_cur_slot;
	zend_ulong hits_by_second[5];
};
/* }}} */
typedef struct { /* {{{ xc_cache_t: only cache info, not in shm */
	int cacheid;
	xc_hash_t  *hcache; /* hash to cacheid */

	xc_mutex_t *mutex;
	xc_shm_t   *shm; /* which shm contains us */
	xc_allocator_t *allocator;

	xc_hash_t  *hentry; /* hash settings to entry */
	xc_hash_t  *hphp;   /* hash settings to php */
	xc_cached_t *cached;
} xc_cache_t;
/* }}} */

/* {{{ globals */
static char *xc_shm_scheme = NULL;
static char *xc_mmap_path = NULL;

static zend_bool xc_admin_enable_auth = 1;
static xc_hash_t xc_php_hcache = { 0, 0, 0 };
static xc_hash_t xc_php_hentry = { 0, 0, 0 };
static xc_hash_t xc_var_hcache = { 0, 0, 0 };
static xc_hash_t xc_var_hentry = { 0, 0, 0 };

static zend_ulong xc_php_ttl    = 0;
static zend_ulong xc_var_maxttl = 0;

enum { xc_deletes_gc_interval = 120 };
static zend_ulong xc_php_gc_interval = 0;
static zend_ulong xc_var_gc_interval = 0;

static char *xc_php_allocator = NULL;
static char *xc_var_allocator = NULL;

/* total size */
static zend_ulong xc_php_size  = 0;
static zend_ulong xc_var_size  = 0;

static xc_cache_t *xc_php_caches = NULL;
static xc_cache_t *xc_var_caches = NULL;

static zend_bool xc_initized = 0;
static time_t xc_init_time = 0;
static long unsigned xc_init_instance_id = 0;
#ifdef ZTS
static long unsigned xc_init_instance_subid = 0;
#endif
static zend_compile_file_t *old_compile_file = NULL;

static zend_bool xc_readonly_protection = 0;

static zend_ulong xc_var_namespace_mode = 0;
static char *xc_var_namespace = NULL;


zend_bool xc_have_op_array_ctor = 0;
/* }}} */

typedef enum { XC_TYPE_PHP, XC_TYPE_VAR } xc_entry_type_t;

static void xc_holds_init(TSRMLS_D);
static void xc_holds_destroy(TSRMLS_D);

static void *xc_cache_storage(void *data, size_t size) /* {{{ */
{
	xc_allocator_t *allocator = (xc_allocator_t *) data;
	return allocator->vtable->malloc(allocator, size);
}
/* }}} */

/* any function in *_unlocked is only safe be called within locked (single thread access) area */

static void xc_php_add_unlocked(xc_cached_t *cached, xc_entry_data_php_t *php) /* {{{ */
{
	xc_entry_data_php_t **head = &(cached->phps[php->hvalue]);
	php->next = *head;
	*head = php;
	cached->phps_count ++;
}
/* }}} */
static xc_entry_data_php_t *xc_php_store_unlocked(xc_cache_t *cache, xc_entry_data_php_t *php TSRMLS_DC) /* {{{ */
{
	xc_entry_data_php_t *stored_php;
	xc_processor_storage_t storage;
	storage.allocator      = &xc_cache_storage;
	storage.allocator_data = (void *) cache->allocator;
	storage.relocatediff   = cache->shm->readonlydiff;

	php->hits     = 0;
	php->refcount = 0;
	stored_php = xc_processor_store_xc_entry_data_php_t(&storage, php TSRMLS_CC);
#if 0
	{
		xc_entry_data_php_t *p = malloc(stored_php->size);
		xc_entry_data_php_t *backup = malloc(stored_php->size);
		fprintf(stderr, "%lu\n", stored_php->size);
		memcpy(backup, stored_php, stored_php->size);

		{
			memcpy(p, stored_php, stored_php->size);
			xc_processor_relocate_xc_entry_data_php_t(p, p , stored_php, stored_php TSRMLS_CC);
			assert(memcmp(stored_php, backup, stored_php->size) == 0);

			memcpy(stored_php, p, p->size);
			xc_processor_relocate_xc_entry_data_php_t(stored_php, stored_php, p, p TSRMLS_CC);
		}

		{
			memcpy(p, stored_php, stored_php->size);
			xc_processor_relocate_xc_entry_data_php_t(p, 0, stored_php, stored_php TSRMLS_CC);
			assert(memcmp(stored_php, backup, stored_php->size) == 0);

			memcpy(stored_php, p, p->size);
			xc_processor_relocate_xc_entry_data_php_t(stored_php, stored_php, p, 0 TSRMLS_CC);
		}
	}
#endif
	if (stored_php) {
		xc_php_add_unlocked(cache->cached, stored_php);
		return stored_php;
	}
	else {
		cache->cached->ooms ++;
		return NULL;
	}
}
/* }}} */
static xc_entry_data_php_t *xc_php_find_unlocked(xc_cached_t *cached, xc_entry_data_php_t *php TSRMLS_DC) /* {{{ */
{
	xc_entry_data_php_t *p;
	for (p = cached->phps[php->hvalue]; p; p = (xc_entry_data_php_t *) p->next) {
		if (memcmp(&php->md5.digest, &p->md5.digest, sizeof(php->md5.digest)) == 0) {
			p->hits ++;
			return p;
		}
	}
	return NULL;
}
/* }}} */
static void xc_php_free_unlocked(xc_cache_t *cache, xc_entry_data_php_t *php) /* {{{ */
{
	cache->allocator->vtable->free(cache->allocator, (xc_entry_data_php_t *)php);
}
/* }}} */
static void xc_php_addref_unlocked(xc_entry_data_php_t *php) /* {{{ */
{
	php->refcount ++;
}
/* }}} */
static void xc_php_release_unlocked(xc_cache_t *cache, xc_entry_data_php_t *php) /* {{{ */
{
	if (-- php->refcount == 0) {
		xc_entry_data_php_t **pp = &(cache->cached->phps[php->hvalue]);
		xc_entry_data_php_t *p;
		for (p = *pp; p; pp = &(p->next), p = p->next) {
			if (memcmp(&php->md5.digest, &p->md5.digest, sizeof(php->md5.digest)) == 0) {
				/* unlink */
				*pp = p->next;
				xc_php_free_unlocked(cache, php);
				return;
			}
		}
		assert(0);
	}
}
/* }}} */

static inline zend_bool xc_entry_equal_unlocked(xc_entry_type_t type, const xc_entry_t *entry1, const xc_entry_t *entry2 TSRMLS_DC) /* {{{ */
{
	/* this function isn't required but can be in unlocked */
	switch (type) {
		case XC_TYPE_PHP:
			{
				const xc_entry_php_t *php_entry1 = (const xc_entry_php_t *) entry1;
				const xc_entry_php_t *php_entry2 = (const xc_entry_php_t *) entry2;
				if (php_entry1->file_inode && php_entry2->file_inode) {
					zend_bool inodeIsSame = php_entry1->file_inode == php_entry2->file_inode
						                 && php_entry1->file_device == php_entry2->file_device;
					if (!inodeIsSame) {
						return 0;
					}
				}
			}

			assert(strstr(entry1->name.str.val, "://") != NULL || IS_ABSOLUTE_PATH(entry1->name.str.val, entry1->name.str.len));
			assert(strstr(entry1->name.str.val, "://") != NULL || IS_ABSOLUTE_PATH(entry2->name.str.val, entry2->name.str.len));

			return entry1->name.str.len == entry2->name.str.len
				&& memcmp(entry1->name.str.val, entry2->name.str.val, entry1->name.str.len + 1) == 0;

		case XC_TYPE_VAR:
#ifdef IS_UNICODE
			if (entry1->name_type != entry2->name_type) {
				return 0;
			}

			if (entry1->name_type == IS_UNICODE) {
				return entry1->name.ustr.len == entry2->name.ustr.len
				    && memcmp(entry1->name.ustr.val, entry2->name.ustr.val, (entry1->name.ustr.len + 1) * sizeof(entry1->name.ustr.val[0])) == 0;
			}
#endif
			return entry1->name.str.len == entry2->name.str.len
			    && memcmp(entry1->name.str.val, entry2->name.str.val, entry1->name.str.len + 1) == 0;
			break;

		default:
			assert(0);
	}
	return 0;
}
/* }}} */
static void xc_entry_add_unlocked(xc_cached_t *cached, xc_hash_value_t entryslotid, xc_entry_t *entry) /* {{{ */
{
	xc_entry_t **head = &(cached->entries[entryslotid]);
	entry->next = *head;
	*head = entry;
	cached->entries_count ++;
}
/* }}} */
static xc_entry_t *xc_entry_store_unlocked(xc_entry_type_t type, xc_cache_t *cache, xc_hash_value_t entryslotid, xc_entry_t *entry TSRMLS_DC) /* {{{ */
{
	xc_entry_t *stored_entry;
	xc_processor_storage_t storage;
	storage.allocator      = &xc_cache_storage;
	storage.allocator_data = (void *) cache->allocator;
	storage.relocatediff   = cache->shm->readonlydiff;

	entry->hits  = 0;
	entry->ctime = XG(request_time);
	entry->atime = XG(request_time);
	stored_entry = type == XC_TYPE_PHP
		? (xc_entry_t *) xc_processor_store_xc_entry_php_t(&storage, (xc_entry_php_t *) entry TSRMLS_CC)
		: (xc_entry_t *) xc_processor_store_xc_entry_var_t(&storage, (xc_entry_var_t *) entry TSRMLS_CC);
	if (stored_entry) {
		xc_entry_add_unlocked(cache->cached, entryslotid, stored_entry);
		++cache->cached->updates;
		return stored_entry;
	}
	else {
		cache->cached->ooms ++;
		return NULL;
	}
}
/* }}} */
static xc_entry_php_t *xc_entry_php_store_unlocked(xc_cache_t *cache, xc_hash_value_t entryslotid, xc_entry_php_t *entry_php TSRMLS_DC) /* {{{ */
{
	return (xc_entry_php_t *) xc_entry_store_unlocked(XC_TYPE_PHP, cache, entryslotid, (xc_entry_t *) entry_php TSRMLS_CC);
}
/* }}} */
static xc_entry_var_t *xc_entry_var_store_unlocked(xc_cache_t *cache, xc_hash_value_t entryslotid, xc_entry_var_t *entry_var TSRMLS_DC) /* {{{ */
{
	return (xc_entry_var_t *) xc_entry_store_unlocked(XC_TYPE_VAR, cache, entryslotid, (xc_entry_t *) entry_var TSRMLS_CC);
}
/* }}} */
static void xc_entry_free_real_unlocked(xc_entry_type_t type, xc_cache_t *cache, volatile xc_entry_t *entry) /* {{{ */
{
	if (type == XC_TYPE_PHP) {
		xc_php_release_unlocked(cache, ((xc_entry_php_t *) entry)->php);
	}
	cache->allocator->vtable->free(cache->allocator, (xc_entry_t *)entry);
}
/* }}} */
static void xc_entry_free_unlocked(xc_entry_type_t type, xc_cache_t *cache, xc_entry_t *entry TSRMLS_DC) /* {{{ */
{
	cache->cached->entries_count --;
	if ((type == XC_TYPE_PHP ? ((xc_entry_php_t *) entry)->refcount : 0) == 0) {
		xc_entry_free_real_unlocked(type, cache, entry);
	}
	else {
		entry->next = cache->cached->deletes;
		cache->cached->deletes = entry;
		entry->dtime = XG(request_time);
		cache->cached->deletes_count ++;
	}
	return;
}
/* }}} */
static void xc_entry_remove_unlocked(xc_entry_type_t type, xc_cache_t *cache, xc_hash_value_t entryslotid, xc_entry_t *entry TSRMLS_DC) /* {{{ */
{
	xc_entry_t **pp = &(cache->cached->entries[entryslotid]);
	xc_entry_t *p;
	for (p = *pp; p; pp = &(p->next), p = p->next) {
		if (xc_entry_equal_unlocked(type, entry, p TSRMLS_CC)) {
			/* unlink */
			*pp = p->next;
			xc_entry_free_unlocked(type, cache, entry TSRMLS_CC);
			return;
		}
	}
	assert(0);
}
/* }}} */
static xc_entry_t *xc_entry_find_unlocked(xc_entry_type_t type, xc_cache_t *cache, xc_hash_value_t entryslotid, xc_entry_t *entry TSRMLS_DC) /* {{{ */
{
	xc_entry_t *p;
	for (p = cache->cached->entries[entryslotid]; p; p = p->next) {
		if (xc_entry_equal_unlocked(type, entry, p TSRMLS_CC)) {
			zend_bool fresh;
			switch (type) {
			case XC_TYPE_PHP:
				{
					xc_entry_php_t *p_php = (xc_entry_php_t *) p;
					xc_entry_php_t *entry_php = (xc_entry_php_t *) entry;
					fresh = p_php->file_mtime == entry_php->file_mtime && p_php->file_size == entry_php->file_size;
				}
				break;

			case XC_TYPE_VAR:
				{
					fresh = !VAR_ENTRY_EXPIRED(p);
				}
				break;

			default:
				assert(0);
			}

			if (fresh) {
				p->hits ++;
				p->atime = XG(request_time);
				return p;
			}

			xc_entry_remove_unlocked(type, cache, entryslotid, p TSRMLS_CC);
			return NULL;
		}
	}
	return NULL;
}
/* }}} */
static void xc_entry_hold_php_unlocked(xc_cache_t *cache, xc_entry_php_t *entry TSRMLS_DC) /* {{{ */
{
	TRACE("hold %lu:%s", (unsigned long) entry->file_inode, entry->entry.name.str.val);
#ifndef ZEND_WIN32
	if (XG(holds_pid) != getpid()) {
		xc_holds_destroy(TSRMLS_C);
		xc_holds_init(TSRMLS_C);
	}
#endif
	entry->refcount ++;
	xc_vector_push_back(&XG(php_holds)[cache->cacheid], &entry);
}
/* }}} */
static inline zend_uint advance_wrapped(zend_uint val, zend_uint count) /* {{{ */
{
	if (val + 1 >= count) {
		return 0;
	}
	return val + 1;
}
/* }}} */
static inline void xc_counters_inc(time_t *curtime, zend_uint *curslot, time_t interval, zend_ulong *counters, zend_uint ncounters TSRMLS_DC) /* {{{ */
{
	time_t n = XG(request_time) / interval;
	if (*curtime < n) {
		zend_uint target_slot = ((zend_uint) n) % ncounters;
		zend_uint slot;
		for (slot = advance_wrapped(*curslot, ncounters);
				slot != target_slot;
				slot = advance_wrapped(slot, ncounters)) {
			counters[slot] = 0;
		}
		counters[target_slot] = 0;
		*curtime = n;
		*curslot = target_slot;
	}
	counters[*curslot] ++;
}
/* }}} */
#define xc_countof(array) (sizeof(array) / sizeof(array[0]))
static inline void xc_cached_hit_unlocked(xc_cached_t *cached TSRMLS_DC) /* {{{ */
{
	cached->hits ++;

	xc_counters_inc(&cached->hits_by_hour_cur_time
			, &cached->hits_by_hour_cur_slot, 60 * 60
			, cached->hits_by_hour
			, xc_countof(cached->hits_by_hour)
			TSRMLS_CC);

	xc_counters_inc(&cached->hits_by_second_cur_time
			, &cached->hits_by_second_cur_slot, 1
			, cached->hits_by_second
			, xc_countof(cached->hits_by_second)
			TSRMLS_CC);
}
/* }}} */

/* helper function that loop through each entry */
#define XC_ENTRY_APPLY_FUNC(name) zend_bool name(xc_entry_t *entry TSRMLS_DC)
typedef XC_ENTRY_APPLY_FUNC((*cache_apply_unlocked_func_t));
static void xc_entry_apply_unlocked(xc_entry_type_t type, xc_cache_t *cache, cache_apply_unlocked_func_t apply_func TSRMLS_DC) /* {{{ */
{
	xc_entry_t *p, **pp;
	size_t i, c;

	for (i = 0, c = cache->hentry->size; i < c; i ++) {
		pp = &(cache->cached->entries[i]);
		for (p = *pp; p; p = *pp) {
			if (apply_func(p TSRMLS_CC)) {
				/* unlink */
				*pp = p->next;
				xc_entry_free_unlocked(type, cache, p TSRMLS_CC);
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
 * xc_gc_expires_php -> xc_gc_expires_one -> xc_entry_apply_unlocked -> xc_gc_expires_php_entry_unlocked
 * xc_gc_expires_var -> xc_gc_expires_one -> xc_entry_apply_unlocked -> xc_gc_expires_var_entry_unlocked
 */
static XC_ENTRY_APPLY_FUNC(xc_gc_expires_php_entry_unlocked) /* {{{ */
{
	TRACE("ttl %lu, %lu %lu", (unsigned long) XG(request_time), (unsigned long) entry->atime, xc_php_ttl);
	if (XG(request_time) > entry->atime + (time_t) xc_php_ttl) {
		return 1;
	}
	return 0;
}
/* }}} */
static XC_ENTRY_APPLY_FUNC(xc_gc_expires_var_entry_unlocked) /* {{{ */
{
	if (VAR_ENTRY_EXPIRED(entry)) {
		return 1;
	}
	return 0;
}
/* }}} */
static void xc_gc_expires_one(xc_entry_type_t type, xc_cache_t *cache, zend_ulong gc_interval, cache_apply_unlocked_func_t apply_func TSRMLS_DC) /* {{{ */
{
	TRACE("interval %lu, %lu %lu", (zend_ulong) XG(request_time), (zend_ulong) cache->cached->last_gc_expires, gc_interval);
	if (!cache->cached->disabled && XG(request_time) >= cache->cached->last_gc_expires + (time_t) gc_interval) {
		ENTER_LOCK(cache) {
			if (XG(request_time) >= cache->cached->last_gc_expires + (time_t) gc_interval) {
				cache->cached->last_gc_expires = XG(request_time);
				xc_entry_apply_unlocked(type, cache, apply_func TSRMLS_CC);
			}
		} LEAVE_LOCK(cache);
	}
}
/* }}} */
static void xc_gc_expires_php(TSRMLS_D) /* {{{ */
{
	size_t i, c;

	if (!xc_php_ttl || !xc_php_gc_interval || !xc_php_caches) {
		return;
	}

	for (i = 0, c = xc_php_hcache.size; i < c; i ++) {
		xc_gc_expires_one(XC_TYPE_PHP, &xc_php_caches[i], xc_php_gc_interval, xc_gc_expires_php_entry_unlocked TSRMLS_CC);
	}
}
/* }}} */
static void xc_gc_expires_var(TSRMLS_D) /* {{{ */
{
	size_t i, c;

	if (!xc_var_gc_interval || !xc_var_caches) {
		return;
	}

	for (i = 0, c = xc_var_hcache.size; i < c; i ++) {
		xc_gc_expires_one(XC_TYPE_VAR, &xc_var_caches[i], xc_var_gc_interval, xc_gc_expires_var_entry_unlocked TSRMLS_CC);
	}
}
/* }}} */

static XC_CACHE_APPLY_FUNC(xc_gc_delete_unlocked) /* {{{ */
{
	xc_entry_t *p, **pp;

	pp = &cache->cached->deletes;
	for (p = *pp; p; p = *pp) {
		xc_entry_php_t *entry = (xc_entry_php_t *) p;
		if (XG(request_time) - p->dtime > 3600) {
			entry->refcount = 0;
			/* issue warning here */
		}
		if (entry->refcount == 0) {
			/* unlink */
			*pp = p->next;
			cache->cached->deletes_count --;
			xc_entry_free_real_unlocked(XC_TYPE_PHP, cache, p);
		}
		else {
			pp = &(p->next);
		}
	}
}
/* }}} */
static XC_CACHE_APPLY_FUNC(xc_gc_deletes_one) /* {{{ */
{
	if (!cache->cached->disabled && cache->cached->deletes && XG(request_time) - cache->cached->last_gc_deletes > xc_deletes_gc_interval) {
		ENTER_LOCK(cache) {
			if (cache->cached->deletes && XG(request_time) - cache->cached->last_gc_deletes > xc_deletes_gc_interval) {
				cache->cached->last_gc_deletes = XG(request_time);
				xc_gc_delete_unlocked(cache TSRMLS_CC);
			}
		} LEAVE_LOCK(cache);
	}
}
/* }}} */
static void xc_gc_deletes(TSRMLS_D) /* {{{ */
{
	size_t i, c;

	if (xc_php_caches) {
		for (i = 0, c = xc_php_hcache.size; i < c; i ++) {
			xc_gc_deletes_one(&xc_php_caches[i] TSRMLS_CC);
		}
	}

	if (xc_var_caches) {
		for (i = 0, c = xc_var_hcache.size; i < c; i ++) {
			xc_gc_deletes_one(&xc_var_caches[i] TSRMLS_CC);
		}
	}
}
/* }}} */

/* helper functions for user functions */
static void xc_fillinfo_unlocked(int cachetype, xc_cache_t *cache, zval *return_value TSRMLS_DC) /* {{{ */
{
	zval *blocks, *hits;
	size_t i;
	const xc_allocator_block_t *b;
#ifndef NDEBUG
	xc_memsize_t avail = 0;
#endif
	const xc_allocator_t *allocator = cache->allocator;
	const xc_allocator_vtable_t *vtable = allocator->vtable;
	zend_ulong interval;
	const xc_cached_t *cached = cache->cached;

	if (cachetype == XC_TYPE_PHP) {
		interval = xc_php_ttl ? xc_php_gc_interval : 0;
	}
	else {
		interval = xc_var_gc_interval;
	}

	add_assoc_long_ex(return_value, XCACHE_STRS("slots"),     cache->hentry->size);
	add_assoc_long_ex(return_value, XCACHE_STRS("compiling"), cached->compiling);
	add_assoc_long_ex(return_value, XCACHE_STRS("disabled"),  cached->disabled);
	add_assoc_long_ex(return_value, XCACHE_STRS("updates"),   cached->updates);
	add_assoc_long_ex(return_value, XCACHE_STRS("misses"),    cached->updates); /* deprecated */
	add_assoc_long_ex(return_value, XCACHE_STRS("hits"),      cached->hits);
	add_assoc_long_ex(return_value, XCACHE_STRS("skips"),     cached->skips);
	add_assoc_long_ex(return_value, XCACHE_STRS("clogs"),     cached->skips); /* deprecated */
	add_assoc_long_ex(return_value, XCACHE_STRS("ooms"),      cached->ooms);
	add_assoc_long_ex(return_value, XCACHE_STRS("errors"),    cached->errors);

	add_assoc_long_ex(return_value, XCACHE_STRS("cached"),    cached->entries_count);
	add_assoc_long_ex(return_value, XCACHE_STRS("deleted"),   cached->deletes_count);
	if (interval) {
		time_t gc = (cached->last_gc_expires + interval) - XG(request_time);
		add_assoc_long_ex(return_value, XCACHE_STRS("gc"),    gc > 0 ? gc : 0);
	}
	else {
		add_assoc_null_ex(return_value, XCACHE_STRS("gc"));
	}
	MAKE_STD_ZVAL(hits);
	array_init(hits);
	for (i = 0; i < xc_countof(cached->hits_by_hour); i ++) {
		add_next_index_long(hits, (long) cached->hits_by_hour[i]);
	}
	add_assoc_zval_ex(return_value, XCACHE_STRS("hits_by_hour"), hits);

	MAKE_STD_ZVAL(hits);
	array_init(hits);
	for (i = 0; i < xc_countof(cached->hits_by_second); i ++) {
		add_next_index_long(hits, (long) cached->hits_by_second[i]);
	}
	add_assoc_zval_ex(return_value, XCACHE_STRS("hits_by_second"), hits);

	MAKE_STD_ZVAL(blocks);
	array_init(blocks);

	add_assoc_long_ex(return_value, XCACHE_STRS("size"),  vtable->size(allocator));
	add_assoc_long_ex(return_value, XCACHE_STRS("avail"), vtable->avail(allocator));
	add_assoc_bool_ex(return_value, XCACHE_STRS("can_readonly"), xc_readonly_protection);

	for (b = vtable->freeblock_first(allocator); b; b = vtable->freeblock_next(b)) {
		zval *bi;

		MAKE_STD_ZVAL(bi);
		array_init(bi);

		add_assoc_long_ex(bi, XCACHE_STRS("size"),   vtable->block_size(b));
		add_assoc_long_ex(bi, XCACHE_STRS("offset"), vtable->block_offset(allocator, b));
		add_next_index_zval(blocks, bi);
#ifndef NDEBUG
		avail += vtable->block_size(b);
#endif
	}
	add_assoc_zval_ex(return_value, XCACHE_STRS("free_blocks"), blocks);
#ifndef NDEBUG
	assert(avail == vtable->avail(allocator));
#endif
}
/* }}} */
static void xc_fillentry_unlocked(xc_entry_type_t type, const xc_entry_t *entry, xc_hash_value_t entryslotid, int del, zval *list TSRMLS_DC) /* {{{ */
{
	zval* ei;
	const xc_entry_data_php_t *php;

	ALLOC_INIT_ZVAL(ei);
	array_init(ei);

	add_assoc_long_ex(ei, XCACHE_STRS("hits"),     entry->hits);
	add_assoc_long_ex(ei, XCACHE_STRS("ctime"),    entry->ctime);
	add_assoc_long_ex(ei, XCACHE_STRS("atime"),    entry->atime);
	add_assoc_long_ex(ei, XCACHE_STRS("hvalue"),   entryslotid);
	if (del) {
		add_assoc_long_ex(ei, XCACHE_STRS("dtime"), entry->dtime);
	}
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
		add_assoc_zval_ex(ei, XCACHE_STRS("name"), zv);
	} while (0);
#else
	add_assoc_stringl_ex(ei, XCACHE_STRS("name"), entry->name.str.val, entry->name.str.len, 1);
#endif
	switch (type) {
		case XC_TYPE_PHP: {
			xc_entry_php_t *entry_php = (xc_entry_php_t *) entry;
			php = entry_php->php;
			add_assoc_long_ex(ei, XCACHE_STRS("size"),          entry->size + php->size);
			add_assoc_long_ex(ei, XCACHE_STRS("refcount"),      entry_php->refcount);
			add_assoc_long_ex(ei, XCACHE_STRS("phprefcount"),   php->refcount);
			add_assoc_long_ex(ei, XCACHE_STRS("file_mtime"),    entry_php->file_mtime);
			add_assoc_long_ex(ei, XCACHE_STRS("file_size"),     entry_php->file_size);
			add_assoc_long_ex(ei, XCACHE_STRS("file_device"),   entry_php->file_device);
			add_assoc_long_ex(ei, XCACHE_STRS("file_inode"),    entry_php->file_inode);

#ifdef HAVE_XCACHE_CONSTANT
			add_assoc_long_ex(ei, XCACHE_STRS("constinfo_cnt"), php->constinfo_cnt);
#endif
			add_assoc_long_ex(ei, XCACHE_STRS("function_cnt"),  php->funcinfo_cnt);
			add_assoc_long_ex(ei, XCACHE_STRS("class_cnt"),     php->classinfo_cnt);
#ifdef ZEND_ENGINE_2_1
			add_assoc_long_ex(ei, XCACHE_STRS("autoglobal_cnt"),php->autoglobal_cnt);
#endif
			break;
		}

		case XC_TYPE_VAR:
			add_assoc_long_ex(ei, XCACHE_STRS("refcount"),      0); /* for BC only */
			add_assoc_long_ex(ei, XCACHE_STRS("size"),          entry->size);
			break;

		default:
			assert(0);
	}

	add_next_index_zval(list, ei);
}
/* }}} */
static void xc_filllist_unlocked(xc_entry_type_t type, xc_cache_t *cache, zval *return_value TSRMLS_DC) /* {{{ */
{
	zval* list;
	size_t i, c;
	xc_entry_t *e;

	ALLOC_INIT_ZVAL(list);
	array_init(list);

	for (i = 0, c = cache->hentry->size; i < c; i ++) {
		for (e = cache->cached->entries[i]; e; e = e->next) {
			xc_fillentry_unlocked(type, e, i, 0, list TSRMLS_CC);
		}
	}
	add_assoc_zval(return_value, "cache_list", list);

	ALLOC_INIT_ZVAL(list);
	array_init(list);
	for (e = cache->cached->deletes; e; e = e->next) {
		xc_fillentry_unlocked(XC_TYPE_PHP, e, 0, 1, list TSRMLS_CC);
	}
	add_assoc_zval(return_value, "deleted_list", list);
}
/* }}} */

static zend_op_array *xc_entry_install(xc_entry_php_t *entry_php TSRMLS_DC) /* {{{ */
{
	zend_uint i;
	xc_entry_data_php_t *p = entry_php->php;
	zend_op_array *old_active_op_array = CG(active_op_array);
#ifndef ZEND_ENGINE_2
	ALLOCA_FLAG(use_heap)
	/* new ptr which is stored inside CG(class_table) */
	xc_cest_t **new_cest_ptrs = (xc_cest_t **)xc_do_alloca(sizeof(xc_cest_t*) * p->classinfo_cnt, use_heap);
#endif

	CG(active_op_array) = p->op_array;

#ifdef HAVE_XCACHE_CONSTANT
	/* install constant */
	for (i = 0; i < p->constinfo_cnt; i ++) {
		xc_constinfo_t *ci = &p->constinfos[i];
		xc_install_constant(entry_php->entry.name.str.val, &ci->constant,
				UNISW(0, ci->type), ci->key, ci->key_size, ci->h TSRMLS_CC);
	}
#endif

	/* install function */
	for (i = 0; i < p->funcinfo_cnt; i ++) {
		xc_funcinfo_t  *fi = &p->funcinfos[i];
		xc_install_function(entry_php->entry.name.str.val, &fi->func,
				UNISW(0, fi->type), fi->key, fi->key_size, fi->h TSRMLS_CC);
	}

	/* install class */
	for (i = 0; i < p->classinfo_cnt; i ++) {
		xc_classinfo_t *ci = &p->classinfos[i];
#ifndef ZEND_ENGINE_2
		zend_class_entry *ce = CestToCePtr(ci->cest);
		/* fix pointer to the be which inside class_table */
		if (ce->parent) {
			zend_uint class_idx = (/* class_num */ (int) (long) ce->parent) - 1;
			assert(class_idx < i);
			ci->cest.parent = new_cest_ptrs[class_idx];
		}
		new_cest_ptrs[i] =
#endif
#ifdef ZEND_COMPILE_DELAYED_BINDING
		xc_install_class(entry_php->entry.name.str.val, &ci->cest, -1,
				UNISW(0, ci->type), ci->key, ci->key_size, ci->h TSRMLS_CC);
#else
		xc_install_class(entry_php->entry.name.str.val, &ci->cest, ci->oplineno,
				UNISW(0, ci->type), ci->key, ci->key_size, ci->h TSRMLS_CC);
#endif
	}

#ifdef ZEND_ENGINE_2_1
	/* trigger auto_globals jit */
	for (i = 0; i < p->autoglobal_cnt; i ++) {
		xc_autoglobal_t *aginfo = &p->autoglobals[i];
		zend_u_is_auto_global(aginfo->type, aginfo->key, aginfo->key_len TSRMLS_CC);
	}
#endif
#ifdef XCACHE_ERROR_CACHING
	/* restore trigger errors */
	for (i = 0; i < p->compilererror_cnt; i ++) {
		xc_compilererror_t *error = &p->compilererrors[i];
		CG(zend_lineno) = error->lineno;
		zend_error(error->type, "%s", error->error);
	}
	CG(zend_lineno) = 0;
#endif

#ifndef ZEND_ENGINE_2
	xc_free_alloca(new_cest_ptrs, use_heap);
#endif
	CG(active_op_array) = old_active_op_array;
	return p->op_array;
}
/* }}} */

static inline void xc_entry_unholds_real(xc_vector_t *holds, xc_cache_t *caches, size_t cachecount TSRMLS_DC) /* {{{ */
{
	size_t i;
	xc_vector_t *v;
	xc_cache_t *cache;
	xc_entry_php_t *entry_php;

	for (i = 0; i < cachecount; i ++) {
		v = &holds[i];
		TRACE("holded %d items", v->cnt);
		if (xc_vector_size(v)) {
			cache = &caches[i];
			ENTER_LOCK(cache) {
				while (xc_vector_size(v)) {
					entry_php = xc_vector_pop_back(xc_entry_php_t *, v);
					TRACE("unhold %lu:%s", (unsigned long) entry_php->file_inode, entry_php->entry.name.str.val);
					assert(entry_php->refcount > 0);
					--entry_php->refcount;
				}
			} LEAVE_LOCK(cache);
		}
	}
}
/* }}} */
static void xc_entry_unholds(TSRMLS_D) /* {{{ */
{
	if (xc_php_caches) {
		xc_entry_unholds_real(XG(php_holds), xc_php_caches, xc_php_hcache.size TSRMLS_CC);
	}

	if (xc_var_caches) {
		xc_entry_unholds_real(XG(var_holds), xc_var_caches, xc_var_hcache.size TSRMLS_CC);
	}
}
/* }}} */

#define HASH(i) (i)
#define HASH_ZSTR_L(t, s, l) HASH(zend_u_inline_hash_func((t), (s), ((l) + 1) * sizeof(UChar)))
#define HASH_STR_S(s, l) HASH(zend_inline_hash_func((char *) (s), (l)))
#define HASH_STR_L(s, l) HASH_STR_S((s), (l) + 1)
#define HASH_STR(s) HASH_STR_L((s), strlen((s)) + 1)
#define HASH_NUM(n) HASH(n)
static inline xc_hash_value_t xc_hash_fold(xc_hash_value_t hvalue, const xc_hash_t *hasher) /* {{{ fold hash bits as needed */
{
	xc_hash_value_t folded = 0;
	while (hvalue) {
		folded ^= (hvalue & hasher->mask);
		hvalue >>= hasher->bits;
	}
	return folded;
}
/* }}} */
static inline xc_hash_value_t xc_entry_hash_name(xc_entry_t *entry TSRMLS_DC) /* {{{ */
{
	return UNISW(NOTHING, UG(unicode) ? HASH_ZSTR_L(entry->name_type, entry->name.uni.val, entry->name.uni.len) :)
		HASH_STR_L(entry->name.str.val, entry->name.str.len);
}
/* }}} */
#define xc_entry_hash_var xc_entry_hash_name
static void xc_entry_free_key_php(xc_entry_php_t *entry_php TSRMLS_DC) /* {{{ */
{
#define X_FREE(var) do {\
	if (entry_php->var) { \
		efree(entry_php->var); \
	} \
} while (0)
#ifdef ZEND_ENGINE_2_3
	X_FREE(dirpath.str);
#endif
#ifdef IS_UNICODE
	X_FREE(u_filepath.str);
#	ifdef ZEND_ENGINE_2_3
	X_FREE(u_dirpath.str);
#	endif
#endif

#undef X_FREE
}
/* }}} */
static char *xc_expand_url(const char *filepath, char *real_path TSRMLS_DC) /* {{{ */
{
	if (strstr(filepath, "://") != NULL) {
		size_t filepath_len = strlen(filepath);
		size_t copy_len = filepath_len > MAXPATHLEN - 1 ? MAXPATHLEN - 1 : filepath_len;
		memcpy(real_path, filepath, filepath_len);
		real_path[copy_len] = '\0';
		return real_path;
	}
	return expand_filepath(filepath, real_path TSRMLS_CC);
}
/* }}} */

#define XC_RESOLVE_PATH_CHECKER(name) int name(const char *filepath, size_t filepath_len, void *data TSRMLS_DC)
typedef XC_RESOLVE_PATH_CHECKER((*xc_resolve_path_checker_func_t));
static int xc_resolve_path(const char *filepath, char *path_buffer, xc_resolve_path_checker_func_t checker_func, void *data TSRMLS_DC) /* {{{ */
{
	char *paths, *path;
	char *tokbuf;
	size_t path_buffer_len;
	size_t size;
	char tokens[] = { DEFAULT_DIR_SEPARATOR, '\0' };
	int ret;
	ALLOCA_FLAG(use_heap)

#if 0
	if ((*filepath == '.' && 
	     (IS_SLASH(filepath[1]) || 
	      ((filepath[1] == '.') && IS_SLASH(filepath[2])))) ||
	    IS_ABSOLUTE_PATH(filepath, strlen(filepath)) ||
	    !path ||
	    !*path) {

		ret = checker_func(path_buffer, path_buffer_len, data TSRMLS_CC);
		goto finish;
	}
#endif

	size = strlen(PG(include_path)) + 1;
	paths = (char *)xc_do_alloca(size, use_heap);
	memcpy(paths, PG(include_path), size);

	for (path = php_strtok_r(paths, tokens, &tokbuf); path; path = php_strtok_r(NULL, tokens, &tokbuf)) {
		path_buffer_len = snprintf(path_buffer, MAXPATHLEN, "%s/%s", path, filepath);
		if (path_buffer_len < MAXPATHLEN - 1) {
			ret = checker_func(path_buffer, path_buffer_len, data TSRMLS_CC);
			if (ret == SUCCESS) {
				goto finish;
			}
		}
	}

	/* fall back to current directory */
	if (zend_is_executing(TSRMLS_C)) {
		const char *executing_filename = zend_get_executed_filename(TSRMLS_C);
		int dirname_len = (int) strlen(executing_filename);
		size_t filename_len = strlen(filepath);

		while ((--dirname_len >= 0) && !IS_SLASH(executing_filename[dirname_len]));
		if (executing_filename && dirname_len > 0 && executing_filename[0] && executing_filename[0] != '['
		 && dirname_len + 1 + filename_len + 1 < MAXPATHLEN) {
			memcpy(path_buffer, executing_filename, dirname_len + 1);
			memcpy(path_buffer + dirname_len + 1, filepath, filename_len + 1);
			path_buffer_len = dirname_len + 1 + filename_len;
			ret = checker_func(path_buffer, path_buffer_len, data TSRMLS_CC);
			if (ret == SUCCESS) {
				goto finish;
			}
		}
	}

	ret = FAILURE;

finish:
	xc_free_alloca(paths, use_heap);

	return ret;
}
/* }}} */

static zend_bool xc_is_absolute(const char *filepath, size_t filepath_len) /* {{{ */
{
	const char *p;

	if (IS_ABSOLUTE_PATH(filepath, filepath_len)) {
		return 1;
	}

	for (p = filepath; isalnum((int)*p) || *p == '+' || *p == '-' || *p == '.'; p++);
	if ((*p == ':') && (p - filepath > 1) && (p[1] == '/') && (p[2] == '/')) {
		return 1;
	}

	return 0;
}
/* }}} */
static int xc_stat(const char *filepath, struct stat *statbuf TSRMLS_DC) /* {{{ */
{
	if (strstr(filepath, "://") != NULL) {
		php_stream_statbuf ssb; 
		php_stream_wrapper *wrapper = NULL; 
#ifdef ZEND_ENGINE_2_6
		const
#endif
		char *path_for_open = NULL; 

		wrapper = php_stream_locate_url_wrapper(filepath, &path_for_open, 0 TSRMLS_CC); 
		if (wrapper && wrapper->wops->url_stat
#ifdef ZEND_ENGINE_2
		 && wrapper->wops->url_stat(wrapper, path_for_open, PHP_STREAM_URL_STAT_QUIET, &ssb, NULL TSRMLS_CC) == SUCCESS
#else
		 && wrapper->wops->url_stat(wrapper, path_for_open, &ssb TSRMLS_CC) == SUCCESS
#endif
		) {
			*statbuf = ssb.sb;
			return SUCCESS;
		}

		return FAILURE;
	}

	return VCWD_STAT(filepath, statbuf);
}
/* }}} */
static XC_RESOLVE_PATH_CHECKER(xc_resolve_path_stat_checker) /* {{{ */
{
	return xc_stat(filepath, (struct stat *) data TSRMLS_CC);
}
/* }}} */
#ifndef ZEND_ENGINE_2_3
static int xc_resolve_path_stat(const char *filepath, char *path_buffer, struct stat *pbuf TSRMLS_DC) /* {{{ */
{
	return xc_resolve_path(filepath, path_buffer, xc_resolve_path_stat_checker, (void *) pbuf TSRMLS_CC);
}
/* }}} */
#endif
typedef struct xc_compiler_t { /* {{{ */
	/* XCache cached compile state */
	const char *filename;
	size_t filename_len;
	const char *opened_path;
	char opened_path_buffer[MAXPATHLEN];

	xc_entry_hash_t entry_hash;
	xc_entry_php_t new_entry;
	xc_entry_data_php_t new_php;
} xc_compiler_t;
/* }}} */
typedef struct xc_resolve_path_entry_checker_t { /* {{{ */
	xc_compiler_t *compiler;
	xc_entry_php_t **stored_entry;
} xc_resolve_path_entry_checker_data_t;
/* }}} */
static XC_RESOLVE_PATH_CHECKER(xc_resolve_path_entry_checker) /* {{{ */
{
	xc_resolve_path_entry_checker_data_t *entry_checker_data = (xc_resolve_path_entry_checker_data_t *) data;
	xc_compiler_t *compiler = entry_checker_data->compiler;

	compiler->new_entry.entry.name.str.val = xc_expand_url(filepath, compiler->opened_path_buffer TSRMLS_CC);
	compiler->new_entry.entry.name.str.len = (int) strlen(compiler->new_entry.entry.name.str.val);

	*entry_checker_data->stored_entry = (xc_entry_php_t *) xc_entry_find_unlocked(
			XC_TYPE_PHP
			, &xc_php_caches[compiler->entry_hash.cacheid]
			, compiler->entry_hash.entryslotid
			, (xc_entry_t *) &compiler->new_entry
			TSRMLS_CC);

	return *entry_checker_data->stored_entry ? SUCCESS : FAILURE;
}
/* }}} */
static int xc_resolve_path_check_entry_unlocked(xc_compiler_t *compiler, const char *filepath, xc_entry_php_t **stored_entry TSRMLS_DC) /* {{{ */
{
	char path_buffer[MAXPATHLEN];
	xc_resolve_path_entry_checker_data_t entry_checker_data;
	entry_checker_data.compiler = compiler;
	entry_checker_data.stored_entry = stored_entry;

	return xc_resolve_path(filepath, path_buffer, xc_resolve_path_entry_checker, (void *) &entry_checker_data TSRMLS_CC);
}
/* }}} */
static int xc_entry_php_quick_resolve_opened_path(xc_compiler_t *compiler, struct stat *statbuf TSRMLS_DC) /* {{{ */
{
	if (strcmp(SG(request_info).path_translated, compiler->filename) == 0) {
		/* sapi has already done this stat() for us */
		if (statbuf) {
			struct stat *sapi_stat = sapi_get_stat(TSRMLS_C);
			if (!sapi_stat) {
				goto giveupsapistat;
			}
			*statbuf = *sapi_stat;
		}

		compiler->opened_path = xc_expand_url(compiler->filename, compiler->opened_path_buffer TSRMLS_CC);
		return SUCCESS;
	}
giveupsapistat:

	/* absolute path */
	if (xc_is_absolute(compiler->filename, strlen(compiler->filename))) {
		if (statbuf && xc_stat(compiler->filename, statbuf TSRMLS_CC) != SUCCESS) {
			return FAILURE;
		}
		compiler->opened_path = xc_expand_url(compiler->filename, compiler->opened_path_buffer TSRMLS_CC);
		return SUCCESS;
	}

	/* relative path */
	if (*compiler->filename == '.' && (IS_SLASH(compiler->filename[1]) || compiler->filename[1] == '.')) {
		const char *ptr = compiler->filename + 1;
		if (*ptr == '.') {
			while (*(++ptr) == '.');
			if (!IS_SLASH(*ptr)) {
				return FAILURE;
			}   
		}

		if (statbuf && VCWD_STAT(compiler->filename, statbuf) != 0) {
			return FAILURE;
		}

		compiler->opened_path = xc_expand_url(compiler->filename, compiler->opened_path_buffer TSRMLS_CC);
		return SUCCESS;
	}

	return FAILURE;
}
/* }}} */
static int xc_entry_php_resolve_opened_path(xc_compiler_t *compiler, struct stat *statbuf TSRMLS_DC) /* {{{ */
{
	if (xc_entry_php_quick_resolve_opened_path(compiler, statbuf TSRMLS_CC) == SUCCESS) {
		/* opened_path resolved */
		return SUCCESS;
	}
	/* fall back to real stat call */
	else {
#ifdef ZEND_ENGINE_2_3
		char *opened_path = php_resolve_path(compiler->filename, (int) compiler->filename_len, PG(include_path) TSRMLS_CC);
		if (opened_path) {
			strcpy(compiler->opened_path_buffer, opened_path);
			efree(opened_path);
			compiler->opened_path = compiler->opened_path_buffer;
			if (!statbuf || xc_stat(compiler->opened_path, statbuf TSRMLS_CC) == SUCCESS) {
				return SUCCESS;
			}
		}
#else
		char path_buffer[MAXPATHLEN];
		if (xc_resolve_path_stat(compiler->filename, path_buffer, statbuf TSRMLS_CC) == SUCCESS) {
			compiler->opened_path = xc_expand_url(path_buffer, compiler->opened_path_buffer TSRMLS_CC);
			return SUCCESS;
		}
#endif
	}
	return FAILURE;
}
/* }}} */
static int xc_entry_php_init_key(xc_compiler_t *compiler TSRMLS_DC) /* {{{ */
{
	if (XG(stat)) {
		struct stat buf;
		time_t delta;

		if (compiler->opened_path) {
			if (xc_stat(compiler->opened_path, &buf TSRMLS_CC) != SUCCESS) {
				return FAILURE;
			}
		}
		else {
			if (xc_entry_php_resolve_opened_path(compiler, &buf TSRMLS_CC) != SUCCESS) {
				return FAILURE;
			}
		}

		delta = XG(request_time) - buf.st_mtime;
		if (abs((int) delta) < 2 && !xc_test) {
			return FAILURE;
		}

		compiler->new_entry.file_mtime   = buf.st_mtime;
		compiler->new_entry.file_size    = buf.st_size;
		compiler->new_entry.file_device  = buf.st_dev;
		compiler->new_entry.file_inode   = buf.st_ino;
	}
	else {
		xc_entry_php_quick_resolve_opened_path(compiler, NULL TSRMLS_CC);

		compiler->new_entry.file_mtime   = 0;
		compiler->new_entry.file_size    = 0;
		compiler->new_entry.file_device  = 0;
		compiler->new_entry.file_inode   = 0;
	}

	{
		xc_hash_value_t basename_hash_value;
		if (xc_php_hcache.size > 1
		 || !compiler->new_entry.file_inode) {
			const char *filename_end = compiler->filename + compiler->filename_len;
			const char *basename_begin = filename_end - 1;

			/* scan till out of basename part */
			while (basename_begin >= compiler->filename && !IS_SLASH(*basename_begin)) {
				--basename_begin;
			}
			/* get back to basename_begin */
			++basename_begin;

			basename_hash_value = HASH_STR_L(basename_begin, (uint) (filename_end - basename_begin));
		}

		compiler->entry_hash.cacheid = xc_php_hcache.size > 1 ? xc_hash_fold(basename_hash_value, &xc_php_hcache) : 0;
		compiler->entry_hash.entryslotid = xc_hash_fold(
				compiler->new_entry.file_inode
				? (xc_hash_value_t) HASH(compiler->new_entry.file_device + compiler->new_entry.file_inode)
				: basename_hash_value
				, &xc_php_hentry);
	}

	compiler->new_entry.filepath.str   = NULL;
#ifdef ZEND_ENGINE_2_3
	compiler->new_entry.dirpath.str    = NULL;
#endif
#ifdef IS_UNICODE
	compiler->new_entry.u_filepath.str = NULL;
	compiler->new_entry.u_dirpath.str  = NULL;
#endif

	return SUCCESS;
}
/* }}} */
static inline xc_hash_value_t xc_php_hash_md5(xc_entry_data_php_t *php TSRMLS_DC) /* {{{ */
{
	return HASH_STR_S(php->md5.digest, sizeof(php->md5.digest));
}
/* }}} */
static int xc_entry_data_php_init_md5(xc_cache_t *cache, xc_compiler_t *compiler TSRMLS_DC) /* {{{ */
{
	unsigned char   buf[1024];
	PHP_MD5_CTX     context;
	int             n;
	php_stream     *stream;
	ulong           old_rsid = EG(regular_list).nNextFreeElement;

	stream = php_stream_open_wrapper((char *) compiler->filename, "rb", USE_PATH | REPORT_ERRORS | ENFORCE_SAFE_MODE | STREAM_DISABLE_OPEN_BASEDIR, NULL);
	if (!stream) {
		return FAILURE;
	}

	PHP_MD5Init(&context);
	while ((n = php_stream_read(stream, (char *) buf, (int) sizeof(buf))) > 0) {
		PHP_MD5Update(&context, buf, n);
	}
	PHP_MD5Final((unsigned char *) compiler->new_php.md5.digest, &context);

	php_stream_close(stream);
	if (EG(regular_list).nNextFreeElement == old_rsid + 1) {
		EG(regular_list).nNextFreeElement = old_rsid;
	}

	if (n < 0) {
		return FAILURE;
	}

	compiler->new_php.hvalue = (xc_php_hash_md5(&compiler->new_php TSRMLS_CC) & cache->hphp->mask);
#ifdef XCACHE_DEBUG
	{
		char md5str[33];
		make_digest(md5str, (unsigned char *) compiler->new_php.md5.digest);
		TRACE("md5 %s", md5str);
	}
#endif

	return SUCCESS;
}
/* }}} */
static void xc_entry_php_init(xc_entry_php_t *entry_php, const char *filepath TSRMLS_DC) /* {{{*/
{
	entry_php->filepath.str = (char *) filepath;
	entry_php->filepath.len = strlen(entry_php->filepath.str);
#ifdef ZEND_ENGINE_2_3
	entry_php->dirpath.str  = estrndup(entry_php->filepath.str, entry_php->filepath.len);
	entry_php->dirpath.len  = zend_dirname(entry_php->dirpath.str, entry_php->filepath.len);
#endif
#ifdef IS_UNICODE
	zend_string_to_unicode(ZEND_U_CONVERTER(UG(runtime_encoding_conv)), &entry_php->u_filepath.str, &entry_php->u_filepath.len, entry_php->filepath.str, entry_php->filepath.len TSRMLS_CC);
	zend_string_to_unicode(ZEND_U_CONVERTER(UG(runtime_encoding_conv)), &entry_php->u_dirpath.str,  &entry_php->u_dirpath.len,  entry_php->dirpath.str,  entry_php->dirpath.len TSRMLS_CC);
#endif
}
/* }}} */
#ifndef ZEND_COMPILE_DELAYED_BINDING
static void xc_cache_early_binding_class_cb(zend_op *opline, int oplineno, void *data TSRMLS_DC) /* {{{ */
{
	char *class_name;
	zend_uint i;
	int class_len;
	xc_cest_t cest;
	xc_entry_data_php_t *php = (xc_entry_data_php_t *) data;

	class_name = Z_OP_CONSTANT(opline->op1).value.str.val;
	class_len  = Z_OP_CONSTANT(opline->op1).value.str.len;
	if (zend_hash_find(CG(class_table), class_name, class_len, (void **) &cest) == FAILURE) {
		assert(0);
	}
	TRACE("got ZEND_DECLARE_INHERITED_CLASS: %s", class_name + 1);
	/* let's see which class */
	for (i = 0; i < php->classinfo_cnt; i ++) {
		if (memcmp(ZSTR_S(php->classinfos[i].key), class_name, class_len) == 0) {
			php->classinfos[i].oplineno = oplineno;
			php->have_early_binding = 1;
			break;
		}
	}

	if (i == php->classinfo_cnt) {
		assert(0);
	}
}
/* }}} */
#endif

/* {{{ Constant Usage */
#define xcache_constant_is_file    1
#ifdef ZEND_ENGINE_2_3
#	define xcache_constant_is_dir  2
#endif
#define xcache_constant_info_mask  0x0F
#define xcache_constant_info_shift 4
typedef struct {
	zend_bool filepath_used;
#ifdef ZEND_ENGINE_2_3
	zend_bool dirpath_used;
#endif
#ifdef IS_UNICODE
	zend_bool u_filepath_used;
#	ifdef ZEND_ENGINE_2_3
	zend_bool u_dirpath_used;
#	endif
#endif
} xc_const_usage_t;

#define XCACHE_ANALYZE_CONSTANT(type, shift) \
	if (zend_binary_strcmp(Z_STRVAL_P(constant), Z_STRLEN_P(constant), compiler->new_entry.type##path.str, compiler->new_entry.type##path.len) == 0) { \
		usage->type##path_used = 1; \
		constantinfo |= xcache_constant_is_##type << (shift * xcache_constant_info_shift); \
		TRACE("found %s constant %s", #type, Z_STRVAL_P(constant)); \
	}

#define XCACHE_ANALYZE_UCONSTANT(type, shift) \
	if (zend_u_binary_strcmp(Z_STRVAL_P(constant), Z_STRLEN_P(constant), compiler->new_entry.u_#type##path.str, compiler->new_entry.u_##type##path.len) == 0) { \
		usage->u_##type##path_used = 1; \
		constantinfo |= xcache_constant_is_##type << (shift * xcache_constant_info_shift); \
		TRACE("found %s constant %s", #type, Z_STRVAL_P(constant)); \
	}

/* }}} */
#ifdef ZEND_ENGINE_2
static void xc_collect_class_constant_info(xc_compiler_t *compiler, xc_const_usage_t *usage, xc_classinfo_t *classinfo, HashTable *constants TSRMLS_DC) /* {{{ */
{
	uint index;
	xc_vector_t constantinfos = xc_vector_initializer(xc_constant_info_t, 0);

	for (index = 0; index < constants->nTableSize; ++index) {
		Bucket *bucket;
		zend_uint index2 = 0;

		for (bucket = constants->arBuckets[index]; bucket; bucket = bucket->pNext, ++index2) {
			zval *constant = *(zval **) bucket->pData;
			zend_uint constantinfo = 0;

			assert(constant);

			if ((Z_TYPE_P(constant) & IS_CONSTANT_TYPE_MASK) == IS_STRING) {
				XCACHE_ANALYZE_CONSTANT(file, 0)
				else XCACHE_ANALYZE_CONSTANT(dir, 0)
			}
#	ifdef IS_UNICODE
			else if ((Z_TYPE_P(constant) & IS_CONSTANT_TYPE_MASK) == IS_UNICODE) {
				XCACHE_ANALYZE_UCONSTANT(file, 0)
				else XCACHE_ANALYZE_UCONSTANT(dir, 0)
			}
#	endif

			if (constantinfo) {
				xc_constant_info_t detail;
				detail.index = index2 * constants->nTableSize + index;
				detail.info  = constantinfo;
				xc_vector_push_back(&constantinfos, &detail);
			}
		}
	}

	classinfo->constantinfo_cnt = xc_vector_size(&constantinfos);
	classinfo->constantinfos    = xc_vector_detach(xc_constant_info_t, &constantinfos);
}
/* }}} */
#endif
static void xc_collect_op_array_info(xc_compiler_t *compiler, xc_const_usage_t *usage, xc_op_array_info_t *op_array_info, zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
#ifdef ZEND_ENGINE_2_4
	int literalindex;
#else
	zend_uint oplinenum;
#endif
	xc_vector_t constantinfos = xc_vector_initializer(xc_constant_info_t, 0);

#ifdef ZEND_ENGINE_2_4
	for (literalindex = 0; literalindex < op_array->last_literal; literalindex++) {
		zend_literal *literal = &op_array->literals[literalindex];
		zval *constant = &literal->constant;
		zend_uint constantinfo = 0;

		if (Z_TYPE_P(constant) == IS_STRING) {
			XCACHE_ANALYZE_CONSTANT(file, 0)
			else XCACHE_ANALYZE_CONSTANT(dir, 0)
		}
#	ifdef IS_UNICODE
		else if (Z_TYPE_P(constant) == IS_UNICODE) {
			XCACHE_ANALYZE_UCONSTANT(file, 0)
			else XCACHE_ANALYZE_UCONSTANT(dir, 0)
		}
#	endif

		if (constantinfo) {
			xc_constant_info_t detail;
			detail.index = literalindex;
			detail.info  = constantinfo;
			xc_vector_push_back(&constantinfos, &detail);
		}
	}
#else /* ZEND_ENGINE_2_4 */
	for (oplinenum = 0; oplinenum < op_array->last; oplinenum++) {
		zend_op *opline = &op_array->opcodes[oplinenum];
		zend_uint constantinfo = 0;

		if (Z_OP_TYPE(opline->op1) == IS_CONST) {
			zval *constant = &Z_OP_CONSTANT(opline->op1);
			if (Z_TYPE_P(constant) == IS_STRING) {
				XCACHE_ANALYZE_CONSTANT(file, 0)
#	ifdef ZEND_ENGINE_2_3
				else XCACHE_ANALYZE_CONSTANT(dir, 0)
#	endif
			}
#	ifdef IS_UNICODE
			else if (Z_TYPE_P(constant) == IS_UNICODE) {
				XCACHE_ANALYZE_UCONSTANT(file, 0)
				else XCACHE_ANALYZE_UCONSTANT(dir, 0)
			}
#	endif
		}

		if (Z_OP_TYPE(opline->op2) == IS_CONST) {
			zval *constant = &Z_OP_CONSTANT(opline->op2);
			if (Z_TYPE_P(constant) == IS_STRING) {
				XCACHE_ANALYZE_CONSTANT(file, 1)
#	ifdef ZEND_ENGINE_2_3
				else XCACHE_ANALYZE_CONSTANT(dir, 1)
#	endif
			}
#	ifdef IS_UNICODE
			else if (Z_TYPE_P(constant) == IS_UNICODE) {
				XCACHE_ANALYZE_UCONSTANT(file, 1)
				else XCACHE_ANALYZE_UCONSTANT(dir, 1)
			}
#	endif
		}

		if (constantinfo) {
			xc_constant_info_t detail;
			detail.index = oplinenum;
			detail.info  = constantinfo;
			xc_vector_add(xc_constant_info_t, &constantinfos, detail);
		}
	}
#endif /* ZEND_ENGINE_2_4 */

	op_array_info->constantinfo_cnt = xc_vector_size(&constantinfos);
	op_array_info->constantinfos    = xc_vector_detach(xc_constant_info_t, &constantinfos);
	xc_vector_destroy(&constantinfos);
}
/* }}} */
#ifdef IS_UNICODE
#	define xc_restore_constant_string xc_restore_constant_string_impl
#else
#	define xc_restore_constant_string(where, constant, type, constant_string, constant_u_string, shallow_copy) xc_restore_constant_string_impl(where, constant, type, constant_string, shallow_copy)
#endif
static inline void xc_restore_constant_string(const char *where, zval *constant, zend_uchar type, const xc_constant_string_t *constant_string, const xc_constant_u_string_t *constant_u_string, int shallow_copy TSRMLS_DC) /* {{{ */
{
	TRACE("fixing constant in %s: %s", where, Z_STRVAL_P(constant));
	if (!shallow_copy) {
		efree(Z_STRVAL_P(constant));
	}
	if (type == IS_STRING) {
		assert(constant_string->str);
		ZVAL_STRINGL(constant, constant_string->str, constant_string->len, !shallow_copy);
		TRACE("fixed  constant in %s: %s", where, constant_string->str);
	}
#ifdef IS_UNICODE
	else if (type == IS_UNICODE) {
		assert(constant_u_string.str);
		ZVAL_UNICODEL(constant, constant_u_string->str, constant_u_string->len, !shallow_copy);
		/* TRACE("restored constant: %s", constant_u_string->str); */
	}
#endif
	else {
		assert(0);
	}
}
/* }}} */
#ifdef ZEND_ENGINE_2
void xc_fix_class_info(const xc_entry_php_t *entry_php, xc_classinfo_t *classinfo, int shallow_copy TSRMLS_DC) /* {{{ */
{
	zend_class_entry *ce = CestToCePtr(classinfo->cest);
	Bucket **constants = ce->constants_table.arBuckets;
	
	zend_uint constantinfo_index;

	for (constantinfo_index = 0; constantinfo_index < classinfo->constantinfo_cnt; ++constantinfo_index) {
		int index = classinfo->constantinfos[constantinfo_index].index;
		int constantinfo = classinfo->constantinfos[constantinfo_index].info;

		int index2 = index / ce->constants_table.nTableSize;

		Bucket *bucket;
		zval *constant;

		for (bucket = constants[(index & ce->constants_table.nTableMask)]; index2; bucket = bucket->pNext, --index2) {
			/* empty */
		}
		constant = *((zval **) bucket->pData);

		if ((constantinfo & xcache_constant_is_file)) {
			xc_restore_constant_string("class_constant", constant, Z_TYPE_P(constant), &entry_php->filepath, &entry_php->u_filepath, shallow_copy TSRMLS_CC);
		}
		else if ((constantinfo & xcache_constant_is_dir)) {
			xc_restore_constant_string("class_constant", constant, Z_TYPE_P(constant), &entry_php->dirpath, &entry_php->u_dirpath, shallow_copy TSRMLS_CC);
		}
	}
}
/* }}} */
#endif
void xc_fix_op_array_info(const xc_entry_php_t *entry_php, zend_op_array *op_array, int shallow_copy, const xc_op_array_info_t *op_array_info TSRMLS_DC) /* {{{ */
{
	zend_uint constantinfo_index;
	for (constantinfo_index = 0; constantinfo_index < op_array_info->constantinfo_cnt; ++constantinfo_index) {
		int index = op_array_info->constantinfos[constantinfo_index].index;
		int constantinfo = op_array_info->constantinfos[constantinfo_index].info;

#ifdef ZEND_ENGINE_2_4
		zend_literal *literal = &op_array->literals[index];

		if ((constantinfo & xcache_constant_is_file)) {
			xc_restore_constant_string("literal", &literal->constant, Z_TYPE(literal->constant), &entry_php->filepath, &entry_php->u_filepath, shallow_copy TSRMLS_CC);
		}
		else if ((constantinfo & xcache_constant_is_dir)) {
			xc_restore_constant_string("literal", &literal->constant, Z_TYPE(literal->constant), &entry_php->dirpath, &entry_php->u_dirpath, shallow_copy TSRMLS_CC);
		}
#else /* ZEND_ENGINE_2_4 */
		zend_op *opline = &op_array->opcodes[index];
		
		if ((constantinfo & xcache_constant_is_file)) {
			xc_restore_constant_string("op1", &Z_OP_CONSTANT(opline->op1), Z_TYPE(Z_OP_CONSTANT(opline->op1)), &entry_php->filepath, &entry_php->u_filepath, shallow_copy TSRMLS_CC);
		}
#	ifdef ZEND_ENGINE_2_3
		else if ((constantinfo & xcache_constant_is_dir)) {
			xc_restore_constant_string("op1", &Z_OP_CONSTANT(opline->op1), Z_TYPE(Z_OP_CONSTANT(opline->op1)), &entry_php->dirpath, &entry_php->u_dirpath, shallow_copy TSRMLS_CC);
		}
#	endif

		constantinfo >>= xcache_constant_info_shift * 1;
		if ((constantinfo & xcache_constant_is_file)) {
			xc_restore_constant_string("op2", &Z_OP_CONSTANT(opline->op2), Z_TYPE(Z_OP_CONSTANT(opline->op2)), &entry_php->filepath, &entry_php->u_filepath, shallow_copy TSRMLS_CC);
		}
#	ifdef ZEND_ENGINE_2_3
		else if ((constantinfo & xcache_constant_is_dir)) {
			xc_restore_constant_string("op2", &Z_OP_CONSTANT(opline->op2), Z_TYPE(Z_OP_CONSTANT(opline->op2)), &entry_php->dirpath, &entry_php->u_dirpath, shallow_copy TSRMLS_CC);
		}
#	endif
#endif /* ZEND_ENGINE_2_4 */
	}
}
/* }}} */
static void xc_free_op_array_info(xc_op_array_info_t *op_array_info TSRMLS_DC) /* {{{ */
{
#ifdef ZEND_ENGINE_2_4
	if (op_array_info->constantinfos) {
		efree(op_array_info->constantinfos);
	}
#else
	if (op_array_info->constantinfos) {
		efree(op_array_info->constantinfos);
	}
#endif
}
/* }}} */
static void xc_free_php(xc_entry_data_php_t *php TSRMLS_DC) /* {{{ */
{
	zend_uint i;
	if (php->classinfos) {
		for (i = 0; i < php->classinfo_cnt; i ++) {
			xc_classinfo_t *classinfo = &php->classinfos[i];
			zend_uint j;

			for (j = 0; j < classinfo->methodinfo_cnt; j ++) {
				xc_free_op_array_info(&classinfo->methodinfos[j] TSRMLS_CC);
			}

			if (classinfo->methodinfos) {
				efree(classinfo->methodinfos);
			}

#ifdef ZEND_ENGINE_2
			if (classinfo->constantinfos) {
				efree(classinfo->constantinfos);
			}
#endif
		}
	}
	if (php->funcinfos) {
		for (i = 0; i < php->funcinfo_cnt; i ++) {
			xc_free_op_array_info(&php->funcinfos[i].op_array_info TSRMLS_CC);
		}
	}
	xc_free_op_array_info(&php->op_array_info TSRMLS_CC);

#define X_FREE(var) do {\
	if (php->var) { \
		efree(php->var); \
	} \
} while (0)

#ifdef ZEND_ENGINE_2_1
	X_FREE(autoglobals);
#endif
	X_FREE(classinfos);
	X_FREE(funcinfos);
#ifdef HAVE_XCACHE_CONSTANT
	X_FREE(constinfos);
#endif
#undef X_FREE
}
/* }}} */
static void xc_compile_php(xc_compiler_t *compiler, zend_file_handle *h, int type TSRMLS_DC) /* {{{ */
{
	zend_uint old_constinfo_cnt, old_funcinfo_cnt, old_classinfo_cnt;
	zend_bool catched = 0;

	/* {{{ compile */
	TRACE("compiling %s", h->opened_path ? h->opened_path : h->filename);

	old_classinfo_cnt = zend_hash_num_elements(CG(class_table));
	old_funcinfo_cnt  = zend_hash_num_elements(CG(function_table));
	old_constinfo_cnt = zend_hash_num_elements(EG(zend_constants));

	zend_try {
		compiler->new_php.op_array = old_compile_file(h, type TSRMLS_CC);
	} zend_catch {
		catched = 1;
	} zend_end_try();

	if (catched) {
		goto err_bailout;
	}

	if (compiler->new_php.op_array == NULL) {
		goto err_op_array;
	}

	if (!XG(initial_compile_file_called)) {
		TRACE("%s", "!initial_compile_file_called, give up");
		return;
	}

	/* }}} */
	/* {{{ prepare */
	zend_restore_compiled_filename(h->opened_path ? h->opened_path : (char *) h->filename TSRMLS_CC);

#ifdef HAVE_XCACHE_CONSTANT
	compiler->new_php.constinfo_cnt  = zend_hash_num_elements(EG(zend_constants)) - old_constinfo_cnt;
#endif
	compiler->new_php.funcinfo_cnt   = zend_hash_num_elements(CG(function_table)) - old_funcinfo_cnt;
	compiler->new_php.classinfo_cnt  = zend_hash_num_elements(CG(class_table))    - old_classinfo_cnt;
#ifdef ZEND_ENGINE_2_1
	/* {{{ count new_php.autoglobal_cnt */ {
		Bucket *b;

		compiler->new_php.autoglobal_cnt = 0;
		for (b = CG(auto_globals)->pListHead; b != NULL; b = b->pListNext) {
			zend_auto_global *auto_global = (zend_auto_global *) b->pData;
			/* check if actived */
			if (auto_global->auto_global_callback && !auto_global->armed) {
				compiler->new_php.autoglobal_cnt ++;
			}
		}
	}
	/* }}} */
#endif

#define X_ALLOC_N(var, cnt) do {     \
	if (compiler->new_php.cnt) {                  \
		ECALLOC_N(compiler->new_php.var, compiler->new_php.cnt); \
		if (!compiler->new_php.var) {             \
			goto err_alloc;          \
		}                            \
	}                                \
	else {                           \
		compiler->new_php.var = NULL;             \
	}                                \
} while (0)

#ifdef HAVE_XCACHE_CONSTANT
	X_ALLOC_N(constinfos,  constinfo_cnt);
#endif
	X_ALLOC_N(funcinfos,   funcinfo_cnt);
	X_ALLOC_N(classinfos,  classinfo_cnt);
#ifdef ZEND_ENGINE_2_1
	X_ALLOC_N(autoglobals, autoglobal_cnt);
#endif
#undef X_ALLOC
	/* }}} */

	/* {{{ shallow copy, pointers only */ {
		Bucket *b;
		zend_uint i;
		zend_uint j;

#define COPY_H(vartype, var, cnt, name, datatype) do {        \
	for (i = 0, j = 0; b; i ++, b = b->pListNext) {           \
		vartype *data = &compiler->new_php.var[j];            \
                                                              \
		if (i < old_##cnt) {                                  \
			continue;                                         \
		}                                                     \
		j ++;                                                 \
                                                              \
		assert(i < old_##cnt + compiler->new_php.cnt);        \
		assert(b->pData);                                     \
		memcpy(&data->name, b->pData, sizeof(datatype));      \
		UNISW(NOTHING, data->type = b->key.type;)             \
		if (UNISW(1, b->key.type == IS_STRING)) {             \
			ZSTR_S(data->key)      = BUCKET_KEY_S(b);         \
		}                                                     \
		else {                                                \
			ZSTR_U(data->key)      = BUCKET_KEY_U(b);         \
		}                                                     \
		data->key_size   = b->nKeyLength;                     \
		data->h          = b->h;                              \
	}                                                         \
} while(0)

#ifdef HAVE_XCACHE_CONSTANT
		b = EG(zend_constants)->pListHead; COPY_H(xc_constinfo_t, constinfos, constinfo_cnt, constant, zend_constant);
#endif
		b = CG(function_table)->pListHead; COPY_H(xc_funcinfo_t,  funcinfos,  funcinfo_cnt,  func,     zend_function);
		b = CG(class_table)->pListHead;    COPY_H(xc_classinfo_t, classinfos, classinfo_cnt, cest,     xc_cest_t);

#undef COPY_H

		/* for ZE1, cest need to be fixed inside store */

#ifdef ZEND_ENGINE_2_1
		/* scan for acatived auto globals */
		i = 0;
		for (b = CG(auto_globals)->pListHead; b != NULL; b = b->pListNext) {
			zend_auto_global *auto_global = (zend_auto_global *) b->pData;
			/* check if actived */
			if (auto_global->auto_global_callback && !auto_global->armed) {
				xc_autoglobal_t *data = &compiler->new_php.autoglobals[i];

				assert(i < compiler->new_php.autoglobal_cnt);
				i ++;
				UNISW(NOTHING, data->type = b->key.type;)
				if (UNISW(1, b->key.type == IS_STRING)) {
					ZSTR_S(data->key)     = BUCKET_KEY_S(b);
				}
				else {
					ZSTR_U(data->key)     = BUCKET_KEY_U(b);
				}
				data->key_len = b->nKeyLength - 1;
				data->h       = b->h;
			}
		}
#endif
	}
	/* }}} */

	/* {{{ collect info for file/dir path */ {
		Bucket *b;
		xc_const_usage_t const_usage;
		unsigned int i;

		xc_entry_php_init(&compiler->new_entry, zend_get_compiled_filename(TSRMLS_C) TSRMLS_CC);
		memset(&const_usage, 0, sizeof(const_usage));

		for (i = 0; i < compiler->new_php.classinfo_cnt; i ++) {
			xc_classinfo_t *classinfo = &compiler->new_php.classinfos[i];
			zend_class_entry *ce = CestToCePtr(classinfo->cest);

			classinfo->methodinfo_cnt = ce->function_table.nNumOfElements;
			if (classinfo->methodinfo_cnt) {
				int j;

				ECALLOC_N(classinfo->methodinfos, classinfo->methodinfo_cnt);
				if (!classinfo->methodinfos) {
					goto err_alloc;
				}

				for (j = 0, b = ce->function_table.pListHead; b; j ++, b = b->pListNext) {
					xc_collect_op_array_info(compiler, &const_usage, &classinfo->methodinfos[j], (zend_op_array *) b->pData TSRMLS_CC);
				}
			}
			else {
				classinfo->methodinfos = NULL;
			}

#ifdef ZEND_ENGINE_2
			if (ce->constants_table.nNumOfElements) {
				xc_collect_class_constant_info(compiler, &const_usage, classinfo, &ce->constants_table TSRMLS_CC);
			}
			else {
				classinfo->constantinfo_cnt = 0;
				classinfo->constantinfos = NULL;
			}
#endif
		}

		for (i = 0; i < compiler->new_php.funcinfo_cnt; i ++) {
			xc_collect_op_array_info(compiler, &const_usage, &compiler->new_php.funcinfos[i].op_array_info, (zend_op_array *) &compiler->new_php.funcinfos[i].func TSRMLS_CC);
		}

		xc_collect_op_array_info(compiler, &const_usage, &compiler->new_php.op_array_info, compiler->new_php.op_array TSRMLS_CC);

		/* file/dir path free unused */
#define X_FREE_UNUSED(var) \
		if (!const_usage.var##path_used) { \
			efree(compiler->new_entry.var##path.str); \
			compiler->new_entry.var##path.str = NULL; \
			compiler->new_entry.var##path.len = 0; \
		}
		/* filepath is required to restore op_array->filename, so no free filepath here */
#ifdef ZEND_ENGINE_2_3
		X_FREE_UNUSED(dir)
#endif
#ifdef IS_UNICODE
		X_FREE_UNUSED(u_file)
		X_FREE_UNUSED(u_dir)
#endif
#undef X_FREE_UNUSED
	}
	/* }}} */
#ifdef XCACHE_ERROR_CACHING
	compiler->new_php.compilererrors = xc_sandbox_compilererrors(TSRMLS_C);
	compiler->new_php.compilererror_cnt = xc_sandbox_compilererror_cnt(TSRMLS_C);
#endif
#ifndef ZEND_COMPILE_DELAYED_BINDING
	/* {{{ find inherited classes that should be early-binding */
	compiler->new_php.have_early_binding = 0;
	{
		zend_uint i;
		for (i = 0; i < compiler->new_php.classinfo_cnt; i ++) {
			compiler->new_php.classinfos[i].oplineno = -1;
		}
	}

	xc_undo_pass_two(compiler->new_php.op_array TSRMLS_CC);
	xc_foreach_early_binding_class(compiler->new_php.op_array, xc_cache_early_binding_class_cb, (void *) &compiler->new_php TSRMLS_CC);
	xc_redo_pass_two(compiler->new_php.op_array TSRMLS_CC);
	/* }}} */
#endif

	return;

err_alloc:
	xc_free_php(&compiler->new_php TSRMLS_CC);

err_bailout:
err_op_array:

	if (catched) {
		zend_bailout();
	}
}
/* }}} */
static zend_op_array *xc_compile_restore(xc_entry_php_t *stored_entry, xc_entry_data_php_t *stored_php TSRMLS_DC) /* {{{ */
{
	zend_op_array *op_array;
	xc_entry_php_t restored_entry;
	xc_entry_data_php_t restored_php;
	zend_bool catched;
	zend_uint i;

	/* still needed because in zend_language_scanner.l, require()/include() check file_handle.handle.stream.handle */
	i = 1;
	zend_hash_add(&EG(included_files), stored_entry->entry.name.str.val, stored_entry->entry.name.str.len + 1, (void *)&i, sizeof(int), NULL);

	CG(in_compilation)    = 1;
	CG(compiled_filename) = stored_entry->entry.name.str.val;
	CG(zend_lineno)       = 0;
	TRACE("restoring %lu:%s", (unsigned long) stored_entry->file_inode, stored_entry->entry.name.str.val);
	xc_processor_restore_xc_entry_php_t(&restored_entry, stored_entry TSRMLS_CC);
	xc_processor_restore_xc_entry_data_php_t(stored_entry, &restored_php, stored_php, xc_readonly_protection TSRMLS_CC);
	restored_entry.php = &restored_php;
#ifdef SHOW_DPRINT
	xc_dprint(&restored_entry, 0 TSRMLS_CC);
#endif

	catched = 0;
	zend_try {
		op_array = xc_entry_install(&restored_entry TSRMLS_CC);
	} zend_catch {
		catched = 1;
	} zend_end_try();

#ifdef HAVE_XCACHE_CONSTANT
	if (restored_php.constinfos) {
		efree(restored_php.constinfos);
	}
#endif
	if (restored_php.funcinfos) {
		efree(restored_php.funcinfos);
	}
	if (restored_php.classinfos) {
		efree(restored_php.classinfos);
	}

	if (catched) {
		zend_bailout();
	}
	CG(in_compilation)    = 0;
	CG(compiled_filename) = NULL;
	TRACE("restored %lu:%s", (unsigned long) stored_entry->file_inode, stored_entry->entry.name.str.val);
	return op_array;
}
/* }}} */
typedef struct xc_sandboxed_compiler_t { /* {{{ */
	xc_compiler_t *compiler;
	/* input */
	zend_file_handle *h;
	int type;

	/* sandbox output */
	xc_entry_php_t *stored_entry;
	xc_entry_data_php_t *stored_php;
} xc_sandboxed_compiler_t;
/* }}} */

static zend_op_array *xc_compile_file_sandboxed(void *data TSRMLS_DC) /* {{{ */
{
	xc_sandboxed_compiler_t *sandboxed_compiler = (xc_sandboxed_compiler_t *) data;
	xc_compiler_t *compiler = sandboxed_compiler->compiler;
	zend_bool catched = 0;
	xc_cache_t *cache = &xc_php_caches[compiler->entry_hash.cacheid];
	xc_entry_php_t *stored_entry;
	xc_entry_data_php_t *stored_php;

	/* {{{ compile */
	/* make compile inside sandbox */
#ifdef HAVE_XCACHE_CONSTANT
	compiler->new_php.constinfos  = NULL;
#endif
	compiler->new_php.funcinfos   = NULL;
	compiler->new_php.classinfos  = NULL;
#ifdef ZEND_ENGINE_2_1
	compiler->new_php.autoglobals = NULL;
#endif
	memset(&compiler->new_php.op_array_info, 0, sizeof(compiler->new_php.op_array_info));

	zend_try {
		compiler->new_php.op_array = NULL;
		xc_compile_php(compiler, sandboxed_compiler->h, sandboxed_compiler->type TSRMLS_CC);
	} zend_catch {
		catched = 1;
	} zend_end_try();

	if (catched
	 || !compiler->new_php.op_array /* possible ? */
	 || !XG(initial_compile_file_called)) {
		goto err_aftersandbox;
	}

	/* }}} */
#ifdef SHOW_DPRINT
	compiler->new_entry.php = &compiler->new_php;
	xc_dprint(&compiler->new_entry, 0 TSRMLS_CC);
#endif

	stored_entry = NULL;
	stored_php = NULL;
	ENTER_LOCK_EX(cache) { /* {{{ php_store/entry_store */
		/* php_store */
		stored_php = xc_php_store_unlocked(cache, &compiler->new_php TSRMLS_CC);
		if (!stored_php) {
			/* error */
			break;
		}
		/* entry_store */
		compiler->new_entry.php = stored_php;
		stored_entry = xc_entry_php_store_unlocked(cache, compiler->entry_hash.entryslotid, &compiler->new_entry TSRMLS_CC);
		if (stored_entry) {
			xc_php_addref_unlocked(stored_php);
			TRACE(" cached %lu:%s, holding", (unsigned long) compiler->new_entry.file_inode, stored_entry->entry.name.str.val);
			xc_entry_hold_php_unlocked(cache, stored_entry TSRMLS_CC);
		}
	} LEAVE_LOCK_EX(cache);
	/* }}} */
	TRACE("%s", stored_entry ? "stored" : "store failed");

	if (catched || !stored_php) {
		goto err_aftersandbox;
	}

	cache->cached->compiling = 0;
	xc_free_php(&compiler->new_php TSRMLS_CC);

	if (stored_entry) {
		sandboxed_compiler->stored_entry = stored_entry;
		sandboxed_compiler->stored_php = stored_php;
		/* discard newly compiled result, restore from stored one */
		if (compiler->new_php.op_array) {
#ifdef ZEND_ENGINE_2
			destroy_op_array(compiler->new_php.op_array TSRMLS_CC);
#else
			destroy_op_array(compiler->new_php.op_array);
#endif
			efree(compiler->new_php.op_array);
			compiler->new_php.op_array = NULL;
		}
		return NULL;
	}
	else {
		return compiler->new_php.op_array;
	}

err_aftersandbox:
	xc_free_php(&compiler->new_php TSRMLS_CC);

	cache->cached->compiling = 0;
	if (catched) {
		cache->cached->errors ++;
		zend_bailout();
	}
	return compiler->new_php.op_array;
} /* }}} */
static zend_op_array *xc_compile_file_cached(xc_compiler_t *compiler, zend_file_handle *h, int type TSRMLS_DC) /* {{{ */
{
	/*
	if (clog) {
		return old;
	}

	if (cached_entry = getby entry_hash) {
		php = cached_entry.php;
		php = restore(php);
		return php;
	}
	else {
		if (!(php = getby md5)) {
			if (clog) {
				return old;
			}

			inside_sandbox {
				php = compile;
				entry = create entries[entry];
			}
		}

		entry.php = php;
		return php;
	}
	*/

	xc_entry_php_t *stored_entry;
	xc_entry_data_php_t *stored_php;
	zend_bool gaveup = 0;
	zend_bool catched = 0;
	zend_op_array *op_array;
	xc_cache_t *cache = &xc_php_caches[compiler->entry_hash.cacheid];
	xc_sandboxed_compiler_t sandboxed_compiler;

	if (cache->cached->disabled) {
		return old_compile_file(h, type TSRMLS_CC);
	}
	/* stale skips precheck */
	if (cache->cached->disabled || XG(request_time) - cache->cached->compiling < 30) {
		cache->cached->skips ++;
		return old_compile_file(h, type TSRMLS_CC);
	}

	/* {{{ entry_lookup/hit/md5_init/php_lookup */
	stored_entry = NULL;
	stored_php = NULL;

	ENTER_LOCK_EX(cache) {
		if (!compiler->opened_path && xc_resolve_path_check_entry_unlocked(compiler, compiler->filename, &stored_entry TSRMLS_CC) == SUCCESS) {
			compiler->opened_path = compiler->new_entry.entry.name.str.val;
		}
		else {
			if (!compiler->opened_path && xc_entry_php_resolve_opened_path(compiler, NULL TSRMLS_CC) != SUCCESS) {
				gaveup = 1;
				break;
			}

			/* finalize name */
			compiler->new_entry.entry.name.str.val = (char *) compiler->opened_path;
			compiler->new_entry.entry.name.str.len = strlen(compiler->new_entry.entry.name.str.val);

			stored_entry = (xc_entry_php_t *) xc_entry_find_unlocked(XC_TYPE_PHP, cache, compiler->entry_hash.entryslotid, (xc_entry_t *) &compiler->new_entry TSRMLS_CC);
		}

		if (stored_entry) {
			xc_cached_hit_unlocked(cache->cached TSRMLS_CC);

			TRACE(" hit %lu:%s, holding", (unsigned long) compiler->new_entry.file_inode, stored_entry->entry.name.str.val);
			xc_entry_hold_php_unlocked(cache, stored_entry TSRMLS_CC);
			stored_php = stored_entry->php;
			break;
		}

		TRACE("miss entry %lu:%s", (unsigned long) compiler->new_entry.file_inode, compiler->new_entry.entry.name.str.val);

		if (xc_entry_data_php_init_md5(cache, compiler TSRMLS_CC) != SUCCESS) {
			gaveup = 1;
			break;
		}

		stored_php = xc_php_find_unlocked(cache->cached, &compiler->new_php TSRMLS_CC);

		if (stored_php) {
			compiler->new_entry.php = stored_php;
			xc_entry_php_init(&compiler->new_entry, compiler->opened_path TSRMLS_CC);
			stored_entry = xc_entry_php_store_unlocked(cache, compiler->entry_hash.entryslotid, &compiler->new_entry TSRMLS_CC);
			if (stored_entry) {
				xc_php_addref_unlocked(stored_php);
				TRACE(" cached %lu:%s, holding", (unsigned long) compiler->new_entry.file_inode, stored_entry->entry.name.str.val);
				xc_entry_hold_php_unlocked(cache, stored_entry TSRMLS_CC);
			}
			else {
				gaveup = 1;
			}
			break;
		}

		if (XG(request_time) - cache->cached->compiling < 30) {
			TRACE("%s", "miss php, but compiling");
			cache->cached->skips ++;
			gaveup = 1;
			break;
		}

		TRACE("%s", "miss php, going to compile");
		cache->cached->compiling = XG(request_time);
	} LEAVE_LOCK_EX(cache);

	if (catched) {
		cache->cached->compiling = 0;
		zend_bailout();
	}

	/* found entry */
	if (stored_entry && stored_php) {
		return xc_compile_restore(stored_entry, stored_php TSRMLS_CC);
	}

	/* gaveup */
	if (gaveup) {
		return old_compile_file(h, type TSRMLS_CC);
	}
	/* }}} */

	sandboxed_compiler.compiler = compiler;
	sandboxed_compiler.h = h;
	sandboxed_compiler.type = type;
	sandboxed_compiler.stored_php = NULL;
	sandboxed_compiler.stored_entry = NULL;
	op_array = xc_sandbox(xc_compile_file_sandboxed, (void *) &sandboxed_compiler, h->opened_path ? h->opened_path : h->filename TSRMLS_CC);
	if (sandboxed_compiler.stored_entry) {
		return xc_compile_restore(sandboxed_compiler.stored_entry, sandboxed_compiler.stored_php TSRMLS_CC);
	}
	else {
		return op_array;
	}
}
/* }}} */
static zend_op_array *xc_compile_file(zend_file_handle *h, int type TSRMLS_DC) /* {{{ */
{
	xc_compiler_t compiler;
	zend_op_array *op_array;

	assert(xc_initized);

	TRACE("xc_compile_file: type=%d name=%s", h->type, h->filename ? h->filename : "NULL");

	if (!XG(cacher)
	 || !h->filename
	 || !SG(request_info).path_translated
	) {
		TRACE("%s", "cacher not enabled");
		return old_compile_file(h, type TSRMLS_CC);
	}

	/* {{{ entry_init_key */
	compiler.opened_path = h->opened_path;
	compiler.filename = compiler.opened_path ? compiler.opened_path : h->filename;
	compiler.filename_len = strlen(compiler.filename);
	if (xc_entry_php_init_key(&compiler TSRMLS_CC) != SUCCESS) {
		TRACE("failed to init key for %s", compiler.filename);
		return old_compile_file(h, type TSRMLS_CC);
	}
	/* }}} */

	op_array = xc_compile_file_cached(&compiler, h, type TSRMLS_CC);

	xc_entry_free_key_php(&compiler.new_entry TSRMLS_CC);

	return op_array;
}
/* }}} */

/* gdb helper functions, but N/A for coredump */
zend_bool xc_is_rw(const void *p) /* {{{ */
{
	xc_shm_t *shm;
	size_t i;

	if (xc_php_caches) {
		for (i = 0; i < xc_php_hcache.size; i ++) {
			shm = xc_php_caches[i].shm;
			if (shm->vtable->is_readwrite(shm, p)) {
				return 1;
			}
		}
	}

	if (xc_var_caches) {
		for (i = 0; i < xc_var_hcache.size; i ++) {
			shm = xc_var_caches[i].shm;
			if (shm->vtable->is_readwrite(shm, p)) {
				return 1;
			}
		}
	}
	return 0;
}
/* }}} */
zend_bool xc_is_ro(const void *p) /* {{{ */
{
	xc_shm_t *shm;
	size_t i;

	if (xc_php_caches) {
		for (i = 0; i < xc_php_hcache.size; i ++) {
			shm = xc_php_caches[i].shm;
			if (shm->vtable->is_readonly(shm, p)) {
				return 1;
			}
		}
	}

	if (xc_var_caches) {
		for (i = 0; i < xc_var_hcache.size; i ++) {
			shm = xc_var_caches[i].shm;
			if (shm->vtable->is_readonly(shm, p)) {
				return 1;
			}
		}
	}
	return 0;
}
/* }}} */
zend_bool xc_is_shm(const void *p) /* {{{ */
{
	return xc_is_ro(p) || xc_is_rw(p);
}
/* }}} */

void xc_gc_add_op_array(xc_gc_op_array_t *gc_op_array TSRMLS_DC) /* {{{ */
{
	zend_llist_add_element(&XG(gc_op_arrays), (void *) gc_op_array);
}
/* }}} */
static void xc_gc_op_array(void *pDest) /* {{{ */
{
	xc_gc_op_array_t *op_array = (xc_gc_op_array_t *) pDest;
#ifdef ZEND_ENGINE_2
	if (op_array->arg_info) {
		zend_uint i;
		for (i = 0; i < op_array->num_args; i++) {
			efree((char *) ZSTR_V(op_array->arg_info[i].name));
			if (ZSTR_V(op_array->arg_info[i].class_name)) {
				efree((char *) ZSTR_V(op_array->arg_info[i].class_name));
			}
		}
		efree(op_array->arg_info);
	}
#endif
	if (op_array->opcodes) {
		efree(op_array->opcodes);
	}
#ifdef ZEND_ENGINE_2_4
	if (op_array->literals) {
		efree(op_array->literals);
	}
#endif
}
/* }}} */

/* variable namespace */
#ifdef IS_UNICODE
void xc_var_namespace_init_from_unicodel(const UChar *string, int len TSRMLS_DC) /* {{{ */
{
	if (!len) {
#ifdef IS_UNICODE
		ZVAL_EMPTY_UNICODE(&XG(uvar_namespace_hard));
#endif
		ZVAL_EMPTY_STRING(&XG(var_namespace_hard));
	}
	else {
		ZVAL_UNICODE_L(&XG(uvar_namespace_hard), string, len, 1);
		/* TODO: copy to var */
	}
}
/* }}} */
#endif
void xc_var_namespace_init_from_stringl(const char *string, int len TSRMLS_DC) /* {{{ */
{
	if (!len) {
#ifdef IS_UNICODE
		ZVAL_EMPTY_UNICODE(&XG(uvar_namespace_hard));
#endif
		ZVAL_EMPTY_STRING(&XG(var_namespace_hard));
	}
	else {
		ZVAL_STRINGL(&XG(var_namespace_hard), string, len, 1);
#ifdef IS_UNICODE
		/* TODO: copy to uvar */
#endif
	}
}
/* }}} */
void xc_var_namespace_init_from_long(long value TSRMLS_DC) /* {{{ */
{
	ZVAL_LONG(&XG(var_namespace_hard), value);
#ifdef IS_UNICODE
	/* TODO: copy to uvar_namespace */
#endif
}
/* }}} */
#ifdef IS_UNICODE
void xc_var_namespace_set_unicodel(const UChar *unicode, int len TSRMLS_DC) /* {{{ */
{
	zval_dtor(&XG(uvar_namespace_soft));
	zval_dtor(&XG(var_namespace_soft));
	if (len) {
		if (!Z_USTRLEN_P(&XG(uvar_namespace_soft))) {
			ZVAL_UNICODEL(&XG(uvar_namespace_soft), unicode, len, 1);
		}
		else {
			int buffer_len = Z_USTRLEN_P(&XG(var_namespace_hard)) + 1 + len;
			char *buffer = emalloc((buffer_len + 1) * sizeof(unicode[0]));
			char *p = buffer;
			memcpy(p, Z_USTRVAL_P(&XG(var_namespace_hard)), (Z_USTRLEN_P(&XG(var_namespace_hard)) + 1));
			p += (Z_USTRLEN_P(&XG(var_namespace_hard)) + 1) * sizeof(unicode[0]);
			memcpy(p, unicode, (len + 1) * sizeof(unicode[0]));
			ZVAL_UNICODEL(&XG(uvar_namespace_soft), buffer, buffer_len, 0);
		}
		/* TODO: copy to var */
	}
	else {
#ifdef IS_UNICODE
		XG(uvar_namespace_soft) = XG(uvar_namespace_hard);
		zval_copy_ctor(&XG(uvar_namespace_soft));
#endif
		XG(var_namespace_soft) = XG(var_namespace_hard);
		zval_copy_ctor(&XG(var_namespace_soft));
	}
}
/* }}} */
#endif
void xc_var_namespace_set_stringl(const char *string, int len TSRMLS_DC) /* {{{ */
{
#ifdef IS_UNICODE
	zval_dtor(&XG(uvar_namespace_soft));
#endif
	zval_dtor(&XG(var_namespace_soft));
	if (len) {
		if (!Z_STRLEN_P(&XG(var_namespace_soft))) {
			ZVAL_STRINGL(&XG(var_namespace_soft), string, len, 1);
		}
		else {
			int buffer_len = Z_STRLEN_P(&XG(var_namespace_hard)) + 1 + len;
			char *buffer = emalloc(buffer_len + 1);
			char *p = buffer;
			memcpy(p, Z_STRVAL_P(&XG(var_namespace_hard)), Z_STRLEN_P(&XG(var_namespace_hard)) + 1);
			p += Z_STRLEN_P(&XG(var_namespace_hard)) + 1;
			memcpy(p, string, len + 1);
			ZVAL_STRINGL(&XG(var_namespace_soft), buffer, buffer_len, 0);
		}
#ifdef IS_UNICODE
		/* TODO: copy to uvar */
#endif
	}
	else {
#ifdef IS_UNICODE
		XG(uvar_namespace_soft) = XG(uvar_namespace_hard);
		zval_copy_ctor(&XG(uvar_namespace_soft));
#endif
		XG(var_namespace_soft) = XG(var_namespace_hard);
		zval_copy_ctor(&XG(var_namespace_soft));
	}
}
/* }}} */
static void xc_var_namespace_break(TSRMLS_D) /* {{{ */
{
#ifdef IS_UNICODE
	zval_dtor(&XG(uvar_namespace_soft));
#endif
	zval_dtor(&XG(var_namespace_soft));
#ifdef IS_UNICODE
	ZVAL_EMPTY_UNICODE(&XG(uvar_namespace_soft));
#endif
	ZVAL_EMPTY_STRING(&XG(var_namespace_soft));
}
/* }}} */
static void xc_var_namespace_init(TSRMLS_D) /* {{{ */
{
	uid_t id = (uid_t) -1;

	switch (xc_var_namespace_mode) {
		case 1:
			{
				zval **server;
				HashTable *ht;
				zval **val;

#ifdef ZEND_ENGINE_2_1
				zend_is_auto_global("_SERVER", sizeof("_SERVER") - 1 TSRMLS_CC);
#endif

				if (zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void**)&server) == FAILURE
				 || Z_TYPE_PP(server) != IS_ARRAY
				 || !(ht = Z_ARRVAL_P(*server))
				 || zend_hash_find(ht, xc_var_namespace, strlen(xc_var_namespace) + 1, (void**)&val) == FAILURE) {
					xc_var_namespace_init_from_stringl(NULL, 0 TSRMLS_CC);
				}
				else {
#ifdef IS_UNICODE
					if (Z_TYPE_PP(val) == IS_UNICODE) {
						xc_var_namespace_init_from_unicodel(Z_USTRVAL_PP(val), Z_USTRLEN_PP(val) TSRMLS_CC);
					}
					else
#endif
					{
						xc_var_namespace_init_from_stringl(Z_STRVAL_PP(val), Z_STRLEN_PP(val) TSRMLS_CC);
					}
				}
			}
			break;

		case 2:
			if (strncmp(xc_var_namespace, "uid", 3) == 0) {
				id = getuid();
			}
			else if (strncmp(xc_var_namespace, "gid", 3) == 0) {
				id = getgid();
			}

			if (id == (uid_t) -1){
				xc_var_namespace_init_from_stringl(NULL, 0 TSRMLS_CC);
			}
			else {
				xc_var_namespace_init_from_long((long) id TSRMLS_CC);
			}
			break;

		case 0:
		default:
			xc_var_namespace_init_from_stringl(xc_var_namespace, strlen(xc_var_namespace) TSRMLS_CC);
			break;
	}

#ifdef IS_UNICODE
	INIT_ZVAL(XG(uvar_namespace_soft));
#endif
	INIT_ZVAL(XG(var_namespace_soft));
	xc_var_namespace_set_stringl("", 0 TSRMLS_CC);
}
/* }}} */
static void xc_var_namespace_destroy(TSRMLS_D) /* {{{ */
{
#ifdef IS_UNICODE
	zval_dtor(&XG(uvar_namespace_hard));
	zval_dtor(&XG(uvar_namespace_soft));
#endif
	zval_dtor(&XG(var_namespace_hard));
	zval_dtor(&XG(var_namespace_soft));
}
/* }}} */
static int xc_var_buffer_prepare(zval *name TSRMLS_DC) /* {{{ prepare name, calculate buffer size */
{
	int namespace_len;
	switch (name->type) {
#ifdef IS_UNICODE
		case IS_UNICODE:
do_unicode:
			namespace_len = Z_USTRLEN_P(&XG(uvar_namespace_soft));
			return (namespace_len ? namespace_len + 1 : 0) + Z_USTRLEN_P(name);
#endif

		case IS_STRING:
do_string:
			namespace_len = Z_STRLEN_P(&XG(var_namespace_soft));
			return (namespace_len ? namespace_len + 1 : 0) + Z_STRLEN_P(name);

		default:
#ifdef IS_UNICODE
			convert_to_unicode(name);
			goto do_unicode;
#else
			convert_to_string(name);
			goto do_string;
#endif
	}
}
/* }}} */
static int xc_var_buffer_alloca_size(zval *name TSRMLS_DC) /* {{{ prepare name, calculate buffer size */
{
	int namespace_len;
	switch (name->type) {
#ifdef IS_UNICODE
		case IS_UNICODE:
			namespace_len = Z_USTRLEN_P(&XG(uvar_namespace_soft));
			return !namespace_len ? 0 : (namespace_len + 1 + Z_USTRLEN_P(name) + 1) * sizeof(Z_USTRVAL_P(&XG(uvar_namespace_soft))[0]);
#endif

		case IS_STRING:
			namespace_len = Z_STRLEN_P(&XG(var_namespace_soft));
			return !namespace_len ? 0 : (namespace_len + 1 + Z_STRLEN_P(name) + 1);
	}
	assert(0);
	return 0;
}
/* }}} */
static void xc_var_buffer_init(char *buffer, zval *name TSRMLS_DC) /* {{{ prepare name, calculate buffer size */
{
#ifdef IS_UNICODE
	if (Z_TYPE(name) == IS_UNICODE) {
		memcpy(buffer, Z_USTRVAL_P(&XG(uvar_namespace_soft)), (Z_USTRLEN_P(&XG(uvar_namespace_soft)) + 1) * sizeof(Z_USTRVAL_P(name)[0]));
		buffer += (Z_USTRLEN_P(&XG(uvar_namespace_soft)) + 1) * sizeof(Z_USTRVAL_P(name)[0]);
		memcpy(buffer, Z_USTRVAL_P(name), (Z_USTRLEN_P(name) + 1) * sizeof(Z_USTRVAL_P(name)[0]));
	}
#endif
	memcpy(buffer, Z_STRVAL_P(&XG(var_namespace_soft)), (Z_STRLEN_P(&XG(var_namespace_soft)) + 1));
	buffer += (Z_STRLEN_P(&XG(var_namespace_soft)) + 1);
	memcpy(buffer, Z_STRVAL_P(name), (Z_STRLEN_P(name) + 1));
}
/* }}} */
typedef struct xc_namebuffer_t_ { /* {{{ */
	ALLOCA_FLAG(useheap)
	void *buffer;
	int alloca_size;
	int len;
} xc_namebuffer_t;
/* }}} */

#define VAR_BUFFER_FLAGS(name) \
	xc_namebuffer_t name##_buffer;

#define VAR_BUFFER_INIT(name) \
	name##_buffer.len = xc_var_buffer_prepare(name TSRMLS_CC); \
	name##_buffer.alloca_size = xc_var_buffer_alloca_size(name TSRMLS_CC); \
	name##_buffer.buffer = name##_buffer.alloca_size \
		? xc_do_alloca(name##_buffer.alloca_size, name##_buffer.useheap) \
		: UNISW(Z_STRVAL_P(name), Z_TYPE(name) == IS_UNICODE ? Z_USTRVAL_P(name) : Z_STRVAL_P(name)); \
	if (name##_buffer.alloca_size) xc_var_buffer_init(name##_buffer.buffer, name TSRMLS_CC);

#define VAR_BUFFER_FREE(name) \
	if (name##_buffer.alloca_size) { \
		xc_free_alloca(name##_buffer.buffer, name##_buffer.useheap); \
	}

static inline zend_bool xc_var_has_prefix(const xc_entry_t *entry, zval *prefix, const xc_namebuffer_t *prefix_buffer TSRMLS_DC) /* {{{ */
{
	zend_bool result = 0;

	if (UNISW(IS_STRING, entry->name_type) != prefix->type) {
		return 0;
	}

#ifdef IS_UNICODE
	if (Z_TYPE(prefix) == IS_UNICODE) {
		return result = entry->name.ustr.len >= prefix_buffer->len
		 && memcmp(entry->name.ustr.val, prefix_buffer->buffer, prefix_buffer->len * sizeof(Z_USTRVAL_P(prefix)[0])) == 0;
	}
#endif

	return result = entry->name.str.len >= prefix_buffer->len
	 && memcmp(entry->name.str.val, prefix_buffer->buffer, prefix_buffer->len) == 0;
}
/* }}} */

/* module helper function */
static int xc_init_constant(int module_number TSRMLS_DC) /* {{{ */
{
	zend_register_long_constant(XCACHE_STRS("XC_TYPE_PHP"), XC_TYPE_PHP, CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC);
	zend_register_long_constant(XCACHE_STRS("XC_TYPE_VAR"), XC_TYPE_VAR, CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC);
	return SUCCESS;
}
/* }}} */
static xc_shm_t *xc_cache_destroy(xc_cache_t *caches, xc_hash_t *hcache) /* {{{ */
{
	size_t i;
	xc_shm_t *shm = NULL;

	assert(caches);

	for (i = 0; i < hcache->size; i ++) {
		xc_cache_t *cache = &caches[i];
		if (cache) {
			/* do NOT touch cached data, do not release mutex shared inside cache */
			if (cache->mutex) {
				xc_mutex_destroy(cache->mutex);
			}
			shm = cache->shm;
			if (shm) {
				cache->shm->vtable->memdestroy(cache->allocator);
			}
		}
	}
	free(caches);
	return shm;
}
/* }}} */
static xc_cache_t *xc_cache_init(xc_shm_t *shm, const char *allocator_name, xc_hash_t *hcache, xc_hash_t *hentry, xc_hash_t *hphp, xc_shmsize_t shmsize) /* {{{ */
{
	xc_cache_t *caches = NULL;
	xc_allocator_t *allocator;
	time_t now = time(NULL);
	size_t i;
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

	CHECK(caches = calloc(hcache->size, sizeof(xc_cache_t)), "caches OOM");

	for (i = 0; i < hcache->size; i ++) {
		xc_cache_t *cache = &caches[i];
		CHECK(allocator = shm->vtable->meminit(shm, memsize), "Failed init shm");
		if (!(allocator->vtable = xc_allocator_find(allocator_name))) {
			zend_error(E_ERROR, "Allocator %s not found", allocator_name);
			goto err;
		}
		CHECK(allocator->vtable->init(shm, allocator, memsize), "Failed init allocator");
		CHECK(cache->cached           = allocator->vtable->calloc(allocator, 1, sizeof(xc_cached_t)), "create cache OOM");
		CHECK(cache->cached->entries  = allocator->vtable->calloc(allocator, hentry->size, sizeof(xc_entry_t*)), "create entries OOM");
		if (hphp) {
			CHECK(cache->cached->phps = allocator->vtable->calloc(allocator, hphp->size, sizeof(xc_entry_data_php_t*)), "create phps OOM");
		}
		CHECK(cache->mutex            = allocator->vtable->calloc(allocator, 1, xc_mutex_size()), "create lock OOM");
		CHECK(cache->mutex = xc_mutex_init(cache->mutex, NULL, 1), "can't create mutex");

		cache->hcache  = hcache;
		cache->hentry  = hentry;
		cache->hphp    = hphp;
		cache->shm     = shm;
		cache->allocator = allocator;
		cache->cacheid = i;
		cache->cached->last_gc_deletes = now;
		cache->cached->last_gc_expires = now;
	}
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
	TSRMLS_FETCH();

	if (old_compile_file && zend_compile_file == xc_compile_file) {
		zend_compile_file = old_compile_file;
		old_compile_file = NULL;
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

	xc_holds_destroy(TSRMLS_C);

	xc_initized = 0;
}
/* }}} */
static int xc_init() /* {{{ */
{
	xc_shm_t *shm = NULL;
	xc_shmsize_t shmsize = ALIGN(xc_php_size) + ALIGN(xc_var_size);

	xc_php_caches = xc_var_caches = NULL;

	if (shmsize < (size_t) xc_php_size || shmsize < (size_t) xc_var_size) {
		zend_error(E_ERROR, "XCache: neither xcache.size nor xcache.var_size can be negative");
		goto err;
	}

	if (xc_php_size || xc_var_size) {
		CHECK(shm = xc_shm_init(xc_shm_scheme, shmsize, xc_readonly_protection, xc_mmap_path, NULL), "Cannot create shm");
		if (!xc_shm_can_readonly(shm)) {
			xc_readonly_protection = 0;
		}

		if (xc_php_size) {
			CHECK(xc_php_caches = xc_cache_init(shm, xc_php_allocator, &xc_php_hcache, &xc_php_hentry, &xc_php_hentry, xc_php_size), "failed init opcode cache");
		}

		if (xc_var_size) {
			CHECK(xc_var_caches = xc_cache_init(shm, xc_var_allocator, &xc_var_hcache, &xc_var_hentry, NULL, xc_var_size), "failed init variable cache");
		}
	}
	return SUCCESS;

err:
	if (xc_php_caches || xc_var_caches) {
		xc_destroy();
		/* shm destroied in xc_destroy() */
	}
	else if (shm) {
		xc_destroy();
		xc_shm_destroy(shm);
		shm = NULL;
	}
	return FAILURE;
}
/* }}} */
static void xc_holds_init(TSRMLS_D) /* {{{ */
{
	size_t i;

#ifndef ZEND_WIN32
	XG(holds_pid) = getpid();
#endif

	if (xc_php_caches && !XG(php_holds)) {
		XG(php_holds_size) = xc_php_hcache.size;
		XG(php_holds) = calloc(XG(php_holds_size), sizeof(xc_vector_t));
		for (i = 0; i < xc_php_hcache.size; i ++) {
			xc_vector_init(xc_entry_php_t *, &XG(php_holds[i]), 1);
		}
	}

	if (xc_var_caches && !XG(var_holds)) {
		XG(var_holds_size) = xc_var_hcache.size;
		XG(var_holds) = calloc(XG(var_holds_size), sizeof(xc_vector_t));
		for (i = 0; i < xc_var_hcache.size; i ++) {
			xc_vector_init(xc_entry_php_t *, &XG(var_holds[i]), 1);
		}
	}
}
/* }}} */
static void xc_holds_destroy(TSRMLS_D) /* {{{ */
{
	size_t i;

	if (xc_php_caches && XG(php_holds)) {
		for (i = 0; i < XG(php_holds_size); i ++) {
			xc_vector_destroy(&XG(php_holds[i]));
		}
		free(XG(php_holds));
		XG(php_holds) = NULL;
		XG(php_holds_size) = 0;
	}

	if (xc_var_caches && XG(var_holds)) {
		for (i = 0; i < XG(var_holds_size); i ++) {
			xc_vector_destroy(&XG(var_holds[i]));
		}
		free(XG(var_holds));
		XG(var_holds) = NULL;
		XG(var_holds_size) = 0;
	}
}
/* }}} */
static void xc_request_init(TSRMLS_D) /* {{{ */
{
	if (!XG(internal_table_copied)) {
		zend_function tmp_func;
		xc_cest_t tmp_cest;

#ifdef HAVE_XCACHE_CONSTANT
		zend_hash_destroy(&XG(internal_constant_table));
#endif
		zend_hash_destroy(&XG(internal_function_table));
		zend_hash_destroy(&XG(internal_class_table));

#ifdef HAVE_XCACHE_CONSTANT
		zend_hash_init_ex(&XG(internal_constant_table), 20,  NULL, (dtor_func_t) xc_zend_constant_dtor, 1, 0);
#endif
		zend_hash_init_ex(&XG(internal_function_table), 100, NULL, NULL, 1, 0);
		zend_hash_init_ex(&XG(internal_class_table),    10,  NULL, NULL, 1, 0);

#ifdef HAVE_XCACHE_CONSTANT
		xc_copy_internal_zend_constants(&XG(internal_constant_table), EG(zend_constants));
#endif
		zend_hash_copy(&XG(internal_function_table), CG(function_table), NULL, &tmp_func, sizeof(tmp_func));
		zend_hash_copy(&XG(internal_class_table), CG(class_table), NULL, &tmp_cest, sizeof(tmp_cest));

		XG(internal_table_copied) = 1;
	}
	xc_holds_init(TSRMLS_C);
	xc_var_namespace_init(TSRMLS_C);
#ifdef ZEND_ENGINE_2
	zend_llist_init(&XG(gc_op_arrays), sizeof(xc_gc_op_array_t), xc_gc_op_array, 0);
#endif

#if PHP_API_VERSION <= 20041225
	XG(request_time) = time(NULL);
#else
	XG(request_time) = sapi_get_request_time(TSRMLS_C);
#endif
}
/* }}} */
static void xc_request_shutdown(TSRMLS_D) /* {{{ */
{
#ifndef ZEND_WIN32
	if (XG(holds_pid) == getpid())
#endif
	{
		xc_entry_unholds(TSRMLS_C);
	}
	xc_gc_expires_php(TSRMLS_C);
	xc_gc_expires_var(TSRMLS_C);
	xc_gc_deletes(TSRMLS_C);
	xc_var_namespace_destroy(TSRMLS_C);
#ifdef ZEND_ENGINE_2
	zend_llist_destroy(&XG(gc_op_arrays));
#endif
}
/* }}} */

/* user functions */
static zend_bool xcache_admin_auth_check(TSRMLS_D) /* {{{ */
{
	zval **server = NULL;
	zval **user = NULL;
	zval **pass = NULL;
	char *admin_user = NULL;
	char *admin_pass = NULL;
	HashTable *ht;

	/* auth disabled, nothing to do.. */
	if (!xc_admin_enable_auth) {
		return 1;
	}

	if (cfg_get_string("xcache.admin.user", &admin_user) == FAILURE || !admin_user[0]) {
		admin_user = NULL;
	}
	if (cfg_get_string("xcache.admin.pass", &admin_pass) == FAILURE || !admin_pass[0]) {
		admin_pass = NULL;
	}

	if (admin_user == NULL || admin_pass == NULL) {
		php_error_docref(XCACHE_WIKI_URL "/InstallAdministration" TSRMLS_CC, E_ERROR,
				"xcache.admin.user and/or xcache.admin.pass settings is not configured."
				" Make sure you've modified the correct php ini file for your php used in webserver.");
		zend_bailout();
	}
	if (strlen(admin_pass) != 32) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "xcache.admin.pass is %lu chars unexpectedly, it is supposed to be the password after md5() which should be 32 chars", (unsigned long) strlen(admin_pass));
		zend_bailout();
	}

#ifdef ZEND_ENGINE_2_1
	zend_is_auto_global("_SERVER", sizeof("_SERVER") - 1 TSRMLS_CC);
#endif
	if (zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &server) != SUCCESS || Z_TYPE_PP(server) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "_SERVER is corrupted");
		zend_bailout();
	}
	ht = Z_ARRVAL_P((*server));

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

#define STR "HTTP/1.0 401 Unauthorized"
	sapi_add_header_ex(STR, sizeof(STR) - 1, 1, 1 TSRMLS_CC);
#undef STR
#define STR "WWW-authenticate: Basic Realm=\"XCache Administration\""
	sapi_add_header_ex(STR, sizeof(STR) - 1, 1, 1 TSRMLS_CC);
#undef STR
#define STR "Content-type: text/html; charset=UTF-8"
	sapi_add_header_ex(STR, sizeof(STR) - 1, 1, 1 TSRMLS_CC);
#undef STR
	ZEND_PUTS("<html>\n");
	ZEND_PUTS("<head><title>XCache Authentication Failed</title></head>\n");
	ZEND_PUTS("<body>\n");
	ZEND_PUTS("<h1>XCache Authentication Failed</h1>\n");
	ZEND_PUTS("<p>You're not authorized to access this page due to wrong username and/or password you typed.<br />The following check points is suggested:</p>\n");
	ZEND_PUTS("<ul>\n");
	ZEND_PUTS("<li>Be aware that `Username' and `Password' is case sense. Check capslock status led on your keyboard, and punch left/right Shift keys once for each</li>\n");
	ZEND_PUTS("<li>Make sure the md5 password is generated correctly. You may use <a href=\"mkpassword.php\">mkpassword.php</a></li>\n");
	ZEND_PUTS("<li>Reload browser cache by pressing F5 and/or Ctrl+F5, or simply clear browser cache after you've updated username/password in php ini.</li>\n");
	ZEND_PUTS("</ul>\n");
	ZEND_PUTS("Check <a href=\"" XCACHE_WIKI_URL "/InstallAdministration\">XCache wiki page</a> for more information.\n");
	ZEND_PUTS("</body>\n");
	ZEND_PUTS("</html>\n");

	zend_bailout();
	return 0;
}
/* }}} */
static void xc_clear(long type, xc_cache_t *cache TSRMLS_DC) /* {{{ */
{
	xc_entry_t *e, *next;
	int entryslotid, c;

	ENTER_LOCK(cache) {
		for (entryslotid = 0, c = cache->hentry->size; entryslotid < c; entryslotid ++) {
			for (e = cache->cached->entries[entryslotid]; e; e = next) {
				next = e->next;
				xc_entry_remove_unlocked(type, cache, entryslotid, e TSRMLS_CC);
			}
			cache->cached->entries[entryslotid] = NULL;
		}
	} LEAVE_LOCK(cache);
} /* }}} */
/* {{{ xcache_admin_operate */
typedef enum { XC_OP_COUNT, XC_OP_INFO, XC_OP_LIST, XC_OP_CLEAR, XC_OP_ENABLE } xcache_op_type;
static void xcache_admin_operate(xcache_op_type optype, INTERNAL_FUNCTION_PARAMETERS)
{
	long type;
	long size;
	xc_cache_t *caches, *cache;
	long id = 0;
	zend_bool enable = 1;

	xcache_admin_auth_check(TSRMLS_C);

	if (!xc_initized) {
		RETURN_NULL();
	}

	switch (optype) {
		case XC_OP_COUNT:
			if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &type) == FAILURE) {
				return;
			}
			break;

		case XC_OP_CLEAR:
			id = -1;
			if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|l", &type, &id) == FAILURE) {
				return;
			}
			break;

		case XC_OP_ENABLE:
			id = -1;
			if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|lb", &type, &id, &enable) == FAILURE) {
				return;
			}
			break;

		default:
			if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &type, &id) == FAILURE) {
				return;
			}
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
			RETURN_LONG(caches ? size : 0)
			break;

		case XC_OP_INFO:
		case XC_OP_LIST:
			if (!caches || id < 0 || id >= size) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cache not exists");
				RETURN_FALSE;
			}

			array_init(return_value);

			cache = &caches[id];
			ENTER_LOCK(cache) {
				if (optype == XC_OP_INFO) {
					xc_fillinfo_unlocked(type, cache, return_value TSRMLS_CC);
				}
				else {
					xc_filllist_unlocked(type, cache, return_value TSRMLS_CC);
				}
			} LEAVE_LOCK(cache);
			break;

		case XC_OP_CLEAR:
			if (!caches || id < -1 || id >= size) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cache not exists");
				RETURN_FALSE;
			}

			if (id == -1) {
				for (id = 0; id < size; ++id) {
					xc_clear(type, &caches[id] TSRMLS_CC);
				}
			}
			else {
				xc_clear(type, &caches[id] TSRMLS_CC);
			}

			xc_gc_deletes(TSRMLS_C);
			break;

		case XC_OP_ENABLE:
			if (!caches || id < -1 || id >= size) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cache not exists");
				RETURN_FALSE;
			}

			if (id == -1) {
				for (id = 0; id < size; ++id) {
					caches[id].cached->disabled = !enable ? XG(request_time) : 0;
				}
			}
			else {
				caches[id].cached->disabled = !enable ? XG(request_time) : 0;
			}

			break;

		default:
			assert(0);
	}
}
/* }}} */
/* {{{ proto int xcache_count(int type)
   Return count of cache on specified cache type */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_count, 0, 0, 1)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_count[] = { 1, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_count)
{
	xcache_admin_operate(XC_OP_COUNT, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto array xcache_info(int type, int id)
   Get cache info by id on specified cache type */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_info, 0, 0, 2)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, id)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_info[] = { 2, BYREF_NONE, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_info)
{
	xcache_admin_operate(XC_OP_INFO, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto array xcache_list(int type, int id)
   Get cache entries list by id on specified cache type */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_list, 0, 0, 2)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, id)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_list[] = { 2, BYREF_NONE, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_list)
{
	xcache_admin_operate(XC_OP_LIST, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto array xcache_clear_cache(int type, [ int id = -1 ])
   Clear cache by id on specified cache type */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_clear_cache, 0, 0, 1)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, id)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_clear_cache[] = { 2, BYREF_NONE, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_clear_cache)
{
	xcache_admin_operate(XC_OP_CLEAR, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto array xcache_enable_cache(int type, [ int id = -1, [ bool enable = true ] ])
   Enable or disable cache by id on specified cache type */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_enable_cache, 0, 0, 1)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, id)
	ZEND_ARG_INFO(0, enable)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_enable_cache[] = { 1, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_enable_cache)
{
	xcache_admin_operate(XC_OP_ENABLE, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto mixed xcache_admin_namespace()
   Break out of namespace limitation */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_admin_namespace, 0, 0, 0)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_admin_namespace[] = { 0 };
#endif

PHP_FUNCTION(xcache_admin_namespace)
{
	xcache_admin_auth_check(TSRMLS_C);
	xc_var_namespace_break(TSRMLS_C);
}
/* }}} */

#define VAR_CACHE_NOT_INITIALIZED() do { \
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "XCache var cache was not initialized properly. Check php log for actual reason"); \
} while (0)

static int xc_entry_var_init_key(xc_entry_var_t *entry_var, xc_entry_hash_t *entry_hash, xc_namebuffer_t *name_buffer TSRMLS_DC) /* {{{ */
{
	xc_hash_value_t hv;

#ifdef IS_UNICODE
	entry_var->name_type = name->type;
#endif
	entry_var->entry.name.str.val = name_buffer->buffer;
	entry_var->entry.name.str.len = name_buffer->len;

	hv = xc_entry_hash_var((xc_entry_t *) entry_var TSRMLS_CC);

	entry_hash->cacheid = (hv & xc_var_hcache.mask);
	hv >>= xc_var_hcache.bits;
	entry_hash->entryslotid = (hv & xc_var_hentry.mask);
	return SUCCESS;
}
/* }}} */
/* {{{ proto mixed xcache_set_namespace(string namespace)
   Switch to user defined namespace */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_set_namespace, 0, 0, 1)
	ZEND_ARG_INFO(0, namespace)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_set_namespace[] = { 1, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_set_namespace)
{
	zval *namespace;

	if (!xc_var_caches) {
		VAR_CACHE_NOT_INITIALIZED();
		RETURN_NULL();
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &namespace) == FAILURE) {
		return;
	}

	if (Z_TYPE_P(namespace) == IS_STRING) {
		xc_var_namespace_set_stringl(Z_STRVAL_P(namespace), Z_STRLEN_P(namespace) TSRMLS_CC);
	}
#ifdef IS_UNICODE
	else if (Z_TYPE_P(namespace) == IS_UNICODE) {
		xc_var_namespace_set_unicodel(Z_USTRVAL_P(namespace), Z_USTRLEN_P(namespace) TSRMLS_CC);
	}
#endif
}
/* }}} */
/* {{{ proto mixed xcache_get(string name)
   Get cached data by specified name */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_get, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_get[] = { 1, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_get)
{
	xc_entry_hash_t entry_hash;
	xc_cache_t *cache;
	xc_entry_var_t entry_var, *stored_entry_var;
	zval *name;
	xc_vector_t index_to_ce = xc_vector_initializer(zend_class_entry *, 0);
	xc_vector_t pending_class_names = xc_vector_initializer(xc_constant_string_t, 0);
	zend_bool reload_class;
	VAR_BUFFER_FLAGS(name);

	if (!xc_var_caches) {
		VAR_CACHE_NOT_INITIALIZED();
		RETURN_NULL();
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &name) == FAILURE) {
		return;
	}
	VAR_BUFFER_INIT(name);
	xc_entry_var_init_key(&entry_var, &entry_hash, &name_buffer TSRMLS_CC);
	cache = &xc_var_caches[entry_hash.cacheid];

	if (cache->cached->disabled) {
		VAR_BUFFER_FREE(name);
		RETURN_NULL();
	}

	do {
		reload_class = 0;
		if (xc_vector_size(&pending_class_names)) {
			size_t i, end = xc_vector_size(&pending_class_names);
			assert(end != 0);
			for (i = 0; i < end; ++i) {
				xc_constant_string_t *pending_class_name = &xc_vector_data(xc_constant_string_t, &pending_class_names)[i];
				zend_class_entry *ce = xc_lookup_class(pending_class_name->str, pending_class_name->len, 1 TSRMLS_CC);
				if (!ce) {
					php_error_docref(NULL TSRMLS_CC, E_ERROR, "Class %s not found when restroing variable", pending_class_name->str);
					break;
				}
			}
			if (i != end) {
				break;
			}
			xc_vector_clear(&pending_class_names);
		}

		ENTER_LOCK(cache) {
			stored_entry_var = (xc_entry_var_t *) xc_entry_find_unlocked(XC_TYPE_VAR, cache, entry_hash.entryslotid, (xc_entry_t *) &entry_var TSRMLS_CC);
			if (!stored_entry_var) {
				RETVAL_NULL();
				break;
			}

			if (stored_entry_var->class_names_count) {
				zend_uint i;
				/* see if lucky to have all classes needed already loaded */
				for (i = 0; i < stored_entry_var->class_names_count; ++i) {
					xc_constant_string_t *class_name = &stored_entry_var->class_names[i];
					zend_class_entry *ce = xc_lookup_class(class_name->str, class_name->len, 0 TSRMLS_CC);

					/* not found, add to pending */
					if (!ce) {
						xc_constant_string_t pending_class_name;
						pending_class_name.str = estrndup(class_name->str, class_name->len);
						pending_class_name.len = class_name->len;
						xc_vector_push_back(&pending_class_names, &pending_class_name);
						reload_class = 1;
					}
					else if (!reload_class) {
						xc_vector_push_back(&index_to_ce, &ce);
					}
				}

				if (reload_class) {
					/* not all loaded, load it after unload and run another pass */
					xc_vector_clear(&index_to_ce);
					break;
				}
			}

			xc_processor_restore_var(return_value, stored_entry_var, xc_vector_data(zend_class_entry *, &index_to_ce) TSRMLS_CC);
			xc_cached_hit_unlocked(cache->cached TSRMLS_CC);
		} LEAVE_LOCK(cache);
	} while (reload_class);

	if (xc_vector_size(&pending_class_names)) {
		size_t i;
		for (i = 0; i < xc_vector_size(&pending_class_names); ++i) {
			efree(xc_vector_data(xc_constant_string_t, &pending_class_names)[i].str);
		}
	}
	xc_vector_destroy(&pending_class_names);
	xc_vector_destroy(&index_to_ce);

	VAR_BUFFER_FREE(name);
}
/* }}} */
/* {{{ proto bool  xcache_set(string name, mixed value [, int ttl])
   Store data to cache by specified name */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_set, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_set[] = { 3, BYREF_NONE, BYREF_NONE, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_set)
{
	xc_entry_hash_t entry_hash;
	xc_cache_t *cache;
	xc_entry_var_t entry_var, *stored_entry_var;
	zval *name;
	zval *value;
	VAR_BUFFER_FLAGS(name);

	if (!xc_var_caches) {
		VAR_CACHE_NOT_INITIALIZED();
		RETURN_NULL();
	}

	entry_var.entry.ttl = XG(var_ttl);
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|l", &name, &value, &entry_var.entry.ttl) == FAILURE) {
		return;
	}

	/* max ttl */
	if (xc_var_maxttl && (!entry_var.entry.ttl || entry_var.entry.ttl > xc_var_maxttl)) {
		entry_var.entry.ttl = xc_var_maxttl;
	}

	VAR_BUFFER_INIT(name);
	xc_entry_var_init_key(&entry_var, &entry_hash, &name_buffer TSRMLS_CC);
	cache = &xc_var_caches[entry_hash.cacheid];

	if (cache->cached->disabled) {
		VAR_BUFFER_FREE(name);
		RETURN_NULL();
	}

	ENTER_LOCK(cache) {
		stored_entry_var = (xc_entry_var_t *) xc_entry_find_unlocked(XC_TYPE_VAR, cache, entry_hash.entryslotid, (xc_entry_t *) &entry_var TSRMLS_CC);
		if (stored_entry_var) {
			xc_entry_remove_unlocked(XC_TYPE_VAR, cache, entry_hash.entryslotid, (xc_entry_t *) stored_entry_var TSRMLS_CC);
		}
		entry_var.value = value;
		RETVAL_BOOL(xc_entry_var_store_unlocked(cache, entry_hash.entryslotid, &entry_var TSRMLS_CC) != NULL ? 1 : 0);
	} LEAVE_LOCK(cache);
	VAR_BUFFER_FREE(name);
}
/* }}} */
/* {{{ proto bool  xcache_isset(string name)
   Check if an entry exists in cache by specified name */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_isset, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_isset[] = { 1, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_isset)
{
	xc_entry_hash_t entry_hash;
	xc_cache_t *cache;
	xc_entry_var_t entry_var, *stored_entry_var;
	zval *name;
	VAR_BUFFER_FLAGS(name);

	if (!xc_var_caches) {
		VAR_CACHE_NOT_INITIALIZED();
		RETURN_FALSE;
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &name) == FAILURE) {
		return;
	}
	VAR_BUFFER_INIT(name);
	xc_entry_var_init_key(&entry_var, &entry_hash, &name_buffer TSRMLS_CC);
	cache = &xc_var_caches[entry_hash.cacheid];

	if (cache->cached->disabled) {
		VAR_BUFFER_FREE(name);
		RETURN_FALSE;
	}

	ENTER_LOCK(cache) {
		stored_entry_var = (xc_entry_var_t *) xc_entry_find_unlocked(XC_TYPE_VAR, cache, entry_hash.entryslotid, (xc_entry_t *) &entry_var TSRMLS_CC);
		if (stored_entry_var) {
			xc_cached_hit_unlocked(cache->cached TSRMLS_CC);
			RETVAL_TRUE;
			/* return */
		}
		else {
			RETVAL_FALSE;
		}

	} LEAVE_LOCK(cache);
	VAR_BUFFER_FREE(name);
}
/* }}} */
/* {{{ proto bool  xcache_unset(string name)
   Unset existing data in cache by specified name */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_unset, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_unset[] = { 1, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_unset)
{
	xc_entry_hash_t entry_hash;
	xc_cache_t *cache;
	xc_entry_var_t entry_var, *stored_entry_var;
	zval *name;
	VAR_BUFFER_FLAGS(name);

	if (!xc_var_caches) {
		VAR_CACHE_NOT_INITIALIZED();
		RETURN_FALSE;
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &name) == FAILURE) {
		return;
	}
	VAR_BUFFER_INIT(name);
	xc_entry_var_init_key(&entry_var, &entry_hash, &name_buffer TSRMLS_CC);
	cache = &xc_var_caches[entry_hash.cacheid];

	if (cache->cached->disabled) {
		VAR_BUFFER_FREE(name);
		RETURN_FALSE;
	}

	ENTER_LOCK(cache) {
		stored_entry_var = (xc_entry_var_t *) xc_entry_find_unlocked(XC_TYPE_VAR, cache, entry_hash.entryslotid, (xc_entry_t *) &entry_var TSRMLS_CC);
		if (stored_entry_var) {
			xc_entry_remove_unlocked(XC_TYPE_VAR, cache, entry_hash.entryslotid, (xc_entry_t *) stored_entry_var TSRMLS_CC);
			RETVAL_TRUE;
		}
		else {
			RETVAL_FALSE;
		}
	} LEAVE_LOCK(cache);
	VAR_BUFFER_FREE(name);
}
/* }}} */
/* {{{ proto bool  xcache_unset_by_prefix(string prefix)
   Unset existing data in cache by specified prefix */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_unset_by_prefix, 0, 0, 1)
	ZEND_ARG_INFO(0, prefix)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_unset_by_prefix[] = { 1, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_unset_by_prefix)
{
	zval *prefix;
	int i, iend;
	VAR_BUFFER_FLAGS(prefix);

	if (!xc_var_caches) {
		VAR_CACHE_NOT_INITIALIZED();
		RETURN_FALSE;
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &prefix) == FAILURE) {
		return;
	}

	VAR_BUFFER_INIT(prefix);
	for (i = 0, iend = xc_var_hcache.size; i < iend; i ++) {
		xc_cache_t *cache = &xc_var_caches[i];
		if (cache->cached->disabled) {
			continue;
		}

		ENTER_LOCK(cache) {
			int entryslotid, jend;
			for (entryslotid = 0, jend = cache->hentry->size; entryslotid < jend; entryslotid ++) {
				xc_entry_t *entry, *next;
				for (entry = cache->cached->entries[entryslotid]; entry; entry = next) {
					next = entry->next;
					if (xc_var_has_prefix(entry, prefix, &prefix_buffer TSRMLS_CC)) {
						xc_entry_remove_unlocked(XC_TYPE_VAR, cache, entryslotid, entry TSRMLS_CC);
					}
				}
			}
		} LEAVE_LOCK(cache);
	}
	VAR_BUFFER_FREE(prefix);
}
/* }}} */
static inline void xc_var_inc_dec(int inc, INTERNAL_FUNCTION_PARAMETERS) /* {{{ */
{
	xc_entry_hash_t entry_hash;
	xc_cache_t *cache;
	xc_entry_var_t entry_var, *stored_entry_var;
	zval *name;
	long count = 1;
	long value = 0;
	zval oldzval;
	VAR_BUFFER_FLAGS(name);

	if (!xc_var_caches) {
		VAR_CACHE_NOT_INITIALIZED();
		RETURN_NULL();
	}

	entry_var.entry.ttl = XG(var_ttl);
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|ll", &name, &count, &entry_var.entry.ttl) == FAILURE) {
		return;
	}

	/* max ttl */
	if (xc_var_maxttl && (!entry_var.entry.ttl || entry_var.entry.ttl > xc_var_maxttl)) {
		entry_var.entry.ttl = xc_var_maxttl;
	}

	VAR_BUFFER_INIT(name);
	xc_entry_var_init_key(&entry_var, &entry_hash, &name_buffer TSRMLS_CC);
	cache = &xc_var_caches[entry_hash.cacheid];

	if (cache->cached->disabled) {
		VAR_BUFFER_FREE(name);
		RETURN_NULL();
	}

	ENTER_LOCK(cache) {
		stored_entry_var = (xc_entry_var_t *) xc_entry_find_unlocked(XC_TYPE_VAR, cache, entry_hash.entryslotid, (xc_entry_t *) &entry_var TSRMLS_CC);
		if (stored_entry_var) {
			TRACE("incdec: got entry_var %s", entry_var.entry.name.str.val);
			/* do it in place */
			if (Z_TYPE_P(stored_entry_var->value) == IS_LONG) {
				zval *zv;
				stored_entry_var->entry.ctime = XG(request_time);
				stored_entry_var->entry.ttl   = entry_var.entry.ttl;
				TRACE("%s", "incdec: islong");
				value = Z_LVAL_P(stored_entry_var->value);
				value += (inc == 1 ? count : - count);
				RETVAL_LONG(value);

				zv = (zval *) xc_shm_to_readwrite(cache->shm, (char *) stored_entry_var->value);
				Z_LVAL_P(zv) = value;
				++cache->cached->updates;
				break; /* leave lock */
			}

			TRACE("%s", "incdec: notlong");
			if (stored_entry_var->objects_count) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cannot convert object to integer");
				value = 0;
			}
			else {
				xc_processor_restore_var(&oldzval, stored_entry_var, NULL TSRMLS_CC);
				convert_to_long(&oldzval);
				value = Z_LVAL(oldzval);
				zval_dtor(&oldzval);
			}
		}
		else {
			TRACE("incdec: %s not found", entry_var.entry.name.str.val);
		}

		value += (inc == 1 ? count : - count);
		RETVAL_LONG(value);
		entry_var.value = return_value;

		if (stored_entry_var) {
			entry_var.entry.atime = stored_entry_var->entry.atime;
			entry_var.entry.ctime = stored_entry_var->entry.ctime;
			entry_var.entry.hits  = stored_entry_var->entry.hits;
			xc_entry_remove_unlocked(XC_TYPE_VAR, cache, entry_hash.entryslotid, (xc_entry_t *) stored_entry_var TSRMLS_CC);
		}
		xc_entry_var_store_unlocked(cache, entry_hash.entryslotid, &entry_var TSRMLS_CC);
	} LEAVE_LOCK(cache);
	VAR_BUFFER_FREE(name);
}
/* }}} */
/* {{{ proto int xcache_inc(string name [, int value [, int ttl]])
   Increase an int counter in cache by specified name, create it if not exists */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_inc, 0, 0, 1)
	ZEND_ARG_INFO(0, prefix)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_inc[] = { 3, BYREF_NONE, BYREF_NONE, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_inc)
{
	xc_var_inc_dec(1, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
/* {{{ proto int xcache_dec(string name [, int value [, int ttl]])
   Decrease an int counter in cache by specified name, create it if not exists */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_dec, 0, 0, 1)
	ZEND_ARG_INFO(0, prefix)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_dec[] = { 3, BYREF_NONE, BYREF_NONE, BYREF_NONE };
#endif

PHP_FUNCTION(xcache_dec)
{
	xc_var_inc_dec(-1, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */
static zend_function_entry xcache_cacher_functions[] = /* {{{ */
{
	PHP_FE(xcache_count,             arginfo_xcache_count)
	PHP_FE(xcache_info,              arginfo_xcache_info)
	PHP_FE(xcache_list,              arginfo_xcache_list)
	PHP_FE(xcache_clear_cache,       arginfo_xcache_clear_cache)
	PHP_FE(xcache_enable_cache,      arginfo_xcache_enable_cache)
	PHP_FE(xcache_admin_namespace,   arginfo_xcache_admin_namespace)
	PHP_FE(xcache_set_namespace,     arginfo_xcache_set_namespace)
	PHP_FE(xcache_get,               arginfo_xcache_get)
	PHP_FE(xcache_set,               arginfo_xcache_set)
	PHP_FE(xcache_isset,             arginfo_xcache_isset)
	PHP_FE(xcache_unset,             arginfo_xcache_unset)
	PHP_FE(xcache_unset_by_prefix,   arginfo_xcache_unset_by_prefix)
	PHP_FE(xcache_inc,               arginfo_xcache_inc)
	PHP_FE(xcache_dec,               arginfo_xcache_dec)
	PHP_FE_END
};
/* }}} */

static int xc_cacher_zend_startup(zend_extension *extension) /* {{{ */
{
	if ((xc_php_size || xc_var_size) && xc_mmap_path && xc_mmap_path[0]) {
		if (xc_init() != SUCCESS) {
			zend_error(E_ERROR, "XCache: Cannot init");
			return FAILURE;
		}
		xc_initized = 1;
		xc_init_time = time(NULL);
		xc_init_instance_id = getpid();
#ifdef ZTS
		xc_init_instance_subid = tsrm_thread_id();
#endif
	}

	if (xc_php_size) {
		old_compile_file = zend_compile_file;
		zend_compile_file = xc_compile_file;
	}

	return SUCCESS;
}
/* }}} */
static void xc_cacher_zend_shutdown(zend_extension *extension) /* {{{ */
{
	if (xc_initized) {
		xc_destroy();
	}
}
/* }}} */
/* {{{ zend extension definition structure */
static zend_extension xc_cacher_zend_extension_entry = {
	XCACHE_NAME " Cacher",
	XCACHE_VERSION,
	XCACHE_AUTHOR,
	XCACHE_URL,
	XCACHE_COPYRIGHT,
	xc_cacher_zend_startup,
	xc_cacher_zend_shutdown,
	NULL,           /* activate_func_t */
	NULL,           /* deactivate_func_t */
	NULL,           /* message_handler_func_t */
	NULL,           /* op_array_handler_func_t */
	NULL,           /* statement_handler_func_t */
	NULL,           /* fcall_begin_handler_func_t */
	NULL,           /* fcall_end_handler_func_t */
	NULL,           /* op_array_ctor_func_t */
	NULL,           /* op_array_dtor_func_t */
	STANDARD_ZEND_EXTENSION_PROPERTIES
};
/* }}} */

/* {{{ ini */
#ifdef ZEND_WIN32
#	define DEFAULT_PATH "xcache"
#else
#	define DEFAULT_PATH "/dev/zero"
#endif
PHP_INI_BEGIN()
	PHP_INI_ENTRY1     ("xcache.shm_scheme",          "mmap", PHP_INI_SYSTEM, xcache_OnUpdateString,   &xc_shm_scheme)
	PHP_INI_ENTRY1     ("xcache.mmap_path",     DEFAULT_PATH, PHP_INI_SYSTEM, xcache_OnUpdateString,   &xc_mmap_path)
	PHP_INI_ENTRY1_EX  ("xcache.readonly_protection",    "0", PHP_INI_SYSTEM, xcache_OnUpdateBool,     &xc_readonly_protection, zend_ini_boolean_displayer_cb)
	/* opcode cache */
	PHP_INI_ENTRY1_EX  ("xcache.admin.enable_auth",      "1", PHP_INI_SYSTEM, xcache_OnUpdateBool,     &xc_admin_enable_auth,   zend_ini_boolean_displayer_cb)
	PHP_INI_ENTRY1     ("xcache.size",                   "0", PHP_INI_SYSTEM, xcache_OnUpdateDummy,    NULL)
	PHP_INI_ENTRY1     ("xcache.count",                  "1", PHP_INI_SYSTEM, xcache_OnUpdateDummy,    NULL)
	PHP_INI_ENTRY1     ("xcache.slots",                 "8K", PHP_INI_SYSTEM, xcache_OnUpdateDummy,    NULL)
	PHP_INI_ENTRY1     ("xcache.allocator",        "bestfit", PHP_INI_SYSTEM, xcache_OnUpdateString,   &xc_php_allocator)
	PHP_INI_ENTRY1     ("xcache.ttl",                    "0", PHP_INI_SYSTEM, xcache_OnUpdateULong,    &xc_php_ttl)
	PHP_INI_ENTRY1     ("xcache.gc_interval",            "0", PHP_INI_SYSTEM, xcache_OnUpdateULong,    &xc_php_gc_interval)
	STD_PHP_INI_BOOLEAN("xcache.cacher",                 "1", PHP_INI_ALL,    OnUpdateBool,    cacher, zend_xcache_globals, xcache_globals)
	STD_PHP_INI_BOOLEAN("xcache.stat",                   "1", PHP_INI_ALL,    OnUpdateBool,    stat,   zend_xcache_globals, xcache_globals)
	/* var cache */
	PHP_INI_ENTRY1     ("xcache.var_size",               "0", PHP_INI_SYSTEM, xcache_OnUpdateDummy,    NULL)
	PHP_INI_ENTRY1     ("xcache.var_count",              "1", PHP_INI_SYSTEM, xcache_OnUpdateDummy,    NULL)
	PHP_INI_ENTRY1     ("xcache.var_slots",             "8K", PHP_INI_SYSTEM, xcache_OnUpdateDummy,    NULL)
	PHP_INI_ENTRY1     ("xcache.var_maxttl",             "0", PHP_INI_SYSTEM, xcache_OnUpdateULong,    &xc_var_maxttl)
	PHP_INI_ENTRY1     ("xcache.var_gc_interval",      "120", PHP_INI_SYSTEM, xcache_OnUpdateULong,    &xc_var_gc_interval)
	PHP_INI_ENTRY1     ("xcache.var_allocator",    "bestfit", PHP_INI_SYSTEM, xcache_OnUpdateString,   &xc_var_allocator)
	STD_PHP_INI_ENTRY  ("xcache.var_ttl",                "0", PHP_INI_ALL,    OnUpdateLong, var_ttl,   zend_xcache_globals, xcache_globals)
	PHP_INI_ENTRY1     ("xcache.var_namespace_mode",     "0", PHP_INI_SYSTEM, xcache_OnUpdateULong,    &xc_var_namespace_mode)
	PHP_INI_ENTRY1     ("xcache.var_namespace",           "", PHP_INI_SYSTEM, xcache_OnUpdateString,   &xc_var_namespace)
PHP_INI_END()
/* }}} */
static PHP_MINFO_FUNCTION(xcache_cacher) /* {{{ */
{
	char buf[100];
	char *ptr;
	int left, len;
	xc_shm_scheme_t *scheme;

	php_info_print_table_start();
	php_info_print_table_row(2, "XCache Cacher Module", "enabled");
	php_info_print_table_row(2, "Readonly Protection", xc_readonly_protection ? "enabled" : "disabled");
#ifdef ZEND_ENGINE_2_1
	ptr = php_format_date("Y-m-d H:i:s", sizeof("Y-m-d H:i:s") - 1, XG(request_time), 1 TSRMLS_CC);
	php_info_print_table_row(2, "Page Request Time", ptr);
	efree(ptr);

	ptr = php_format_date("Y-m-d H:i:s", sizeof("Y-m-d H:i:s") - 1, xc_init_time, 1 TSRMLS_CC);
	php_info_print_table_row(2, "Cache Init Time", ptr);
	efree(ptr);
#else
	snprintf(buf, sizeof(buf), "%lu", (long unsigned) XG(request_time));
	php_info_print_table_row(2, "Page Request Time", buf);

	snprintf(buf, sizeof(buf), "%lu", (long unsigned) xc_init_time);
	php_info_print_table_row(2, "Cache Init Time", buf);
#endif

#ifdef ZTS
	snprintf(buf, sizeof(buf), "%lu.%lu", xc_init_instance_id, xc_init_instance_subid);
#else
	snprintf(buf, sizeof(buf), "%lu", xc_init_instance_id);
#endif
	php_info_print_table_row(2, "Cache Instance Id", buf);

	if (xc_php_size) {
		ptr = _php_math_number_format(xc_php_size, 0, '.', ',');
		snprintf(buf, sizeof(buf), "enabled, %s bytes, %lu split(s), with %lu slots each", ptr, (unsigned long) xc_php_hcache.size, xc_php_hentry.size);
		php_info_print_table_row(2, "Opcode Cache", buf);
		efree(ptr);
	}
	else {
		php_info_print_table_row(2, "Opcode Cache", "disabled");
	}
	if (xc_var_size) {
		ptr = _php_math_number_format(xc_var_size, 0, '.', ',');
		snprintf(buf, sizeof(buf), "enabled, %s bytes, %lu split(s), with %lu slots each", ptr, (unsigned long) xc_var_hcache.size, xc_var_hentry.size);
		php_info_print_table_row(2, "Variable Cache", buf);
		efree(ptr);
	}
	else {
		php_info_print_table_row(2, "Variable Cache", "disabled");
	}

	left = sizeof(buf);
	ptr = buf;
	buf[0] = '\0';
	for (scheme = xc_shm_scheme_first(); scheme; scheme = xc_shm_scheme_next(scheme)) {
		len = snprintf(ptr, left, ptr == buf ? "%s" : ", %s", xc_shm_scheme_name(scheme));
		left -= len;
		ptr += len;
	}
	php_info_print_table_row(2, "Shared Memory Schemes", buf);

	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */
static int xc_config_hash(xc_hash_t *p, char *name, char *default_value) /* {{{ */
{
	size_t bits, size;
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

	*p = zend_atol(value, strlen(value));
	return SUCCESS;
}
/* }}} */
static PHP_MINIT_FUNCTION(xcache_cacher) /* {{{ */
{
	zend_extension *ext;
	zend_llist_position lpos;

	ext = zend_get_extension("Zend Optimizer");
	if (ext) {
		char *value;
		if (cfg_get_string("zend_optimizer.optimization_level", &value) == SUCCESS && zend_atol(value, strlen(value)) > 0) {
			zend_error(E_NOTICE, "Zend Optimizer with zend_optimizer.optimization_level>0 is not compatible with other cacher, disabling");
		}
		ext->op_array_handler = NULL;
	}

	ext = zend_get_extension("Zend OPcache");
	if (ext) {
		char *value;
		if (cfg_get_string("opcache.optimization_level", &value) == SUCCESS && zend_atol(value, strlen(value)) > 0) {
			zend_error(E_WARNING, "Constant folding feature in Zend OPcache is not compatible with XCache's __DIR__ handling, please set opcache.optimization_level=0 or disable Zend OPcache");
		}
	}

	/* cache if there's an op_array_ctor */
	for (ext = zend_llist_get_first_ex(&zend_extensions, &lpos);
			ext;
			ext = zend_llist_get_next_ex(&zend_extensions, &lpos)) {
		if (ext->op_array_ctor) {
			xc_have_op_array_ctor = 1;
			break;
		}
	}

	xc_config_long(&xc_php_size,       "xcache.size",        "0");
	xc_config_hash(&xc_php_hcache,     "xcache.count",       "1");
	xc_config_hash(&xc_php_hentry,     "xcache.slots",      "8K");

	xc_config_long(&xc_var_size,       "xcache.var_size",    "0");
	xc_config_hash(&xc_var_hcache,     "xcache.var_count",   "1");
	xc_config_hash(&xc_var_hentry,     "xcache.var_slots",  "8K");

	if (strcmp(sapi_module.name, "cli") == 0) {
		if (!xc_test) {
			/* disable cache for cli except for testing */
			xc_php_size = 0;
		}
	}

	if (xc_php_size <= 0) {
		xc_php_size = xc_php_hcache.size = 0;
	}
	if (xc_var_size <= 0) {
		xc_var_size = xc_var_hcache.size = 0;
	}

	xc_init_constant(module_number TSRMLS_CC);

	REGISTER_INI_ENTRIES();

	xc_sandbox_module_init(module_number TSRMLS_CC);
	return xcache_zend_extension_add(&xc_cacher_zend_extension_entry, 0);
}
/* }}} */
static PHP_MSHUTDOWN_FUNCTION(xcache_cacher) /* {{{ */
{
	xc_sandbox_module_shutdown();

	xcache_zend_extension_remove(&xc_cacher_zend_extension_entry);
	UNREGISTER_INI_ENTRIES();

	if (xc_mmap_path) {
		pefree(xc_mmap_path, 1);
		xc_mmap_path = NULL;
	}
	if (xc_shm_scheme) {
		pefree(xc_shm_scheme, 1);
		xc_shm_scheme = NULL;
	}
	if (xc_php_allocator) {
		pefree(xc_php_allocator, 1);
		xc_php_allocator = NULL;
	}
	if (xc_var_allocator) {
		pefree(xc_var_allocator, 1);
		xc_var_allocator = NULL;
	}
	if (xc_var_namespace) {
		pefree(xc_var_namespace, 1);
		xc_var_namespace = NULL;
	}

	return SUCCESS;
}
/* }}} */
static PHP_RINIT_FUNCTION(xcache_cacher) /* {{{ */
{
	xc_request_init(TSRMLS_C);
	return SUCCESS;
}
/* }}} */
/* {{{ static PHP_RSHUTDOWN_FUNCTION(xcache_cacher) */
#ifndef ZEND_ENGINE_2
static PHP_RSHUTDOWN_FUNCTION(xcache_cacher)
#else
static ZEND_MODULE_POST_ZEND_DEACTIVATE_D(xcache_cacher)
#endif
{
#ifdef ZEND_ENGINE_2
	TSRMLS_FETCH();
#endif

	xc_request_shutdown(TSRMLS_C);
	return SUCCESS;
}
/* }}} */
static zend_module_entry xcache_cacher_module_entry = { /* {{{ */
	STANDARD_MODULE_HEADER,
	XCACHE_NAME " Cacher",
	xcache_cacher_functions,
	PHP_MINIT(xcache_cacher),
	PHP_MSHUTDOWN(xcache_cacher),
	PHP_RINIT(xcache_cacher),
#ifndef ZEND_ENGINE_2
	PHP_RSHUTDOWN(xcache_cacher),
#else
	NULL,
#endif
	PHP_MINFO(xcache_cacher),
	XCACHE_VERSION,
#ifdef PHP_GINIT
	NO_MODULE_GLOBALS,
#endif
#ifdef ZEND_ENGINE_2
	ZEND_MODULE_POST_ZEND_DEACTIVATE_N(xcache_cacher),
#else
	NULL,
	NULL,
#endif
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */
int xc_cacher_startup_module() /* {{{ */
{
	return zend_startup_module(&xcache_cacher_module_entry);
}
/* }}} */
void xc_cacher_disable() /* {{{ */
{
	time_t now = time(NULL);
	size_t i;

	if (xc_php_caches) {
		for (i = 0; i < xc_php_hcache.size; i ++) {
			if (xc_php_caches[i].cached) {
				xc_php_caches[i].cached->disabled = now;
			}
		}
	}

	if (xc_var_caches) {
		for (i = 0; i < xc_var_hcache.size; i ++) {
			if (xc_var_caches[i].cached) {
				xc_var_caches[i].cached->disabled = now;
			}
		}
	}
}
/* }}} */
