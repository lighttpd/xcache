#ifndef __ALIGN_H
#define __ALIGN_H
#ifndef ALIGN
typedef union align_union {
	double d;
	void *v;
	int (*func)(int);
	long l;
} align_union;

#if (defined (__GNUC__) && __GNUC__ >= 2)
#define XCACHE_PLATFORM_ALIGNMENT (__alignof__ (align_union))
#else
#define XCACHE_PLATFORM_ALIGNMENT (sizeof(align_union))
#endif

#define ALIGN(n) ((((size_t)(n)-1) & ~(XCACHE_PLATFORM_ALIGNMENT-1)) + XCACHE_PLATFORM_ALIGNMENT)
#endif
#endif /* __ALIGN_H */
