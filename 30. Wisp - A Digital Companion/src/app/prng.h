#ifndef DIGITAL_COMPANION_PRNG_H_
#define DIGITAL_COMPANION_PRNG_H_

#include <stdint.h>

/* xorshift32 pseudo-random generator.
 *
 * libc rand() is banned (bloated, and its quality is wasted here); xorshift32 is
 * three shifts and three XORs -- a handful of cycles -- with a full 2^32-1
 * period, ample for idle-motion jitter and saccade direction. The seed is built
 * at boot from a FLOATING ADC pin's noise, then mixed with TCNT1 latched on the
 * first pet-button press, so the sequence depends on real user timing and is not
 * identical every power-up. */

/* Seed the generator (a zero seed is remapped to a nonzero constant). */
void prng_seed(uint32_t seed);

/* Fold extra entropy into the running state (e.g. TCNT1 at first press). */
void prng_add_entropy(uint32_t extra);

/* Next 32-bit value (advances the state). */
uint32_t prng_next(void);

/* Uniform-ish value in [0, n). n must be > 0. */
uint16_t prng_range(uint16_t n);

#endif /* DIGITAL_COMPANION_PRNG_H_ */
