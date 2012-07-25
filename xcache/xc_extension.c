
#include "xc_extension.h"
#include "xcache.h"
#include "util/xc_trace.h"


int xcache_zend_extension_prepend(zend_extension *new_extension) /* {{{ */
{
	zend_extension extension;

	extension = *new_extension;
	extension.handle = 0;

	zend_extension_dispatch_message(ZEND_EXTMSG_NEW_EXTENSION, &extension);

	zend_llist_prepend_element(&zend_extensions, &extension);
	TRACE("%s", "prepended");
	return SUCCESS;
}
/* }}} */
static int xc_ptr_compare_func(void *p1, void *p2) /* {{{ */
{
	return p1 == p2;
}
/* }}} */
static int xc_zend_extension_remove(zend_extension *extension) /* {{{ */
{
	llist_dtor_func_t dtor;

	assert(extension);
	dtor = zend_extensions.dtor; /* avoid dtor */
	zend_extensions.dtor = NULL;
	zend_llist_del_element(&zend_extensions, extension, xc_ptr_compare_func);
	zend_extensions.dtor = dtor;
	return SUCCESS;
}
/* }}} */
int xcache_zend_extension_remove(zend_extension *extension) /* {{{ */
{
	zend_extension *ext = zend_get_extension(extension->name);
	if (!ext) {
		return FAILURE;
	}

	if (ext->shutdown) {
		ext->shutdown(ext);
	}
	xc_zend_extension_remove(ext);
	return SUCCESS;
}
/* }}} */

int xcache_zend_extension_count_by_prefix(zend_llist *l, const char *extension_name_prefix) /* {{{ */
{
	zend_llist_element *element;
	size_t n = strlen(extension_name_prefix);
	int count = 0;

	for (element = zend_extensions.head; element; element = element->next) {
		zend_extension *extension = (zend_extension *) element->data;

		if (strncmp(extension->name, extension_name_prefix, n) == 0) {
			++count;
		}
	}
	return count;
}
/* }}} */
void xcache_zend_extension_unlink_by_prefix(xc_stack_t *linked, zend_llist *l, const char *extension_name_prefix) /* {{{ */
{
	size_t n = strlen(extension_name_prefix);
	zend_llist_element *unlinked = NULL;
	zend_llist_element *element, *next;

	for (element = zend_extensions.head; element; element = next) {
		zend_extension *extension = (zend_extension *) element->data;
		next = element->next;

		if (strncmp(extension->name, extension_name_prefix, n) == 0) {
			xcache_llist_unlink(l, element);
			xc_stack_push(linked, element);
		}
	}

	for (element = zend_extensions.head; element; element = next) {
		zend_extension *extension = (zend_extension *) element->data;
		next = element->next;
	}
}
/* }}} */
void xcache_llist_prepend(zend_llist *l, zend_llist_element *element) /* {{{ */
{
	element->next = l->head;
	element->prev = NULL;
	if (l->head) {
		l->head->prev = element;
	}
	else {
		l->tail = element;
	}
	l->head = element;
	++l->count;
}
/* }}} */
void xcache_llist_unlink(zend_llist *l, zend_llist_element *element) /* {{{ */
{
	if ((element)->prev) {
		(element)->prev->next = (element)->next;
	}
	else {
		(l)->head = (element)->next;
	}

	if ((element)->next) {
		(element)->next->prev = (element)->prev;
	}
	else {
		(l)->tail = (element)->prev;
	}

	--l->count;
}
/* }}} */
