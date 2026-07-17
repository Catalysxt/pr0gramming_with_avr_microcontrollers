/*
 * rng.h
 *
 * 16-bit xorshift PRNG seeded from a floating ADC pin at boot.
 * ATmega328P datasheet §23 (ADC)
 */

#ifndef DIGITAL_DASH_RNG_H_
#define DIGITAL_DASH_RNG_H_

#include <stdint.h>

/*
 * Perform one ADC conversion on ADC2 (PC2, left floating) and use the
 * 10-bit noise value to seed the xorshift state.
 * Call once before the game loop, after hardware init.
 */
void rng_init(void);

/* Return a pseudo-random 16-bit value. */
uint16_t rng_u16(void);

/*
 * Return a value in [lo_inclusive, hi_exclusive).
 * hi_exclusive must be > lo_inclusive.
 */
uint16_t rng_range(uint16_t lo_inclusive, uint16_t hi_exclusive);

#endif /* DIGITAL_DASH_RNG_H_ */
