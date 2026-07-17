/*
 * rng.c
 *
 * 16-bit xorshift PRNG (Marsaglia, "Xorshift RNGs", 2003).
 * Seed entropy: one ADCH read from ADC0 with the input pin floating.
 * Human-timing entropy: caller XOR-mixes in button-press timestamps.
 *
 * ATmega328P datasheet references:
 *   Section 23.2  — ADC overview
 *   Section 23.9.1 — ADMUX register (MUX[3:0] selects ADC0 = 0b0000)
 *   Section 23.9.2 — ADCSRA register (ADEN, ADSC, ADPS prescaler)
 *   Section 23.9.5 — ADCH: result high byte (ADLAR=1 → 8-bit result in ADCH)
 */

#include "rng.h"
#include <avr/io.h>
#include <util/delay.h>

/* xorshift state — must never be zero */
static uint16_t s_state = 0xACE1U;

void rng_init(void)
{
    /* ADC setup for a single noise sample from ADC0.
     * ADMUX: REFS1:0 = 01 (AVCC reference, Section 23.9.1)
     *        ADLAR   = 1  (left-adjust → 8-bit result in ADCH)
     *        MUX3:0  = 0000 (ADC0 = PC0, Section 23.9.1 Table 23-4) */
    ADMUX = (1U << REFS0) | (1U << ADLAR);

    /* ADCSRA: ADEN=1 (enable), ADPS2:0=111 → /128 prescaler = 125 kHz
     * (datasheet requires 50–200 kHz for full 10-bit accuracy, Section 23.4) */
    ADCSRA = (1U << ADEN) | (1U << ADPS2) | (1U << ADPS1) | (1U << ADPS0);

    /* Start conversion and wait for it to finish.
     * ADSC clears to 0 when done (Section 23.9.2). */
    ADCSRA |= (1U << ADSC);
    while (ADCSRA & (1U << ADSC))
    {
        /* busy-wait */
    }

    /* XOR the floating-pin noise into the state; protect against zero. */
    s_state ^= (uint16_t)ADCH;
    if (s_state == 0U)
    {
        s_state = 0xACE1U;
    }

    /* Disable ADC to save power (can be re-enabled by other subsystems). */
    ADCSRA &= ~(1U << ADEN);
}

void rng_reseed_mix(uint16_t val)
{
    /* Avalanche val into state without risk of landing on zero. */
    s_state ^= val;
    s_state ^= (s_state << 7U);
    s_state ^= (s_state >> 9U);
    s_state ^= (s_state << 8U);
    if (s_state == 0U)
    {
        s_state = 0xACE1U;
    }
}

uint16_t rng_next(void)
{
    /* 16-bit xorshift (period 2^16 - 1, Section 3 of Marsaglia 2003) */
    s_state ^= (s_state << 13U);
    s_state ^= (s_state >> 9U);
    s_state ^= (s_state << 7U);
    return s_state;
}

uint8_t rng_u8(uint8_t range)
{
    /* Bias is acceptable for small ranges on a piezo casino toy. */
    return (uint8_t)(rng_next() % (uint16_t)range);
}
