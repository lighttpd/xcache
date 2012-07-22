#include "xc_ini.h"

PHP_INI_MH(xcache_OnUpdateDummy)
{
	return SUCCESS;
}

PHP_INI_MH(xcache_OnUpdateULong)
{
	zend_ulong *p = (zend_ulong *) mh_arg1;

	*p = (zend_ulong) atoi(new_value);
	return SUCCESS;
}

PHP_INI_MH(xcache_OnUpdateBool)
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

PHP_INI_MH(xcache_OnUpdateString)
{
	char **p = (char**)mh_arg1;
	if (*p) {
		pefree(*p, 1);
	}
	*p = pemalloc(strlen(new_value) + 1, 1);
	strcpy(*p, new_value);
	return SUCCESS;
}
