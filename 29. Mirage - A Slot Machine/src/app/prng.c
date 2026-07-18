#include "prng.h"

/* Nonzero default so the generator is usable before seeding and as the fallback
 * when prng_seed(0) is called (xorshift is a fixed point at state 0). */
#define PRNG_DEFAULT_STATE 0x2545F491UL

static uint32_t s_state = PRNG_DEFAULT_STATE;

void prng_seed(uint32_t seed) {
    s_state = (seed != 0) ? seed : PRNG_DEFAULT_STATE;
}

uint32_t prng_next(void) {
    /* Marsaglia's classic xorshift32 triplet (13, 17, 5). */
    uint32_t x = s_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_state = x;
    return x;
}

uint16_t prng_below(uint16_t bound) {
    if (bound == 0) {
        return 0;
    }
    return (uint16_t)(prng_next() % bound);
}
