#ifndef XC_UTIL_H_709AE2523EDACB72B54D9CB42DDB0FEE
#define XC_UTIL_H_709AE2523EDACB72B54D9CB42DDB0FEE

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

#define ptradd(type, ptr, ptrdiff) ((type) (((char *) (ptr)) + (ptrdiff)))
#define ptrsub(ptr1, ptr2) (((char *) (ptr1)) - ((char *) (ptr2)))

#endif /* XC_UTIL_H_709AE2523EDACB72B54D9CB42DDB0FEE */
