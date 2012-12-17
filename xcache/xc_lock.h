#ifndef XC_LOCK_H_1913F3DED68715D7CDA5A055E79FE0FF
#define XC_LOCK_H_1913F3DED68715D7CDA5A055E79FE0FF

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

#include <stdlib.h>

typedef struct _xc_lock_t xc_lock_t;

size_t xc_lock_size(void);
xc_lock_t *xc_lock_init(xc_lock_t *lck, const char *pathname, unsigned char interprocess /* only with ZTS */);
void xc_lock_destroy(xc_lock_t *lck);
void xc_lock(xc_lock_t *lck);
void xc_unlock(xc_lock_t *lck);

#endif /* XC_LOCK_H_1913F3DED68715D7CDA5A055E79FE0FF */
