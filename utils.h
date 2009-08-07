#include "php.h"
#include "xcache.h"

#ifdef XCACHE_DEBUG
#	define IFDEBUG(x) (x)
int xc_vtrace(const char *fmt, va_list args);
int xc_trace(const char *fmt, ...) ZEND_ATTRIBUTE_PTR_FORMAT(printf, 1, 2);

#	ifdef ZEND_WIN32
static inline int TRACE(const char *fmt, ...) 
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = xc_vtrace(fmt, args);
	va_end(args);
	return ret;
}
#	else
#		define TRACE(fmt, ...) \
		xc_trace("%s:%d: " fmt "\r\n", __FILE__, __LINE__, __VA_ARGS__)
#	endif /* ZEND_WIN32 */
#   undef NDEBUG
#   undef inline
#   define inline
#else /* XCACHE_DEBUG */

#	ifdef ZEND_WIN32
static inline int TRACE_DUMMY(const char *fmt, ...)
{
	return 0;
}
#		define TRACE 1 ? 0 : TRACE_DUMMY
#	else
#		define TRACE(fmt, ...) do { } while (0)
#	endif /* ZEND_WIN32 */

#	define IFDEBUG(x) do { } while (0)
#endif /* XCACHE_DEBUG */
#include <assert.h>

typedef struct {
	int alloc;
	zend_op_array *op_array;
	HashTable *function_table;
	HashTable *class_table;
} xc_compile_result_t;

xc_compile_result_t *xc_compile_result_init(xc_compile_result_t *cr,
		zend_op_array *op_array,
		HashTable *function_table,
		HashTable *class_table);
void xc_compile_result_free(xc_compile_result_t *cr);
xc_compile_result_t *xc_compile_result_init_cur(xc_compile_result_t *cr, zend_op_array *op_array TSRMLS_DC);
/* apply func */
int xc_apply_function(zend_function *zf, apply_func_t applyer TSRMLS_DC);
int xc_apply_class(zend_class_entry *ce, apply_func_t applyer TSRMLS_DC);
int xc_apply_op_array(xc_compile_result_t *cr, apply_func_t applyer TSRMLS_DC);

int xc_undo_pass_two(zend_op_array *op_array TSRMLS_DC);
int xc_redo_pass_two(zend_op_array *op_array TSRMLS_DC);
int xc_fix_opcode(zend_op_array *op_array TSRMLS_DC);
int xc_undo_fix_opcode(zend_op_array *op_array TSRMLS_DC);
zend_uchar xc_get_fixed_opcode(zend_uchar opcode, int line);

int xc_foreach_early_binding_class(zend_op_array *op_array, void (*callback)(zend_op *opline, int oplineno, void *data TSRMLS_DC), void *data TSRMLS_DC);

/* installer */
#ifdef HAVE_XCACHE_CONSTANT
void xc_install_constant(char *filename, zend_constant *constant, zend_uchar type, zstr key, uint len, ulong h TSRMLS_DC);
#endif
void xc_install_function(char *filename, zend_function *func, zend_uchar type, zstr key, uint len, ulong h TSRMLS_DC);
ZESW(xc_cest_t *, void) xc_install_class(char *filename, xc_cest_t *cest, int oplineno, zend_uchar type, zstr key, uint len, ulong h TSRMLS_DC);

#if defined(E_STRICT) || defined(E_DEPRECATED)
#define XCACHE_ERROR_CACHING
#endif

/* sandbox */
typedef struct {
	int alloc;
	char *filename;

	HashTable orig_included_files;
	HashTable *tmp_included_files;

#ifdef HAVE_XCACHE_CONSTANT
	HashTable *orig_zend_constants;
	HashTable tmp_zend_constants;
#endif
	HashTable *orig_function_table;
	HashTable *orig_class_table;
	HashTable *orig_auto_globals;
	HashTable tmp_function_table;
	HashTable tmp_class_table;
	HashTable tmp_auto_globals;
#ifdef HAVE_XCACHE_CONSTANT
	Bucket    *tmp_internal_constant_tail;
#endif
	Bucket    *tmp_internal_function_tail;
	Bucket    *tmp_internal_class_tail;

#ifdef XCACHE_ERROR_CACHING
	int orig_user_error_handler_error_reporting;
	void (*orig_zend_error_cb)(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);
	zend_uint compilererror_cnt;
	zend_uint compilererror_size;
	xc_compilererror_t *compilererrors;
#endif

#ifdef ZEND_COMPILE_IGNORE_INTERNAL_CLASSES
	zend_uint orig_compiler_options;
#endif
} xc_sandbox_t;

typedef enum _xc_install_action_t {
    XC_NoInstall,
    XC_Install,
    XC_InstallNoBinding
} xc_install_action_t;

void xc_zend_class_add_ref(zend_class_entry ZESW(*ce, **ce));
xc_sandbox_t *xc_sandbox_init(xc_sandbox_t *sandbox, char *filename TSRMLS_DC);
void xc_sandbox_free(xc_sandbox_t *sandbox, xc_install_action_t install TSRMLS_DC);

typedef zend_bool (*xc_if_func_t)(void *data);

void xc_hash_copy_if(HashTable *target, HashTable *source, copy_ctor_func_t pCopyConstructor, void *tmp, uint size, xc_if_func_t checker);
#ifdef HAVE_XCACHE_CONSTANT
void xc_zend_constant_ctor(zend_constant *c);
void xc_zend_constant_dtor(zend_constant *c);
void xc_copy_internal_zend_constants(HashTable *target, HashTable *source);
#endif

typedef struct {
	zend_uint size;
	zend_uint cnt;
	void *data;
} xc_vector_t;

#define xc_vector_init(type, vector) do { \
	(vector)->cnt = 0;     \
	(vector)->size = 0;    \
	(vector)->data = NULL; \
} while (0)

#define xc_vector_add(type, vector, value) do { \
	if ((vector)->cnt == (vector)->size) { \
		if ((vector)->size) { \
			(vector)->size <<= 1; \
			(vector)->data = erealloc((vector)->data, sizeof(type) * (vector)->size); \
		} \
		else { \
			(vector)->size = 8; \
			(vector)->data = emalloc(sizeof(type) * (vector)->size); \
		} \
	} \
	((type *) (vector)->data)[(vector)->cnt++] = value; \
} while (0)

static inline void *xc_vector_detach_impl(xc_vector_t *vector)
{
	void *data = vector->data;
	vector->data = NULL;
	vector->size = 0;
	vector->cnt = 0;
	return data;
}

#define xc_vector_detach(type, vector) ((type *) xc_vector_detach_impl(vector))

static inline void xc_vector_free_impl(xc_vector_t *vector TSRMLS_DC)
{
	if (vector->data) {
		efree(vector->data);
	}
	vector->size = 0;
	vector->cnt = 0;
}

#define xc_vector_free(type, vector) xc_vector_free_impl(vector TSRMLS_CC)

