#ifndef XC_INI_H_E208B8E597E7FAD950D249BE9C6B6F53
#define XC_INI_H_E208B8E597E7FAD950D249BE9C6B6F53

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

#include "php.h"
#include "php_ini.h"

PHP_INI_MH(xcache_OnUpdateDummy);
PHP_INI_MH(xcache_OnUpdateULong);
PHP_INI_MH(xcache_OnUpdateBool);
PHP_INI_MH(xcache_OnUpdateString);
#ifndef ZEND_ENGINE_2
#define OnUpdateLong OnUpdateInt
#endif

#endif /* XC_INI_H_E208B8E597E7FAD950D249BE9C6B6F53 */
