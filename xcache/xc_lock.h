#ifndef XC_LOCK_H_1913F3DED68715D7CDA5A055E79FE0FF
#define XC_LOCK_H_1913F3DED68715D7CDA5A055E79FE0FF

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef struct _xc_lock_t xc_lock_t;

xc_lock_t *xc_fcntl_init(const char *pathname);
void xc_fcntl_destroy(xc_lock_t *lck);
void xc_fcntl_lock(xc_lock_t *lck);
void xc_fcntl_unlock(xc_lock_t *lck);

#define xc_lock_init(name)  xc_fcntl_init(name)
#define xc_lock_destroy(fd) xc_fcntl_destroy(fd)
#define xc_lock(fd)         xc_fcntl_lock(fd)
#define xc_unlock(fd)       xc_fcntl_unlock(fd)

#endif /* XC_LOCK_H_1913F3DED68715D7CDA5A055E79FE0FF */
