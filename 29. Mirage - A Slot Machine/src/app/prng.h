#ifndef SLOT_MACHINE_PRNG_H_
#define SLOT_MACHINE_PRNG_H_

#include <stdint.h>

/* xorshift32 pseudo-random generator (Marsaglia). Chosen over libc rand() per
 * the project spec: it is tiny, has no static-init/heap cost, and its 32-bit
 * state and single-word output are ideal for an 8-bit MCU. Period 2^32 - 1.
 *
 * Seeding: the app reads a floating ADC pin at boot for a noisy seed, then mixes
 * in TCNT1 captured at the instant of the first spin so the sequence depends on
 * human timing (see game.c). The state must never be zero -- xorshift is stuck
 * at 0 -- so prng_seed() substitutes a constant if handed 0. */

/* Seed the generator (0 is remapped to a nonzero constant). */
void prng_seed(uint32_t seed);

/* Next 32-bit pseudo-random value. */
uint32_t prng_next(void);

/* Uniform-ish value in [0, bound). Returns 0 if bound == 0. Modulo bias is
 * negligible for the small bounds used here (symbol counts, stop jitter). */
uint16_t prng_below(uint16_t bound);

#endif /* SLOT_MACHINE_PRNG_H_ */
