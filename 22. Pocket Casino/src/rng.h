/*
 * rng.h
 *
 * 16-bit xorshift pseudo-random number generator.
 * Seeded at boot from a floating ADC0 read; reseeded from button-press
 * timestamps to mix in human-timing entropy.
 *
 * ATmega328P datasheet: Section 23 (ADC)
 */

#ifndef RNG_H
#define RNG_H

#include <stdint.h>

/* Seed the xorshift state from one 8-bit ADC0 read on a floating pin.
 * Call once at boot before sei(). */
void rng_init(void);

/* XOR-mix val into the current state — call on every button press with
 * a timestamp (e.g. g_ms_tick cast to uint16_t) for entropy. */
void rng_reseed_mix(uint16_t val);

/* Return the next 16-bit pseudo-random value. */
uint16_t rng_next(void);

/* Convenience: return a value in [0, range). range must be > 0. */
uint8_t rng_u8(uint8_t range);

#endif /* RNG_H */
