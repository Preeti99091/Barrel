/**
 * @file brlbool.h
 * @brief Boolean type abstraction for the Barrel library.
 *
 * This header provides a cross-platform definition of a `bool` type.
 * If `BRL_USE_STDBOOL` is **not** defined, it defines `bool`, `true`,
 * and `false` manually for older compilers or freestanding environments.
 */

#ifndef BRL_BOOL
#define BRL_BOOL

#ifdef BRL_USE_STDBOOL
#include <stdbool.h>
#else
typedef int bool;

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif
#endif // BRL_USE_STDBOOL

#endif // BRL_BOOL
