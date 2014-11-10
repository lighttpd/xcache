typedef struct _xc_processor_t {
	char *p;
	size_t size;
	HashTable zvalptrs;
	zend_bool handle_reference; /* enable if to deal with reference */
	zend_bool have_references;
	ptrdiff_t relocatediff;

	const xc_entry_php_t *entry_php_src;
	const xc_entry_php_t *entry_php_dst;
	const xc_entry_data_php_t *php_src;
	const xc_entry_data_php_t *php_dst;
	const zend_class_entry *cache_ce;
	zend_ulong cache_class_index;

	const zend_op_array    *active_op_array_src;
	zend_op_array          *active_op_array_dst;
	const zend_class_entry *active_class_entry_src;
	zend_class_entry       *active_class_entry_dst;
	zend_uint                 active_class_index;
	zend_uint                 active_op_array_index;
	const xc_op_array_info_t *active_op_array_infos_src;

	zend_bool readonly_protection; /* wheather it's present */

	#include "processor/string-helper-t.h"
	#include "processor/var-helper-t.h"

#ifdef HAVE_XCACHE_TEST
	xc_vector_t allocsizes;
#endif
} xc_processor_t;
