/**
 * @file brlalg.h
 * @brief Barrel algorithm functions. Used internally.
 */


#ifndef BRL_ALG
#define BRL_ALG

#include <stdint.h>

uint64_t BRL_Hash64(
    const void* key,
    uint64_t len
);

#endif /* BRL_ALG */