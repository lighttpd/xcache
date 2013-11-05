
#include "xcache.h"
#include "xc_sandbox.h"
#include "xc_utils.h"
#include "xcache_globals.h"

/* utilities used by sandbox */
static void (*old_zend_error_cb)(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args) = NULL;
static void call_old_zend_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, ...) /* {{{ */
{
	va_list args;
	va_start(args, format);
	old_zend_error_cb(type, error_filename, error_lineno, format, args);
}
/* }}} */
#ifdef ZEND_ENGINE_2_1
static zend_bool xc_auto_global_callback(ZEND_24(NOTHING, const) char *name, uint name_len TSRMLS_DC) /* {{{ */
{
	return 0;
}
/* }}} */
static int xc_auto_global_arm(zend_auto_global *auto_global TSRMLS_DC) /* {{{ */
{
	if (auto_global->auto_global_callback) {
		auto_global->armed = 1;
		auto_global->auto_global_callback = xc_auto_global_callback;
	}
	else {
		auto_global->armed = 0;
	}
	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */
#endif

#ifdef HAVE_XCACHE_CONSTANT
static void xc_free_zend_constant(zend_constant *c) /* {{{ */
{
	if (!(c->flags & CONST_PERSISTENT)) {
		zval_dtor(&c->value);
	}
	free(ZSTR_V(c->name));
}
/* }}} */
#endif

typedef struct _xc_sandbox_t { /* sandbox {{{ */
	ZEND_24(NOTHING, const) char *filename;

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
	zend_uint compilererror_cnt;
	zend_uint compilererror_size;
	xc_compilererror_t *compilererrors;
#endif

#ifdef ZEND_COMPILE_IGNORE_INTERNAL_CLASSES
	zend_uint orig_compiler_options;
#endif
	struct _xc_sandbox_t *parent;
} xc_sandbox_t;

#undef TG
#undef OG
#define TG(x) (sandbox->tmp_##x)
#define OG(x) (sandbox->orig_##x)
/* }}} */
#ifdef XCACHE_ERROR_CACHING
static void xc_sandbox_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args) /* {{{ */
{
	xc_compilererror_t *compilererror;
	xc_sandbox_t *sandbox;
	TSRMLS_FETCH();

	sandbox = (xc_sandbox_t *) XG(sandbox);
	if (!sandbox) {
		old_zend_error_cb(type, error_filename, error_lineno, format, args);
		return;
	}

	switch (type) {
#ifdef E_STRICT
	case E_STRICT:
#endif
#ifdef E_DEPRECATED
	case E_DEPRECATED:
#endif
		if (sandbox->compilererror_cnt <= sandbox->compilererror_size) {
			if (sandbox->compilererror_size) {
				sandbox->compilererror_size += 16;
				sandbox->compilererrors = erealloc(sandbox->compilererrors, sandbox->compilererror_size * sizeof(sandbox->compilererrors));
			}
			else {
				sandbox->compilererror_size = 16;
				sandbox->compilererrors = emalloc(sandbox->compilererror_size * sizeof(sandbox->compilererrors));
			}
		}
		compilererror = &sandbox->compilererrors[sandbox->compilererror_cnt++];
		compilererror->type = type;
		compilererror->lineno = error_lineno;
		compilererror->error_len = vspprintf(&compilererror->error, 0, format, args);
		break;

	default: {
		/* give up, and user handler is not supported in this case */
		zend_uint i;
		zend_uint old_lineno = CG(zend_lineno);

		for (i = 0; i < sandbox->compilererror_cnt; i ++) {
			compilererror = &sandbox->compilererrors[i];
			CG(zend_lineno) = compilererror->lineno;
			call_old_zend_error_cb(compilererror->type, error_filename, error_lineno, "%s", compilererror->error);
			efree(compilererror->error);
		}
		if (sandbox->compilererrors) {
			efree(sandbox->compilererrors);
			sandbox->compilererrors = NULL;
		}
		sandbox->compilererror_cnt  = 0;
		sandbox->compilererror_size = 0;

		CG(zend_lineno) = old_lineno;
		old_zend_error_cb(type, error_filename, error_lineno, format, args);
		break;
	}
	}
}
/* }}} */
#endif

static xc_sandbox_t *xc_sandbox_init(xc_sandbox_t *sandbox, ZEND_24(NOTHING, const) char *filename TSRMLS_DC) /* {{{ */
{
	HashTable *h;

	assert(sandbox);
	memset(sandbox, 0, sizeof(sandbox[0]));

	memcpy(&OG(included_files), &EG(included_files), sizeof(EG(included_files)));

#ifdef HAVE_XCACHE_CONSTANT
	OG(zend_constants) = EG(zend_constants);
	EG(zend_constants) = &TG(zend_constants);
#endif

	OG(function_table) = CG(function_table);
	CG(function_table) = &TG(function_table);
	EG(function_table) = CG(function_table);

	OG(class_table) = CG(class_table);
	CG(class_table) = &TG(class_table);
	EG(class_table) = CG(class_table);

#ifdef ZEND_ENGINE_2_1
	OG(auto_globals) = CG(auto_globals);
	CG(auto_globals) = &TG(auto_globals);
#endif

	TG(included_files) = &EG(included_files);

	zend_hash_init_ex(TG(included_files), 5, NULL, NULL, 0, 1);
#ifdef HAVE_XCACHE_CONSTANT
	h = OG(zend_constants);
	zend_hash_init_ex(&TG(zend_constants),  20, NULL, (dtor_func_t) xc_free_zend_constant, h->persistent, h->bApplyProtection);
	xc_copy_internal_zend_constants(&TG(zend_constants), &XG(internal_constant_table));
	TG(internal_constant_tail) = TG(zend_constants).pListTail;
#endif
	h = OG(function_table);
	zend_hash_init_ex(&TG(function_table), 128, NULL, ZEND_FUNCTION_DTOR, h->persistent, h->bApplyProtection);
	{
		zend_function tmp_func;
		zend_hash_copy(&TG(function_table), &XG(internal_function_table), NULL, (void *) &tmp_func, sizeof(tmp_func));
	}
	TG(internal_function_tail) = TG(function_table).pListTail;

	h = OG(class_table);
	zend_hash_init_ex(&TG(class_table),     16, NULL, ZEND_CLASS_DTOR, h->persistent, h->bApplyProtection);
#if 0 && TODO
	{
		xc_cest_t tmp_cest;
		zend_hash_copy(&TG(class_table), &XG(internal_class_table), NULL, (void *) &tmp_cest, sizeof(tmp_cest));
	}
#endif
	TG(internal_class_tail) = TG(class_table).pListTail;

#ifdef ZEND_ENGINE_2_1
	/* shallow copy, don't destruct */
	h = OG(auto_globals);
	zend_hash_init_ex(&TG(auto_globals),     8, NULL,           NULL, h->persistent, h->bApplyProtection);
	{
		zend_auto_global tmp_autoglobal;

		zend_hash_copy(&TG(auto_globals), OG(auto_globals), NULL, (void *) &tmp_autoglobal, sizeof(tmp_autoglobal));
		zend_hash_apply(&TG(auto_globals), (apply_func_t) xc_auto_global_arm TSRMLS_CC);
	}
#endif

	sandbox->filename = filename;

#ifdef XCACHE_ERROR_CACHING
	sandbox->orig_user_error_handler_error_reporting = EG(user_error_handler_error_reporting);
	EG(user_error_handler_error_reporting) = 0;

	sandbox->compilererror_cnt  = 0;
	sandbox->compilererror_size = 0;
#endif

#ifdef ZEND_COMPILE_IGNORE_INTERNAL_CLASSES
	sandbox->orig_compiler_options = CG(compiler_options);
	/* Using ZEND_COMPILE_IGNORE_INTERNAL_CLASSES for ZEND_FETCH_CLASS_RT_NS_CHECK
	 */
	CG(compiler_options) |= ZEND_COMPILE_IGNORE_INTERNAL_CLASSES | ZEND_COMPILE_NO_CONSTANT_SUBSTITUTION | ZEND_COMPILE_DELAYED_BINDING;
#endif

	sandbox->parent = XG(sandbox);
	XG(sandbox) = (void *) sandbox;
	XG(initial_compile_file_called) = 0;
	return sandbox;
}
/* }}} */

#ifndef ZEND_COMPILE_DELAYED_BINDING
static void xc_early_binding_cb(zend_op *opline, int oplineno, void *data TSRMLS_DC) /* {{{ */
{
	xc_sandbox_t *sandbox = (xc_sandbox_t *) data;
	xc_do_early_binding(CG(active_op_array), OG(class_table), oplineno TSRMLS_CC);
}
/* }}} */
#endif
static void xc_sandbox_install(xc_sandbox_t *sandbox TSRMLS_DC) /* {{{ */
{
	zend_uint i;
	Bucket *b;

#ifdef HAVE_XCACHE_CONSTANT
	for (b = TG(zend_constants).pListHead; b != NULL && b != TG(internal_constant_tail); b = b->pListNext) {
		zend_constant *c = (zend_constant*) b->pData;
		xc_free_zend_constant(c);
	}

	b = TG(internal_constant_tail) ? TG(internal_constant_tail)->pListNext : TG(zend_constants).pListHead;
	/* install constants */
	while (b != NULL) {
		zend_constant *c = (zend_constant*) b->pData;
		xc_install_constant(sandbox->filename, c,
				BUCKET_KEY_TYPE(b), ZSTR(BUCKET_KEY_S(b)), b->nKeyLength, b->h TSRMLS_CC);
		b = b->pListNext;
	}
#endif

	b = TG(internal_function_tail) ? TG(internal_function_tail)->pListNext : TG(function_table).pListHead;
	/* install function */
	while (b != NULL) {
		zend_function *func = (zend_function*) b->pData;
		xc_install_function(sandbox->filename, func,
				BUCKET_KEY_TYPE(b), ZSTR(BUCKET_KEY_S(b)), b->nKeyLength, b->h TSRMLS_CC);
		b = b->pListNext;
	}

	b = TG(internal_class_tail) ? TG(internal_class_tail)->pListNext : TG(class_table).pListHead;
	/* install class */
	while (b != NULL) {
		xc_install_class(sandbox->filename, (xc_cest_t*) b->pData, -1,
				BUCKET_KEY_TYPE(b), ZSTR(BUCKET_KEY_S(b)), b->nKeyLength, b->h TSRMLS_CC);
		b = b->pListNext;
	}

#ifdef ZEND_ENGINE_2_1
	/* trigger auto_globals jit */
	for (b = TG(auto_globals).pListHead; b != NULL; b = b->pListNext) {
		zend_auto_global *auto_global = (zend_auto_global *) b->pData;
		/* check if actived */
		if (auto_global->auto_global_callback && !auto_global->armed) {
			zend_u_is_auto_global(BUCKET_KEY_TYPE(b), ZSTR(BUCKET_KEY_S(b)), auto_global->name_len TSRMLS_CC);
		}
	}
#endif

	/* CG(compiler_options) applies only if initial_compile_file_called */
	if (XG(initial_compile_file_called)) {
#ifdef ZEND_COMPILE_DELAYED_BINDING
		zend_do_delayed_early_binding(CG(active_op_array) TSRMLS_CC);
#else
		xc_undo_pass_two(CG(active_op_array) TSRMLS_CC);
		xc_foreach_early_binding_class(CG(active_op_array), xc_early_binding_cb, (void *) sandbox TSRMLS_CC);
		xc_redo_pass_two(CG(active_op_array) TSRMLS_CC);
#endif
	}

#ifdef XCACHE_ERROR_CACHING
	/* restore trigger errors */
	for (i = 0; i < sandbox->compilererror_cnt; i ++) {
		xc_compilererror_t *error = &sandbox->compilererrors[i];
		CG(zend_lineno) = error->lineno;
		zend_error(error->type, "%s", error->error);
	}
	CG(zend_lineno) = 0;
#endif

	i = 1;
	/* still needed because in zend_language_scanner.l, require()/include() check file_handle.handle.stream.handle */
	zend_hash_add(&OG(included_files), sandbox->filename, strlen(sandbox->filename) + 1, (void *)&i, sizeof(int), NULL);
}
/* }}} */
static void xc_sandbox_free(xc_sandbox_t *sandbox, zend_op_array *op_array TSRMLS_DC) /* {{{ */
{
	XG(sandbox) = sandbox->parent;
#ifdef XCACHE_ERROR_CACHING
	EG(user_error_handler_error_reporting) = sandbox->orig_user_error_handler_error_reporting;
#endif

	/* restore first first install function/class */
#ifdef HAVE_XCACHE_CONSTANT
	EG(zend_constants) = OG(zend_constants);
#endif
	CG(function_table) = OG(function_table);
	EG(function_table) = CG(function_table);
	CG(class_table)    = OG(class_table);
	EG(class_table)    = CG(class_table);
#ifdef ZEND_ENGINE_2_1
	CG(auto_globals)   = OG(auto_globals);
#endif

	if (op_array) {
		zend_op_array *old_active_op_array = CG(active_op_array);
		CG(in_compilation)    = 1;
		CG(compiled_filename) = ZEND_24(NOTHING, (char *)) sandbox->filename;
		CG(zend_lineno)       = 0;

		CG(active_op_array) = op_array;
		xc_sandbox_install(sandbox TSRMLS_CC);
		CG(active_op_array) = old_active_op_array;

		CG(in_compilation)    = 0;
		CG(compiled_filename) = NULL;

		/* no free as it's installed */
#ifdef HAVE_XCACHE_CONSTANT
		TG(zend_constants).pDestructor = NULL;
#endif
		TG(function_table).pDestructor = NULL;
		TG(class_table).pDestructor = NULL;
	}

	/* destroy all the tmp */
#ifdef HAVE_XCACHE_CONSTANT
	zend_hash_destroy(&TG(zend_constants));
#endif
	zend_hash_destroy(&TG(function_table));
	zend_hash_destroy(&TG(class_table));
#ifdef ZEND_ENGINE_2_1
	zend_hash_destroy(&TG(auto_globals));
#endif
	zend_hash_destroy(TG(included_files));

	/* restore orig here, as EG/CG holded tmp before */
	memcpy(&EG(included_files), &OG(included_files), sizeof(EG(included_files)));

#ifdef XCACHE_ERROR_CACHING
	if (sandbox->compilererrors) {
		zend_uint i;
		for (i = 0; i < sandbox->compilererror_cnt; i ++) {
			efree(sandbox->compilererrors[i].error);
		}
		efree(sandbox->compilererrors);
	}
#endif

#ifdef ZEND_COMPILE_IGNORE_INTERNAL_CLASSES
	CG(compiler_options) = sandbox->orig_compiler_options;
#endif
}
/* }}} */
zend_op_array *xc_sandbox(xc_sandboxed_func_t sandboxed_func, void *data, ZEND_24(NOTHING, const) char *filename TSRMLS_DC) /* {{{ */
{
	xc_sandbox_t sandbox;
	zend_op_array *op_array = NULL;
	zend_bool catched = 0;

	memset(&sandbox, 0, sizeof(sandbox));
	zend_try {
		xc_sandbox_init(&sandbox, filename TSRMLS_CC);
		op_array = sandboxed_func(data TSRMLS_CC);
	} zend_catch {
		catched = 1;
	} zend_end_try();

	xc_sandbox_free(&sandbox, op_array TSRMLS_CC);
	if (catched) {
		zend_bailout();
	}
	return op_array;
}
/* }}} */
const Bucket *xc_sandbox_user_function_begin(TSRMLS_D) /* {{{ */
{
	xc_sandbox_t *sandbox = (xc_sandbox_t *) XG(sandbox);
	assert(sandbox);
	return TG(internal_function_tail) ? TG(internal_function_tail)->pListNext : TG(function_table).pListHead;
}
/* }}} */
const Bucket *xc_sandbox_user_class_begin(TSRMLS_D) /* {{{ */
{
	xc_sandbox_t *sandbox = (xc_sandbox_t *) XG(sandbox);
	assert(sandbox);
	return TG(internal_class_tail) ? TG(internal_class_tail)->pListNext : TG(class_table).pListHead;
}
/* }}} */
#ifdef XCACHE_ERROR_CACHING
xc_compilererror_t *xc_sandbox_compilererrors(TSRMLS_D) /* {{{ */
{
	xc_sandbox_t *sandbox = (xc_sandbox_t *) XG(sandbox);
	assert(sandbox);
	return sandbox->compilererrors;
}
/* }}} */
zend_uint xc_sandbox_compilererror_cnt(TSRMLS_D) /* {{{ */
{
	xc_sandbox_t *sandbox = (xc_sandbox_t *) XG(sandbox);
	assert(sandbox);
	return sandbox->compilererror_cnt;
}
/* }}} */
#endif

/* MINIT/MSHUTDOWN */
int xc_sandbox_module_init(int module_number TSRMLS_DC) /* {{{ */
{
#ifdef XCACHE_ERROR_CACHING
	old_zend_error_cb = zend_error_cb;
	zend_error_cb = xc_sandbox_error_cb;
#endif

	return SUCCESS;
}
/* }}} */
void xc_sandbox_module_shutdown() /* {{{ */
{
#ifdef XCACHE_ERROR_CACHING
	if (zend_error_cb == xc_sandbox_error_cb) {
		zend_error_cb = old_zend_error_cb;
	}
#endif
}
/* }}} */
