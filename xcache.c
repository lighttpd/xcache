/* {{{ macros */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <signal.h>

#include "xcache.h"

#ifdef HAVE_XCACHE_OPTIMIZER
#	include "mod_optimizer/xc_optimizer.h"
#endif
#ifdef HAVE_XCACHE_CACHER
#	include "mod_cacher/xc_cacher.h"
#endif
#ifdef HAVE_XCACHE_COVERAGER
#	include "mod_coverager/xc_coverager.h"
#endif
#ifdef HAVE_XCACHE_DISASSEMBLER
#	include "mod_disassembler/xc_disassembler.h"
#endif

#include "xcache_globals.h"
#include "xcache/xc_extension.h"
#include "xcache/xc_ini.h"
#include "xcache/xc_const_string.h"
#include "xcache/xc_opcode_spec.h"
#include "xcache/xc_utils.h"
#include "util/xc_stack.h"

#include "php.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "SAPI.h"
/* }}} */

/* {{{ globals */
static char *xc_coredump_dir = NULL;
#ifdef ZEND_WIN32
static zend_ulong xc_coredump_type = 0;
#endif
static zend_bool xc_disable_on_crash = 0;

static zend_compile_file_t *old_compile_file = NULL;

zend_bool xc_test = 0;

ZEND_DECLARE_MODULE_GLOBALS(xcache)

/* }}} */

static zend_op_array *xc_check_initial_compile_file(zend_file_handle *h, int type TSRMLS_DC) /* {{{ */
{
	XG(initial_compile_file_called) = 1;
	return old_compile_file(h, type TSRMLS_CC);
}
/* }}} */

/* devel helper function */
#if 0
static void xc_list_extensions() /* {{{ */
{
	zend_llist_element *element;
	zend_extension *ext;
	fprintf(stderr, "extensions:\n");
	for (element = zend_extensions.head; element; element = element->next) {
		ext = (zend_extension *) element->data;
		fprintf(stderr, " - %s\n", ext->name);
	}
}
/* }}} */
#endif

/* module helper function */
static int xc_init_constant(int module_number TSRMLS_DC) /* {{{ */
{
	typedef struct {
		const char *prefix;
		zend_uchar (*getsize)();
		const char *(*get)(zend_uchar i);
	} xc_nameinfo_t;
	xc_nameinfo_t nameinfos[] = {
		{ "",        xc_get_op_type_count,   xc_get_op_type   },
		{ "",        xc_get_data_type_count, xc_get_data_type },
		{ "",        xc_get_opcode_count,    xc_get_opcode    },
		{ "OPSPEC_", xc_get_op_spec_count,   xc_get_op_spec   },
		{ NULL, NULL, NULL }
	};
	int undefdone = 0;
	xc_nameinfo_t *p;

	for (p = nameinfos; p->getsize; p ++) {
		zend_uchar i, count;
		char const_name[96];
		int const_name_len;

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

	zend_register_long_constant(XCACHE_STRS("XC_SIZEOF_TEMP_VARIABLE"), sizeof(temp_variable), CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC);
	zend_register_stringl_constant(XCACHE_STRS("XCACHE_VERSION"), XCACHE_STRL(XCACHE_VERSION), CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC);
	zend_register_stringl_constant(XCACHE_STRS("XCACHE_MODULES"), XCACHE_STRL(XCACHE_MODULES), CONST_CS | CONST_PERSISTENT, module_number TSRMLS_CC);
	return 0;
}
/* }}} */
/* {{{ PHP_GINIT_FUNCTION(xcache) */
#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#ifdef PHP_GINIT_FUNCTION
static PHP_GINIT_FUNCTION(xcache)
#else
static void xc_init_globals(zend_xcache_globals* xcache_globals TSRMLS_DC)
#endif
{
#ifdef __GNUC__
#pragma GCC pop_options
#endif

	memset(xcache_globals, 0, sizeof(zend_xcache_globals));

#ifdef HAVE_XCACHE_CONSTANT
	zend_hash_init_ex(&xcache_globals->internal_constant_table, 1, NULL, (dtor_func_t) xc_zend_constant_dtor, 1, 0);
#endif
	zend_hash_init_ex(&xcache_globals->internal_function_table, 1, NULL, NULL, 1, 0);
	zend_hash_init_ex(&xcache_globals->internal_class_table,    1, NULL, NULL, 1, 0);
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
	size_t i;

	if (xcache_globals->php_holds != NULL) {
		for (i = 0; i < xcache_globals->php_holds_size; i ++) {
			xc_stack_destroy(&xcache_globals->php_holds[i]);
		}
		free(xcache_globals->php_holds);
		xcache_globals->php_holds = NULL;
		xcache_globals->php_holds_size = 0;
	}

	if (xcache_globals->var_holds != NULL) {
		for (i = 0; i < xcache_globals->var_holds_size; i ++) {
			xc_stack_destroy(&xcache_globals->var_holds[i]);
		}
		free(xcache_globals->var_holds);
		xcache_globals->var_holds = NULL;
		xcache_globals->var_holds_size = 0;
	}

	if (xcache_globals->internal_table_copied) {
#ifdef HAVE_XCACHE_CONSTANT
		zend_hash_destroy(&xcache_globals->internal_constant_table);
#endif
		zend_hash_destroy(&xcache_globals->internal_function_table);
		zend_hash_destroy(&xcache_globals->internal_class_table);
	}
}
/* }}} */

/* {{{ proto int xcache_get_refcount(mixed variable)
   XCache internal uses only: Get reference count of variable */
PHP_FUNCTION(xcache_get_refcount)
{
	zval *variable;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &variable) == FAILURE) {
		RETURN_NULL();
	}

	RETURN_LONG(Z_REFCOUNT(*variable));
}
/* }}} */
/* {{{ proto bool xcache_get_isref(mixed variable)
   XCache internal uses only: Check if variable data is marked referenced */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_xcache_get_isref, 0, 0, 1)
	ZEND_ARG_INFO(1, variable)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_xcache_get_isref[] = { 1, BYREF_FORCE };
#endif

PHP_FUNCTION(xcache_get_isref)
{
	zval *variable;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &variable) == FAILURE) {
		RETURN_NULL();
	}

	RETURN_BOOL(Z_ISREF(*variable) && Z_REFCOUNT(*variable) >= 3);
}
/* }}} */
#ifdef HAVE_XCACHE_DPRINT
/* {{{ proto bool  xcache_dprint(mixed value)
   Prints variable (or value) internal struct (debug only) */
#include "xc_processor.h"
PHP_FUNCTION(xcache_dprint)
{
	zval *value;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &value) == FAILURE) {
		return;
	}
	xc_dprint_zval(value, 0 TSRMLS_CC);
}
/* }}} */
#endif
/* {{{ proto string xcache_asm(string filename)
 */
#ifdef HAVE_XCACHE_ASSEMBLER
PHP_FUNCTION(xcache_asm)
{
}
#endif
/* }}} */
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
			int len = (int) strlen(name);
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
			add_assoc_long_ex(return_value, XCACHE_STRS("ext"), opspec->ext);
			add_assoc_long_ex(return_value, XCACHE_STRS("op1"), opspec->op1);
			add_assoc_long_ex(return_value, XCACHE_STRS("op2"), opspec->op2);
			add_assoc_long_ex(return_value, XCACHE_STRS("res"), opspec->res);
			return;
		}
	}
	RETURN_NULL();
}
/* }}} */
/* {{{ proto mixed xcache_get_special_value(zval value)
   XCache internal use only: For decompiler to get static value with type fixed */
PHP_FUNCTION(xcache_get_special_value)
{
	zval *value;
	zval value_copied;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &value) == FAILURE) {
		return;
	}
	value_copied = *value;
	value = &value_copied;

	switch ((Z_TYPE_P(value) & IS_CONSTANT_TYPE_MASK)) {
	case IS_CONSTANT:
		value->type = UNISW(IS_STRING, UG(unicode) ? IS_UNICODE : IS_STRING);
		RETURN_ZVAL(value, 1, 0);
		break;

#ifdef IS_CONSTANT_ARRAY
	case IS_CONSTANT_ARRAY:
		value->type = IS_ARRAY;
		RETURN_ZVAL(value, 1, 0);
		break;
#endif

#ifdef IS_CONSTANT_AST
	case IS_CONSTANT_AST:
		RETURN_NULL();
		break;
#endif

	default:
		if ((Z_TYPE_P(value) & ~IS_CONSTANT_TYPE_MASK)) {
			value->type &= IS_CONSTANT_TYPE_MASK;
			RETURN_ZVAL(value, 1, 0);
		}
		else {
			RETURN_NULL();
		}
	}
}
/* }}} */
/* {{{ proto int xcache_get_type(zval value)
   XCache internal use only for disassembler to get variable type in engine level */
PHP_FUNCTION(xcache_get_type)
{
	zval *value;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &value) == FAILURE) {
		return;
	}

	RETURN_LONG(Z_TYPE_P(value));
}
/* }}} */
/* {{{ proto string xcache_coredump(int op_type) */
PHP_FUNCTION(xcache_coredump)
{
	if (xc_test) {
		char *null_ptr = NULL;
		*null_ptr = 0;
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
	zval *name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &name) == FAILURE) {
		return;
	}

#ifdef IS_UNICODE
	convert_to_unicode(name);
#else
	convert_to_string(name);
#endif

	RETURN_BOOL(zend_u_hash_exists(CG(auto_globals), UG(unicode), Z_STRVAL_P(name), Z_STRLEN_P(name) + 1));
}
/* }}} */
static zend_function_entry xcache_functions[] = /* {{{ */
{
	PHP_FE(xcache_coredump,          NULL)
#ifdef HAVE_XCACHE_ASSEMBLER
	PHP_FE(xcache_asm,               NULL)
#endif
#ifdef HAVE_XCACHE_ENCODER
	PHP_FE(xcache_encode,            NULL)
#endif
#ifdef HAVE_XCACHE_DECODER
	PHP_FE(xcache_decode_file,       NULL)
	PHP_FE(xcache_decode_string,     NULL)
#endif
	PHP_FE(xcache_get_special_value, NULL)
	PHP_FE(xcache_get_type,          NULL)
	PHP_FE(xcache_get_op_type,       NULL)
	PHP_FE(xcache_get_data_type,     NULL)
	PHP_FE(xcache_get_opcode,        NULL)
	PHP_FE(xcache_get_opcode_spec,   NULL)
	PHP_FE(xcache_is_autoglobal,     NULL)
	PHP_FE(xcache_get_refcount,      NULL)
	PHP_FE(xcache_get_isref,         arginfo_xcache_get_isref)
#ifdef HAVE_XCACHE_DPRINT
	PHP_FE(xcache_dprint,            NULL)
#endif
	PHP_FE_END
};
/* }}} */

#ifdef ZEND_WIN32
#include "dbghelp.h"
typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
		CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
		CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
		CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
		);

static PTOP_LEVEL_EXCEPTION_FILTER oldFilter = NULL;
static HMODULE dbghelpModule = NULL;
static char crash_dumpPath[_MAX_PATH] = { 0 };
static MINIDUMPWRITEDUMP dbghelp_MiniDumpWriteDump = NULL;

static LONG WINAPI miniDumperFilter(struct _EXCEPTION_POINTERS *pExceptionInfo) /* {{{ */
{
	HANDLE fileHandle;
	LONG ret = EXCEPTION_CONTINUE_SEARCH;

	SetUnhandledExceptionFilter(oldFilter);

	/* create the file */
	fileHandle = CreateFile(crash_dumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (fileHandle != INVALID_HANDLE_VALUE) {
		MINIDUMP_EXCEPTION_INFORMATION exceptionInformation;
		MINIDUMP_TYPE type = xc_coredump_type ? xc_coredump_type : (MiniDumpNormal|MiniDumpWithDataSegs|MiniDumpWithIndirectlyReferencedMemory);
		BOOL ok;

		exceptionInformation.ThreadId = GetCurrentThreadId();
		exceptionInformation.ExceptionPointers = pExceptionInfo;
		exceptionInformation.ClientPointers = FALSE;

		/* write the dump */
		ok = dbghelp_MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), fileHandle, type, &exceptionInformation, NULL, NULL);
		CloseHandle(fileHandle);
		if (ok) {
			zend_error(E_ERROR, "Saved dump file to '%s'", crash_dumpPath);
			ret = EXCEPTION_EXECUTE_HANDLER;
		}
		else {
			zend_error(E_ERROR, "Failed to save dump file to '%s' (error %d)", crash_dumpPath, GetLastError());
		}
	}
	else {
		zend_error(E_ERROR, "Failed to create dump file '%s' (error %d)", crash_dumpPath, GetLastError());
	}

	if (xc_disable_on_crash) {
		xc_disable_on_crash = 0;
		xc_cacher_disable();
	}

	return ret;
}
/* }}} */

static void xcache_restore_crash_handler() /* {{{ */
{
	if (oldFilter) {
		SetUnhandledExceptionFilter(oldFilter);
		oldFilter = NULL;
	}
}
/* }}} */
static void xcache_init_crash_handler() /* {{{ */
{
	/* firstly see if dbghelp.dll is around and has the function we need
	   look next to the EXE first, as the one in System32 might be old
	   (e.g. Windows 2000) */
	char dbghelpPath[_MAX_PATH];

	if (GetModuleFileName(NULL, dbghelpPath, _MAX_PATH)) {
		char *slash = strchr(dbghelpPath, '\\');
		if (slash) {
			strcpy(slash + 1, "DBGHELP.DLL");
			dbghelpModule = LoadLibrary(dbghelpPath);
		}
	}

	if (!dbghelpModule) {
		/* load any version we can */
		dbghelpModule = LoadLibrary("DBGHELP.DLL");
		if (!dbghelpModule) {
			zend_error(E_ERROR, "Unable to enable crash dump saver: %s", "DBGHELP.DLL not found");
			return;
		}
	}

	dbghelp_MiniDumpWriteDump = (MINIDUMPWRITEDUMP)GetProcAddress(dbghelpModule, "MiniDumpWriteDump");
	if (!dbghelp_MiniDumpWriteDump) {
		zend_error(E_ERROR, "Unable to enable crash dump saver: %s", "DBGHELP.DLL too old. Get updated dll and put it aside of php_xcache.dll");
		return;
	}

#ifdef XCACHE_VERSION_REVISION
#define REVISION "r" XCACHE_VERSION_REVISION
#else
#define REVISION ""
#endif
	sprintf(crash_dumpPath, "%s\\php-%s-xcache-%s%s-%lu-%lu.dmp", xc_coredump_dir, zend_get_module_version("standard"), XCACHE_VERSION, REVISION, (unsigned long) time(NULL), (unsigned long) GetCurrentProcessId());
#undef REVISION

	oldFilter = SetUnhandledExceptionFilter(&miniDumperFilter);
}
/* }}} */
#else
/* old signal handlers {{{ */
typedef void (*xc_sighandler_t)(int);
#define FOREACH_SIG(sig) static xc_sighandler_t old_##sig##_handler = NULL
#include "util/xc_foreachcoresig.h"
#undef FOREACH_SIG
/* }}} */
static void xcache_signal_handler(int sig);
static void xcache_restore_crash_handler() /* {{{ */
{
#define FOREACH_SIG(sig) do { \
	if (old_##sig##_handler != xcache_signal_handler) { \
		signal(sig, old_##sig##_handler); \
	} \
	else { \
		signal(sig, SIG_DFL); \
	} \
} while (0)
#include "util/xc_foreachcoresig.h"
#undef FOREACH_SIG
}
/* }}} */
static void xcache_init_crash_handler() /* {{{ */
{
#define FOREACH_SIG(sig) \
	old_##sig##_handler = signal(sig, xcache_signal_handler)
#include "util/xc_foreachcoresig.h"
#undef FOREACH_SIG
}
/* }}} */
static void xcache_signal_handler(int sig) /* {{{ */
{
	xcache_restore_crash_handler();
	if (xc_coredump_dir && xc_coredump_dir[0]) {
		if (chdir(xc_coredump_dir) != 0) {
			/* error, but nothing can do about it
			 * and should'nt print anything which might SEGV again */
		}
	}
	if (xc_disable_on_crash) {
		xc_disable_on_crash = 0;
		xc_cacher_disable();
	}
	raise(sig);
}
/* }}} */
#endif

/* {{{ incompatible zend extensions handling */
typedef struct {
	const char *name;
	startup_func_t old_startup;
} xc_incompatible_zend_extension_info_t;
static xc_incompatible_zend_extension_info_t xc_incompatible_zend_extensions[] = {
	{ "Zend Extension Manager", NULL },
	{ "Zend Optimizer", NULL },
	{ "the ionCube PHP Loader", NULL }
};

static xc_incompatible_zend_extension_info_t *xc_get_incompatible_zend_extension_info(const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(xc_incompatible_zend_extensions) / sizeof(xc_incompatible_zend_extensions[0]); ++i) {
		xc_incompatible_zend_extension_info_t *incompatible_zend_extension_info = &xc_incompatible_zend_extensions[i];
		if (strcmp(incompatible_zend_extension_info->name, name) == 0) {
			return incompatible_zend_extension_info;
		}
	}

	return NULL;
}
/* }}} */
static void xc_zend_llist_add_element(zend_llist *list, zend_llist_element *element) /* {{{ */
{
	if (!zend_extensions.head) {
		zend_extensions.head = zend_extensions.tail = element;
	}
	else {
		zend_extensions.tail->next = element;
		element->prev = zend_extensions.tail;
		zend_extensions.tail = element;
	}
}
/* }}} */
static int xc_incompatible_zend_extension_startup_hook(zend_extension *extension) /* {{{ */
{
	xc_incompatible_zend_extension_info_t *incompatible_zend_extension_info = xc_get_incompatible_zend_extension_info(extension->name);
	int status;
	zend_bool catched = 0;
	zend_llist saved_zend_extensions_container; /* without elements */
	zend_llist_element **saved_zend_extensions_elments;
	size_t new_zend_extensions_elments_count;
	zend_llist_element **new_zend_extensions_elments;
	zend_extension *ext;
	size_t i;
	zend_llist_element *element;
	TSRMLS_FETCH();

	/* restore startup hack */
	extension->startup = incompatible_zend_extension_info->old_startup;
	incompatible_zend_extension_info->old_startup = NULL;
	assert(extension->startup);

	/* save extensions list */
	saved_zend_extensions_container = zend_extensions;
	saved_zend_extensions_elments = malloc(sizeof(zend_llist_element *) * saved_zend_extensions_container.count);
	for (i = 0, element = saved_zend_extensions_container.head; element; ++i, element = element->next) {
		saved_zend_extensions_elments[i] = element;
	}

	/* hide all XCache extensions from it */
	zend_extensions.head = NULL;
	zend_extensions.tail = NULL;
	zend_extensions.count = 0;

	for (i = 0; i < saved_zend_extensions_container.count; ++i) {
		element = saved_zend_extensions_elments[i];
		element->next = element->prev = NULL;

		ext = (zend_extension *) element->data;

		if (!(strcmp(ext->name, XCACHE_NAME) == 0 || strncmp(ext->name, XCACHE_NAME " ", sizeof(XCACHE_NAME " ") - 1) == 0)) {
			xc_zend_llist_add_element(&zend_extensions, element);
			++zend_extensions.count;
		}
	}

	assert(extension->startup != xc_incompatible_zend_extension_startup_hook);
	zend_try {
		status = extension->startup(extension);
	} zend_catch {
		catched = 1;
	} zend_end_try();

	/* save newly added extensions added by this extension*/
	new_zend_extensions_elments_count = zend_extensions.count - 1;
	new_zend_extensions_elments = NULL;
	if (new_zend_extensions_elments_count) {
		new_zend_extensions_elments = malloc(sizeof(zend_llist_element *) * new_zend_extensions_elments_count);
		element = zend_extensions.head;
		for (i = 0, element = element->next; element; ++i, element = element->next) {
			new_zend_extensions_elments[i] = element;
		}
	}

	/* restore original extension list*/
	zend_extensions = saved_zend_extensions_container;
	zend_extensions.head = NULL;
	zend_extensions.tail = NULL;
	zend_extensions.count = 0;
	for (i = 0; i < saved_zend_extensions_container.count; ++i) {
		element = saved_zend_extensions_elments[i];
		element->next = element->prev = NULL;

		xc_zend_llist_add_element(&zend_extensions, element);
		++zend_extensions.count;

		ext = (zend_extension *) element->data;
		if (ext == extension && new_zend_extensions_elments_count) {
			/* add new created extension */
			size_t j;
			for (j = 0; j < new_zend_extensions_elments_count; ++j) {
				element = new_zend_extensions_elments[j];
				element->next = element->prev = NULL;

				xc_zend_llist_add_element(&zend_extensions, element);
				++zend_extensions.count;
			}
		}
	}
	free(saved_zend_extensions_elments);
	if (new_zend_extensions_elments) {
		free(new_zend_extensions_elments);
	}

	if (catched) {
		zend_bailout();
	}
	return status;
}
/* }}} */
static int xc_zend_startup(zend_extension *extension) /* {{{ */
{
	zend_llist_position lpos;
	zend_extension *ext;

	ext = (zend_extension *) zend_extensions.head->data;
	if (strcmp(ext->name, XCACHE_NAME) != 0) {
		zend_error(E_WARNING, "XCache failed to load itself to before zend_extension=\"%s\". compatibility downgraded", ext->name);
	}

	for (ext = (zend_extension *) zend_llist_get_first_ex(&zend_extensions, &lpos);
			ext;
			ext = (zend_extension *) zend_llist_get_next_ex(&zend_extensions, &lpos)) {
		xc_incompatible_zend_extension_info_t *incompatible_zend_extension_info = xc_get_incompatible_zend_extension_info(ext->name);
		if (incompatible_zend_extension_info) {
			assert(!incompatible_zend_extension_info->old_startup);
			incompatible_zend_extension_info->old_startup = ext->startup;
			ext->startup = xc_incompatible_zend_extension_startup_hook;
		}
	}
	return SUCCESS;
}
/* }}} */
static void xc_zend_shutdown(zend_extension *extension) /* {{{ */
{
}
/* }}} */
/* {{{ zend extension definition structure */
static zend_extension xc_zend_extension_entry = {
	XCACHE_NAME,
	XCACHE_VERSION,
	XCACHE_AUTHOR,
	XCACHE_URL,
	XCACHE_COPYRIGHT,
	xc_zend_startup,
	xc_zend_shutdown,
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

/* {{{ PHP_INI */
PHP_INI_BEGIN()
	PHP_INI_ENTRY1     ("xcache.coredump_directory",      "", PHP_INI_SYSTEM, xcache_OnUpdateString,   &xc_coredump_dir)
#ifdef ZEND_WIN32
	PHP_INI_ENTRY1     ("xcache.coredump_type",          "0", PHP_INI_SYSTEM, xcache_OnUpdateULong,    &xc_coredump_type)
#endif
	PHP_INI_ENTRY1_EX  ("xcache.disable_on_crash",       "0", PHP_INI_SYSTEM, xcache_OnUpdateBool,     &xc_disable_on_crash, zend_ini_boolean_displayer_cb)
	PHP_INI_ENTRY1_EX  ("xcache.test",                   "0", PHP_INI_SYSTEM, xcache_OnUpdateBool,     &xc_test,             zend_ini_boolean_displayer_cb)
	STD_PHP_INI_BOOLEAN("xcache.experimental",           "0", PHP_INI_ALL,    OnUpdateBool,        experimental,      zend_xcache_globals, xcache_globals)
PHP_INI_END()
/* }}} */
static PHP_MINFO_FUNCTION(xcache) /* {{{ */
{
	php_info_print_table_start();
	php_info_print_table_row(2, "XCache Version", XCACHE_VERSION);
#ifdef XCACHE_VERSION_REVISION
	php_info_print_table_row(2, "Revision", "r" XCACHE_VERSION_REVISION);
#endif
	php_info_print_table_row(2, "Modules Built", XCACHE_MODULES);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */
static PHP_MINIT_FUNCTION(xcache) /* {{{ */
{
#ifndef PHP_GINIT
	ZEND_INIT_MODULE_GLOBALS(xcache, xc_init_globals, xc_shutdown_globals);
#endif
	REGISTER_INI_ENTRIES();

	if (xc_coredump_dir && xc_coredump_dir[0]) {
		xcache_init_crash_handler();
	}

	if (strcmp(sapi_module.name, "cli") == 0) {
		char *env;
		if ((env = getenv("XCACHE_TEST")) != NULL) {
			xc_test = (zend_bool) atoi(env);
		}
	}

	xc_init_constant(module_number TSRMLS_CC);
	xc_shm_init_modules();

	/* must be the first */
	xcache_zend_extension_add(&xc_zend_extension_entry, 1);
	old_compile_file = zend_compile_file;
	zend_compile_file = xc_check_initial_compile_file;

#ifdef HAVE_XCACHE_OPTIMIZER
	xc_optimizer_startup_module();
#endif
#ifdef HAVE_XCACHE_CACHER
	xc_cacher_startup_module();
#endif
#ifdef HAVE_XCACHE_COVERAGER
	xc_coverager_startup_module();
#endif
#ifdef HAVE_XCACHE_DISASSEMBLER
	xc_disassembler_startup_module();
#endif

	return SUCCESS;
}
/* }}} */
static PHP_MSHUTDOWN_FUNCTION(xcache) /* {{{ */
{
	if (old_compile_file && zend_compile_file == xc_check_initial_compile_file) {
		zend_compile_file = old_compile_file;
		old_compile_file = NULL;
	}

	if (xc_coredump_dir && xc_coredump_dir[0]) {
		xcache_restore_crash_handler();
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
	xcache_zend_extension_remove(&xc_zend_extension_entry);
	return SUCCESS;
}
/* }}} */
/* {{{ module dependencies */
#ifdef STANDARD_MODULE_HEADER_EX
static zend_module_dep xcache_module_deps[] = {
	ZEND_MOD_REQUIRED("standard")
	ZEND_MOD_CONFLICTS("apc")
	ZEND_MOD_CONFLICTS("eAccelerator")
	ZEND_MOD_CONFLICTS("Turck MMCache")
	ZEND_MOD_END
};
#endif
/* }}} */ 
/* {{{ module definition structure */
zend_module_entry xcache_module_entry = {
#ifdef STANDARD_MODULE_HEADER_EX
	STANDARD_MODULE_HEADER_EX,
	NULL,
	xcache_module_deps,
#else
	STANDARD_MODULE_HEADER,
#endif
	XCACHE_NAME,
	xcache_functions,
	PHP_MINIT(xcache),
	PHP_MSHUTDOWN(xcache),
	NULL, /* RINIT */
	NULL, /* RSHUTDOWN */
	PHP_MINFO(xcache),
	XCACHE_VERSION,
#ifdef PHP_GINIT
	PHP_MODULE_GLOBALS(xcache),
	PHP_GINIT(xcache),
	PHP_GSHUTDOWN(xcache),
#endif
#ifdef ZEND_ENGINE_2
	NULL /* ZEND_MODULE_POST_ZEND_DEACTIVATE_N */,
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
