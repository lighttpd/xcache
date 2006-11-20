#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <php.h>
#ifndef ZEND_WIN32
typedef int HANDLE;
#	ifndef INVALID_HANDLE_VALUE
#		define INVALID_HANDLE_VALUE -1
#	endif
#	define CloseHandle(h) close(h)
#endif
#include "lock.h"

struct _xc_lock_t {
	HANDLE fd;
	char *pathname;
};

#ifndef ZEND_WIN32
#	include <unistd.h>
#	include <fcntl.h>
#	include <errno.h>
#	define LCK_WR F_WRLCK
#	define LCK_RD F_RDLCK
#	define LCK_UN F_UNLCK
#	define LCK_NB 0
static inline int dolock(xc_lock_t *lck, int type) /* {{{ */
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
/* }}} */
#else

#	include <win32/flock.h>
#	include <io.h>
#	include <fcntl.h>
#	include <sys/types.h>
#	include <sys/stat.h>
#	ifndef errno
#		define errno GetLastError()
#	endif
#	define getuid() 0
#	define LCK_WR LOCKFILE_EXCLUSIVE_LOCK
#	define LCK_RD 0
#	define LCK_UN 0
#	define LCK_NB LOCKFILE_FAIL_IMMEDIATELY
static inline int dolock(xc_lock_t *lck, int type) /* {{{ */
{ 
	static OVERLAPPED offset = {0, 0, 0, 0, NULL};

	if (type == LCK_UN) {
		return UnlockFileEx((HANDLE)lck->fd, 0, 1, 0, &offset);
	}
	else {
		return LockFileEx((HANDLE)lck->fd, type, 0, 1, 0, &offset);
	}
}
/* }}} */
#endif

xc_lock_t *xc_fcntl_init(const char *pathname) /* {{{ */
{
	HANDLE fd;
	xc_lock_t *lck;
	int size;
	char *myname;

	if (pathname == NULL) {
		static int i = 0;
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
		snprintf(myname, size - 1, "%s%c.xcache.%d.%d.%d.lock", tmpdir, DEFAULT_SLASH, (int) getuid(), i ++, rand());
		pathname = myname;
	}
	else {
		myname = NULL;
	}

	fd = (HANDLE) open(pathname, O_RDWR|O_CREAT, 0666);

	if (fd != INVALID_HANDLE_VALUE) {
		lck = malloc(sizeof(lck[0]));

#ifndef __CYGWIN__
		unlink(pathname);
#endif
		lck->fd = fd;
		size = strlen(pathname) + 1;
		lck->pathname = malloc(size);
		memcpy(lck->pathname, pathname, size);
	}
	else {
		fprintf(stderr, "xc_fcntl_create: open(%s, O_RDWR|O_CREAT, 0666) failed:", pathname);
		lck = NULL;
	}

	if (myname) {
		free(myname);
	}

	return lck;
}
/* }}} */
void xc_fcntl_destroy(xc_lock_t *lck) /* {{{ */
{   
	CloseHandle(lck->fd);
#ifdef __CYGWIN__
	unlink(lck->pathname);
#endif
	free(lck->pathname);
	free(lck);
}
/* }}} */
void xc_fcntl_lock(xc_lock_t *lck) /* {{{ */
{   
	if (dolock(lck, LCK_WR) < 0) {
		fprintf(stderr, "xc_fcntl_lock failed errno:%d", errno);
	}
}
/* }}} */
void xc_fcntl_rdlock(xc_lock_t *lck) /* {{{ */
{   
	if (dolock(lck, LCK_RD) < 0) {
		fprintf(stderr, "xc_fcntl_lock failed errno:%d", errno);
	}
}
/* }}} */
void xc_fcntl_unlock(xc_lock_t *lck) /* {{{ */
{   
	if (dolock(lck, LCK_UN) < 0) {
		fprintf(stderr, "xc_fcntl_unlock failed errno:%d", errno);
	}
}
/* }}} */
