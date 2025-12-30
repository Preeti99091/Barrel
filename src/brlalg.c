#include "brlalg.h"

uint64_t BRL_Hash64(
    const void* key,
    uint64_t len
) {
    const uint8_t* p = key;
    uint64_t h = 0x9E3779B185EBCA87ULL;
    for (uint64_t i = 0; i < len; i++) {
        h = (h ^ p[i]) * 0x100000001b3ULL;
    }
    return h ^ (h >> 33);
}
