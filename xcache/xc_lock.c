#include "xc_lock.h"
#include "xcache.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef ZEND_WIN32
#	include <process.h>
#else
#	include <unistd.h>
#	include <fcntl.h>
#	include <errno.h>
#endif

/* {{{ detect what type of lock is needed */
#ifdef ZTS
#	define XC_LOCK_NEED_TS
#endif

#ifndef ZEND_WIN32
#	define XC_LOCK_NEED_INTERPROCESS
#endif

#if defined(XC_LOCK_NEED_TS) && defined(XC_LOCK_NEED_INTERPROCESS)
/* allow switching off interprocess support */
#	define XC_LOCK_HAVE_INTERPROCESS_SWITCH
#endif
/* }}} */

/* {{{ detect which lock is needed */
#if defined(XC_LOCK_NEED_TS) && defined(XC_LOCK_NEED_INTERPROCESS)
#	ifdef PTHREADS
#		define XC_LOCK_USE_PTHREAD
#		ifndef _POSIX_THREAD_PROCESS_SHARED
#			define XC_LOCK_USE_FCNTL
#		endif
#	else
#		define XC_LOCK_USE_TSRM
#		define XC_LOCK_USE_FCNTL
#	endif
#elif defined(XC_LOCK_NEED_TS)
#	define XC_LOCK_USE_TSRM
#elif defined(XC_LOCK_NEED_INTERPROCESS)
#	define XC_LOCK_USE_FCNTL
#else
#	define XC_LOCK_USE_NOOP
#endif
/* }}} */

/* {{{ fcntl lock impl */
#ifdef XC_LOCK_USE_FCNTL
#ifndef ZEND_WIN32
typedef int HANDLE;
#	ifndef INVALID_HANDLE_VALUE
#		define INVALID_HANDLE_VALUE -1
#	endif
#else
#	define close(h) CloseHandle(h)
#	define open(filename, mode, permission) CreateFile(filename, \
		GENERIC_READ | GENERIC_WRITE, \
		FILE_SHARE_READ | FILE_SHARE_WRITE, \
		NULL, \
		OPEN_ALWAYS, \
		FILE_ATTRIBUTE_NORMAL, \
		NULL)
#endif

typedef struct {
	HANDLE fd;
	char *pathname;
} xc_fcntl_lock_t;

#ifndef ZEND_WIN32
#	define LCK_WR F_WRLCK
#	define LCK_RD F_RDLCK
#	define LCK_UN F_UNLCK
#	define LCK_NB 0
static inline int dolock(xc_fcntl_lock_t *lck, int type)
{
	int ret;
	struct flock lock;

	lock.l_type = type;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 1;
	lock.l_pid = 0;

	do {
		ret = fcntl(lck->fd, F_SETLKW, &lock);
	} while (ret < 0 && errno == EINTR);
	return ret;
}
#else

#	include <win32/flock.h>
#	include <io.h>
#	include <fcntl.h>
#	include <sys/types.h>
#	include <sys/stat.h>
#	undef errno
#	define errno GetLastError()
#	define getuid() 0
#	define LCK_WR LOCKFILE_EXCLUSIVE_LOCK
#	define LCK_RD 0
#	define LCK_UN 0
#	define LCK_NB LOCKFILE_FAIL_IMMEDIATELY
static inline int dolock(xc_fcntl_lock_t *lck, int type)
{
	static OVERLAPPED offset = {0, 0, 0, 0, NULL};

	if (type == LCK_UN) {
		return UnlockFileEx(lck->fd, 0, 1, 0, &offset);
	}
	else {
		return LockFileEx(lck->fd, type, 0, 1, 0, &offset);
	}
}
#endif
/* }}} */

static zend_bool xc_fcntl_init(xc_fcntl_lock_t *lck, const char *pathname) /* {{{ */
{
	HANDLE fd;
	int size;
	char *myname;

	if (pathname == NULL) {
		static int instanceId = 0;
		const char default_tmpdir[] = { DEFAULT_SLASH, 't', 'm', 'p', '\0' };
		const char *tmpdir;

		tmpdir = getenv("TEMP");
		if (!tmpdir) {
			tmpdir = getenv("TMP");
			if (!tmpdir) {
				tmpdir = default_tmpdir;
			}
		}
		size = strlen(tmpdir) + sizeof("/.xcache.lock") - 1 + 3 * 10 + 100;
		myname = malloc(size);
		snprintf(myname, size - 1, "%s%c.xcache.%d.%d.%d.lock", tmpdir, DEFAULT_SLASH, (int) getuid(), (int) getpid(), ++instanceId);
		pathname = myname;
	}
	else {
		myname = NULL;
	}

	fd = open(pathname, O_RDWR|O_CREAT, 0666);

	if (fd != INVALID_HANDLE_VALUE) {

#ifndef __CYGWIN__
		unlink(pathname);
#endif
		lck->fd = fd;
		size = strlen(pathname) + 1;
		lck->pathname = malloc(size);
		memcpy(lck->pathname, pathname, size);
	}
	else {
		zend_error(E_ERROR, "xc_fcntl_create: open(%s, O_RDWR|O_CREAT, 0666) failed:", pathname);
		lck = NULL;
	}

	if (myname) {
		free(myname);
	}

	return lck ? 1 : 0;
}
/* }}} */
static void xc_fcntl_destroy(xc_fcntl_lock_t *lck) /* {{{ */
{
	close(lck->fd);
#ifdef __CYGWIN__
	unlink(lck->pathname);
#endif
	free(lck->pathname);
}
/* }}} */
static void xc_fcntl_lock(xc_fcntl_lock_t *lck) /* {{{ */
{
	if (dolock(lck, LCK_WR) < 0) {
		zend_error(E_ERROR, "xc_fcntl_lock failed errno:%d", errno);
	}
}
/* }}} */
static void xc_fcntl_rdlock(xc_fcntl_lock_t *lck) /* {{{ */
{
	if (dolock(lck, LCK_RD) < 0) {
		zend_error(E_ERROR, "xc_fcntl_lock failed errno:%d", errno);
	}
}
/* }}} */
static void xc_fcntl_unlock(xc_fcntl_lock_t *lck) /* {{{ */
{
	if (dolock(lck, LCK_UN) < 0) {
		zend_error(E_ERROR, "xc_fcntl_unlock failed errno:%d", errno);
	}
}
/* }}} */
#endif /* XC_LOCK_USE_FCNTL */

struct _xc_lock_t {
#ifdef XC_LOCK_USE_NOOP
	int dummy;
#endif

#ifdef XC_LOCK_USE_TSRM
	MUTEX_T tsrm_mutex;
#endif

#ifdef XC_LOCK_HAVE_INTERPROCESS_SWITCH
	zend_bool interprocess;
#endif

#ifdef XC_LOCK_USE_PTHREAD
	pthread_mutex_t pthread_mutex;
#endif

#ifdef XC_LOCK_USE_FCNTL
	xc_fcntl_lock_t fcntl_lock;
#endif

#ifndef NDEBUG
	zend_bool locked;
#endif
};

#ifdef XC_LOCK_HAVE_INTERPROCESS_SWITCH
#	define XC_LOCK_INTERPROCESS (lck->interprocess)
#else
#	define XC_LOCK_INTERPROCESS 1
#endif

size_t xc_lock_size(void) /* {{{ */
{
	return sizeof(xc_lock_t);
}
/* }}} */
xc_lock_t *xc_lock_init(xc_lock_t *lck, const char *pathname, unsigned char interprocess) /* {{{ */
{
#ifdef XC_LOCK_HAVE_INTERPROCESS_SWITCH
	lck->interprocess = interprocess;
#endif

#ifdef XC_LOCK_USE_PTHREAD
	{
		pthread_mutexattr_t psharedm;
		pthread_mutexattr_init(&psharedm);
		pthread_mutexattr_setpshared(&psharedm, XC_LOCK_INTERPROCESS ? PTHREAD_PROCESS_PRIVATE : PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&lck->pthread_mutex, &psharedm);
	}
#endif

#ifdef XC_LOCK_USE_TSRM
	lck->tsrm_mutex = tsrm_mutex_alloc();
#endif

#ifdef XC_LOCK_USE_FCNTL
	if (XC_LOCK_INTERPROCESS) {
		xc_fcntl_init(&lck->fcntl_lock, pathname);
	}
#endif

#ifndef NDEBUG
	lck->locked = 0;
#endif

	return lck;
}
/* }}} */
void xc_lock_destroy(xc_lock_t *lck) /* {{{ */
{
#ifdef XC_LOCK_USE_PTHREAD
	pthread_mutex_destroy(&lck->pthread_mutex);
#endif

#ifdef XC_LOCK_USE_TSRM
	tsrm_mutex_free(lck->tsrm_mutex);
#endif

#ifdef XC_LOCK_USE_FCNTL
	if (XC_LOCK_INTERPROCESS) {
		xc_fcntl_destroy(&lck->fcntl_lock);
	}
#endif
}
/* }}} */
void xc_lock(xc_lock_t *lck) /* {{{ */
{
#ifdef XC_LOCK_USE_PTHREAD
	if (pthread_mutex_lock(&lck->pthread_mutex) < 0) {
		zend_error(E_ERROR, "xc_lock failed errno:%d", errno);
	}
#endif

#ifdef XC_LOCK_USE_TSRM
	if (tsrm_mutex_lock(lck->tsrm_mutex) < 0) {
		zend_error(E_ERROR, "xc_lock failed errno:%d", errno);
	}
#endif

#ifdef XC_LOCK_USE_FCNTL
	if (XC_LOCK_INTERPROCESS) {
		xc_fcntl_lock(&lck->fcntl_lock);
	}
#endif

#ifndef NDEBUG
	assert(!lck->locked);
	lck->locked = 1;
	assert(lck->locked);
#endif
}
/* }}} */
void xc_rdlock(xc_lock_t *lck) /* {{{ */
{
#ifdef XC_LOCK_USE_PTHREAD
	if (pthread_mutex_lock(&lck->pthread_mutex) < 0) {
		zend_error(E_ERROR, "xc_rdlock failed errno:%d", errno);
	}
#endif

#ifdef XC_LOCK_USE_TSRM
	if (tsrm_mutex_lock(lck->tsrm_mutex) < 0) {
		zend_error(E_ERROR, "xc_rdlock failed errno:%d", errno);
	}
#endif

#ifdef XC_LOCK_USE_FCNTL
	if (XC_LOCK_INTERPROCESS) {
		xc_fcntl_lock(&lck->fcntl_lock);
	}
#endif

#ifndef NDEBUG
	assert(!lck->locked);
	lck->locked = 1;
	assert(lck->locked);
#endif
}
/* }}} */
void xc_unlock(xc_lock_t *lck) /* {{{ */
{
#ifndef NDEBUG
	assert(lck->locked);
	lck->locked = 0;
	assert(!lck->locked);
#endif

#ifdef XC_LOCK_USE_FCNTL
	if (XC_LOCK_INTERPROCESS) {
		xc_fcntl_unlock(&lck->fcntl_lock);
	}
#endif

#ifdef XC_LOCK_USE_TSRM
	if (tsrm_mutex_unlock(lck->tsrm_mutex) < 0) {
		zend_error(E_ERROR, "xc_unlock failed errno:%d", errno);
	}
#endif

#ifdef XC_LOCK_USE_PTHREAD
	if (pthread_mutex_unlock(&lck->pthread_mutex) < 0) {
		zend_error(E_ERROR, "xc_unlock failed errno:%d", errno);
	}
#endif
}
/* }}} */
