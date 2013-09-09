#ifndef XC_MUTEX_H_1913F3DED68715D7CDA5A055E79FE0FF
#define XC_MUTEX_H_1913F3DED68715D7CDA5A055E79FE0FF

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

#include <stdlib.h>

typedef struct _xc_mutex_t xc_mutex_t;

size_t xc_mutex_size(void);
xc_mutex_t *xc_mutex_init(xc_mutex_t *shared_mutex, const char *pathname, unsigned char want_inter_process);
void xc_mutex_destroy(xc_mutex_t *mutex);
void xc_mutex_lock(xc_mutex_t *mutex);
void xc_mutex_unlock(xc_mutex_t *mutex);

#endif /* XC_MUTEX_H_1913F3DED68715D7CDA5A055E79FE0FF */
