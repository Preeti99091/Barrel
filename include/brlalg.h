/**
 * @file brlalg.h
 * @brief Barrel algorithm functions. Used internally.
 */


#ifndef BRL_ALG
#define BRL_ALG

#include <stdint.h>
#include "brlextc.h"

BRL_EXTERN_C_START

BRLAPI uint64_t BRL_Hash64(
    const void* key,
    uint64_t len
);

BRL_EXTERN_C_END

#endif /* BRL_ALG */
