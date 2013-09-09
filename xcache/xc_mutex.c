#include "xc_mutex.h"
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

/* {{{ detect what type of mutex is needed */
#ifdef ZTS
#	define XC_MUTEX_NEED_TS
#endif

#ifdef HAVE_FORK
#	define XC_MUTEX_NEED_INTERPROCESS
#endif

#if defined(XC_MUTEX_NEED_TS) && defined(XC_MUTEX_NEED_INTERPROCESS)
/* the check is not even needed for non-threaded env */
#	define XC_MUTEX_HAVE_INTERPROCESS_SWITCH
#endif
/* }}} */

/* {{{ detect which mutex is needed */
#if defined(XC_MUTEX_NEED_TS) && defined(XC_MUTEX_NEED_INTERPROCESS)
#	ifdef PTHREADS
#		define XC_MUTEX_USE_PTHREAD
#		ifndef _POSIX_THREAD_PROCESS_SHARED
#			define XC_MUTEX_USE_FCNTL
#		endif
#	else
#		define XC_MUTEX_USE_TSRM
#		define XC_MUTEX_USE_FCNTL
#	endif
#elif defined(XC_MUTEX_NEED_TS)
#	define XC_MUTEX_USE_TSRM
#elif defined(XC_MUTEX_NEED_INTERPROCESS)
#	define XC_MUTEX_USE_FCNTL
#else
#	define XC_MUTEX_USE_NOOP
#endif
/* }}} */

/* {{{ fcntl mutex impl */
#ifdef XC_MUTEX_USE_FCNTL
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
#ifdef __CYGWIN__
	/* store the path for unlink() later */
	char *pathname;
#endif
} xc_fcntl_mutex_t;

#ifndef ZEND_WIN32
#	define LCK_WR F_WRLCK
#	define LCK_RD F_RDLCK
#	define LCK_UN F_UNLCK
#	define LCK_NB 0
static inline int dolock(xc_fcntl_mutex_t *fcntl_mutex, int type)
{
	int ret;
	struct flock lock;

	lock.l_type = type;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 1;
	lock.l_pid = 0;

	do {
		ret = fcntl(fcntl_mutex->fd, F_SETLKW, &lock);
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
static inline int dolock(xc_fcntl_mutex_t *fcntl_mutex, int type)
{
	static OVERLAPPED offset = {0, 0, 0, 0, NULL};

	if (type == LCK_UN) {
		return UnlockFileEx(fcntl_mutex->fd, 0, 1, 0, &offset);
	}
	else {
		return LockFileEx(fcntl_mutex->fd, type, 0, 1, 0, &offset);
	}
}
#endif
/* }}} */

static zend_bool xc_fcntl_init(xc_fcntl_mutex_t *fcntl_mutex, const char *pathname) /* {{{ */
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
		size = strlen(tmpdir) + sizeof("/.xcache.mutex") - 1 + 3 * 10 + 100;
		myname = malloc(size);
		snprintf(myname, size - 1, "%s%c.xcache.%d.%d.%d.mutex", tmpdir, DEFAULT_SLASH, (int) getuid(), (int) getpid(), ++instanceId);
		pathname = myname;
	}
	else {
		myname = NULL;
	}

	fd = open(pathname, O_RDWR|O_CREAT, 0666);

	if (fd != INVALID_HANDLE_VALUE) {
		fcntl_mutex->fd = fd;
#ifdef __CYGWIN__
		size = strlen(pathname) + 1;
		fcntl_mutex->pathname = malloc(size);
		memcpy(fcntl_mutex->pathname, pathname, size);
#else
		unlink(pathname);
#endif
	}
	else {
		zend_error(E_ERROR, "xc_fcntl_create: open(%s, O_RDWR|O_CREAT, 0666) failed:", pathname);
		fcntl_mutex = NULL;
	}

	if (myname) {
		free(myname);
	}

	return fcntl_mutex ? 1 : 0;
}
/* }}} */
static void xc_fcntl_destroy(xc_fcntl_mutex_t *fcntl_mutex) /* {{{ */
{
	close(fcntl_mutex->fd);
#ifdef __CYGWIN__
	unlink(fcntl_mutex->pathname);
	free(fcntl_mutex->pathname);
#endif
}
/* }}} */
static void xc_fcntl_mutex(xc_fcntl_mutex_t *fcntl_mutex) /* {{{ */
{
	if (dolock(fcntl_mutex, LCK_WR) < 0) {
		zend_error(E_ERROR, "xc_fcntl_mutex failed errno:%d", errno);
	}
}
/* }}} */
static void xc_fcntl_rdlock(xc_fcntl_mutex_t *fcntl_mutex) /* {{{ */
{
	if (dolock(fcntl_mutex, LCK_RD) < 0) {
		zend_error(E_ERROR, "xc_fcntl_mutex failed errno:%d", errno);
	}
}
/* }}} */
static void xc_fcntl_unlock(xc_fcntl_mutex_t *fcntl_mutex) /* {{{ */
{
	if (dolock(fcntl_mutex, LCK_UN) < 0) {
		zend_error(E_ERROR, "xc_fcntl_unlock failed errno:%d", errno);
	}
}
/* }}} */
#endif /* XC_MUTEX_USE_FCNTL */

struct _xc_mutex_t {
#ifdef XC_MUTEX_USE_NOOP
	int dummy;
#endif

#ifdef XC_MUTEX_HAVE_INTERPROCESS_SWITCH
	zend_bool want_inter_process;  /* whether the lock is created for inter process usage */
#endif
	zend_bool shared; /* shared, or locally allocated */

#ifdef XC_MUTEX_USE_TSRM
	MUTEX_T tsrm_mutex;
#endif

#ifdef XC_MUTEX_USE_PTHREAD
	pthread_mutex_t pthread_mutex;
#endif

#ifdef XC_MUTEX_USE_FCNTL
	xc_fcntl_mutex_t fcntl_mutex;
#endif

#ifndef NDEBUG
	zend_bool locked;
#endif
};

#ifdef XC_MUTEX_HAVE_INTERPROCESS_SWITCH
#	define xc_want_inter_process() (mutex->want_inter_process)
#elif defined(HAVE_FORK)
#	define xc_want_inter_process() 1
#else
#	define xc_want_inter_process() 0
#endif

size_t xc_mutex_size(void) /* {{{ */
{
	return sizeof(xc_mutex_t);
}
/* }}} */
xc_mutex_t *xc_mutex_init(xc_mutex_t *const shared_mutex, const char *pathname, unsigned char want_inter_process) /* {{{ */
{
	xc_mutex_t *mutex = NULL;

#ifndef HAVE_FORK
	want_inter_process = 0;
#endif

	/* if interprocessed is needed, shared_mutex is required to be a pre-allocated memory on shm
	 * this function can always return non-shared memory if necessary despite shared memory is given
	 */

	/* when inter-process is wanted, pthread lives in shm */
#ifdef XC_MUTEX_USE_PTHREAD
	if (want_inter_process) {
		assert(shared_mutex);
		mutex = shared_mutex;
		mutex->shared = 1;
	}
	else
#endif
	{
		/* all other mutex assumed live locally */
		mutex = calloc(1, sizeof(*mutex));
		mutex->shared = 0;
#ifdef XC_MUTEX_HAVE_INTERPROCESS_SWITCH
		mutex->want_inter_process = want_inter_process;
#endif
	}


#ifdef XC_MUTEX_USE_PTHREAD
	{
		/* If you see mutex leak using valgrind, see xc_mutex_destroy function */
		pthread_mutexattr_t psharedm;
		pthread_mutexattr_init(&psharedm);
		pthread_mutexattr_setpshared(&psharedm, xc_want_inter_process() ? PTHREAD_PROCESS_PRIVATE : PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&mutex->pthread_mutex, &psharedm);
	}
#endif

#ifdef XC_MUTEX_USE_TSRM
	mutex->tsrm_mutex = tsrm_mutex_alloc();
#endif

#ifdef XC_MUTEX_USE_FCNTL
	if (xc_want_inter_process()) {
		xc_fcntl_init(&mutex->fcntl_mutex, pathname);
	}
#endif

#ifndef NDEBUG
	mutex->locked = 0;
#endif

	return mutex;
}
/* }}} */
void xc_mutex_destroy(xc_mutex_t *mutex) /* {{{ */
{
	assert(mutex);

	/* intended to not destroy mutex when mutex is shared between process */
	if (!mutex->shared) {
#ifdef XC_MUTEX_USE_PTHREAD
		pthread_mutex_destroy(&mutex->pthread_mutex);
#endif

#ifdef XC_MUTEX_USE_TSRM
		tsrm_mutex_free(mutex->tsrm_mutex);
#endif
	}

#ifdef XC_MUTEX_USE_FCNTL
	if (xc_want_inter_process()) {
		xc_fcntl_destroy(&mutex->fcntl_mutex);
	}
#endif

	if (!mutex->shared) {
		free(mutex);
	}
}
/* }}} */
void xc_mutex_lock(xc_mutex_t *mutex) /* {{{ */
{
#ifdef XC_MUTEX_USE_PTHREAD
	if (pthread_mutex_lock(&mutex->pthread_mutex) < 0) {
		zend_error(E_ERROR, "xc_mutex failed errno:%d", errno);
	}
#endif

#ifdef XC_MUTEX_USE_TSRM
	if (tsrm_mutex_lock(mutex->tsrm_mutex) < 0) {
		zend_error(E_ERROR, "xc_mutex failed errno:%d", errno);
	}
#endif

#ifdef XC_MUTEX_USE_FCNTL
	if (xc_want_inter_process()) {
		xc_fcntl_mutex(&mutex->fcntl_mutex);
	}
#endif

#ifndef NDEBUG
	assert(!mutex->locked);
	mutex->locked = 1;
	assert(mutex->locked);
#endif
}
/* }}} */
void xc_mutex_unlock(xc_mutex_t *mutex) /* {{{ */
{
#ifndef NDEBUG
	assert(mutex->locked);
	mutex->locked = 0;
	assert(!mutex->locked);
#endif

#ifdef XC_MUTEX_USE_FCNTL
	if (xc_want_inter_process()) {
		xc_fcntl_unlock(&mutex->fcntl_mutex);
	}
#endif

#ifdef XC_MUTEX_USE_TSRM
	if (tsrm_mutex_unlock(mutex->tsrm_mutex) < 0) {
		zend_error(E_ERROR, "xc_mutex_unlock failed errno:%d", errno);
	}
#endif

#ifdef XC_MUTEX_USE_PTHREAD
	if (pthread_mutex_unlock(&mutex->pthread_mutex) < 0) {
		zend_error(E_ERROR, "xc_mutex_unlock failed errno:%d", errno);
	}
#endif
}
/* }}} */
