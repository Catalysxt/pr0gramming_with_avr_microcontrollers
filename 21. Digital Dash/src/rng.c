/*
 * rng.c
 *
 * Purpose : Seed a 16-bit xorshift PRNG from a floating ADC pin.
 * Hardware : ADC2 / PC2 (left unconnected — thermal + capacitive noise).
 * Datasheet: ATmega328P §23.9 (ADC registers), §23.5 (ADC prescaler),
 *            Table 23-5 (ADMUX MUX bits)
 *
 * Why ADC2 / PC2?
 *   PC0 and PC1 are driven by button pull-ups → near-constant voltage.
 *   PC2 is intentionally left floating so the first conversion reads
 *   genuine noise, giving each power cycle a different seed.
 */

#include "rng.h"
#include "hardware_connections.h"

#include <avr/io.h>

/* 16-bit xorshift state — must never be 0. */
static uint16_t s_state = 1u;

void rng_init(void)
{
    /*
     * Select ADC2 (PC2) with AVCC reference.
     * REFS1=0, REFS0=1  → AVCC with external cap on AREF  (Table 23-3)
     * MUX3:0 = 0b0010   → ADC2                             (Table 23-5)
     */
    ADMUX = (1u << REFS0) | (uint8_t)RNG_ADC_CH;

    /*
     * Enable ADC, start single conversion, set prescaler /128.
     * At 16 MHz: ADC clock = 125 kHz — within the 50-200 kHz range for
     * maximum 10-bit accuracy.  (datasheet §23.4)
     * ADEN=1, ADSC=1, ADPS2|ADPS1|ADPS0=1 → /128  (Table 23-5)
     */
    ADCSRA = (1u << ADEN) | (1u << ADSC)
           | (1u << ADPS2) | (1u << ADPS1) | (1u << ADPS0);

    /* Poll until ADSC clears — conversion complete.  (§23.9.2) */
    while (ADCSRA & (1u << ADSC)) { /* wait */ }

    uint16_t seed = ADC;    /* 10-bit result; ADCL must be read first — the
                             * driver handles this via the ADC macro.        */

    /* Disable ADC to save ~350 µA quiescent current.  (§23.9.2) */
    ADCSRA = 0u;
    ADMUX  = 0u;

    /* Seed xorshift — state must not be 0. */
    s_state = (seed != 0u) ? seed : 1u;
}

uint16_t rng_u16(void)
{
    /* Marsaglia xorshift16 — period 2^16-1, passes BigCrush for small state.
     * Constants from: Marsaglia, G. (2003). "Xorshift RNGs". JSS 8(14). */
    s_state ^= (uint16_t)(s_state << 7u);
    s_state ^= (uint16_t)(s_state >> 9u);
    s_state ^= (uint16_t)(s_state << 8u);
    return s_state;
}

uint16_t rng_range(uint16_t lo_inclusive, uint16_t hi_exclusive)
{
    return lo_inclusive + (rng_u16() % (uint16_t)(hi_exclusive - lo_inclusive));
}
