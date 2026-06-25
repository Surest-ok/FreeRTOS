/*
 * stddef.h for AArch64 bare-metal
 *
 * Provides correct 64-bit definitions that the cross-compiler's
 * stddef.h may get wrong due to sysroot interference.
 */
#ifndef _STDDEF_H
#define _STDDEF_H

/* Guard against GCC's stddef.h redefinitions */
#define ___int_size_t_h
#define _SIZE_T
#define __size_t

typedef long unsigned int  size_t;
typedef long int           ptrdiff_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef offsetof
#define offsetof(type, member) ((size_t)&(((type *)0)->member))
#endif

typedef unsigned int wchar_t;

#endif /* _STDDEF_H */
