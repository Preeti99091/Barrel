/**
 * @file brlstdlib.h
 * @brief Barrel standard library abstraction layer.
 *
 * This header defines lightweight wrapper macros for common
 * memory and string operations. The purpose of these macros is
 * to provide a single point of customization for memory allocation
 * and utility functions in environments that may lack the C standard library.
 *
 * If `BRL_NO_STD_LIB` is defined, you can provide your own implementations
 * of `malloc`, `free`, `memcpy`, `memset`, and `strlen` before including this file.
 */

#ifndef BRL_STDLIB
#define BRL_STDLIB

#ifndef BRL_NO_STD_LIB
#include <stdlib.h>
#include <string.h>
#endif

/* Allocation */
#ifndef BRL_MALLOC
#define BRL_MALLOC(size) malloc(size)
#endif

#ifndef BRL_FREE
#define BRL_FREE(ptr) free(ptr)
#endif

/* Memory copying and setting */
#ifndef BRL_MEMCPY
#define BRL_MEMCPY(dest, src, n) memcpy(dest, src, n)
#endif

#ifndef BRL_MEMSET
#define BRL_MEMSET(ptr, val, n) memset(ptr, val, n)
#endif

/* String handling */
#ifndef BRL_STRLEN
#define BRL_STRLEN(s) strlen(s)
#endif

#endif /* BRL_STDLIB */
