#ifndef XC_EXTENSION_H_9885D3A6DE7C469D13E34AF331E02BB8
#define XC_EXTENSION_H_9885D3A6DE7C469D13E34AF331E02BB8

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

#include "util/xc_stack.h"
#include "zend_extensions.h"
int xcache_zend_extension_prepend(zend_extension *new_extension);
int xcache_zend_extension_remove(zend_extension *extension);
int xcache_zend_extension_count_by_prefix(zend_llist *l, const char *extension_name_prefix);
void xcache_zend_extension_unlink_by_prefix(xc_stack_t *unlinked, zend_llist *l, const char *extension_name_prefix);

void xcache_llist_prepend(zend_llist *l, zend_llist_element *element);
void xcache_llist_unlink(zend_llist *l, zend_llist_element *element);

#endif /* XC_EXTENSION_H_9885D3A6DE7C469D13E34AF331E02BB8 */
