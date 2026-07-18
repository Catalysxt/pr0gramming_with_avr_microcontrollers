#include "prng.h"

/* Nonzero default so an un-seeded generator still runs; a zero state is a fixed
 * point for xorshift and must never be allowed. */
static uint32_t s_state = 0xC0FFEEu;

void prng_seed(uint32_t seed) {
    s_state = seed ? seed : 0xC0FFEEu;
}

void prng_add_entropy(uint32_t extra) {
    s_state ^= extra;
    if (s_state == 0) {
        s_state = 0xDEADBEEFu;
    }
}

uint32_t prng_next(void) {
    /* Marsaglia's canonical xorshift32 triple (13, 17, 5). */
    uint32_t x = s_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_state = x;
    return x;
}

uint16_t prng_range(uint16_t n) {
    return (uint16_t)(prng_next() % n);
}
