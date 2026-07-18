#include <avr/io.h>
#include <avr/interrupt.h>   /* ISR() */
#include <util/atomic.h>     /* ATOMIC_BLOCK -- torn-free 32-bit read */
#include "timing.h"

/* Timer0 CTC math: at F_CPU = 16 MHz with the /64 prescaler the timer clock is
 * 250 kHz, so a compare-match every (250000/1000) = 250 counts is exactly 1 ms.
 * OCR0A is TOP, so we program TOP = 250 - 1 = 249. Integer-exact -> no drift. */
#define TIMER0_PRESCALE 64u
#define TICK_HZ         1000u
#define TIMER0_TOP      ((uint8_t)((F_CPU / TIMER0_PRESCALE / TICK_HZ) - 1u))  /* 249 */

static volatile uint32_t s_ticks_ms = 0;

void timing_init(void) {
    /* WGM01 = CTC (TOP = OCR0A). CS01|CS00 = clk/64. */
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS01) | (1 << CS00);
    OCR0A  = TIMER0_TOP;
    TIMSK0 = (1 << OCIE0A);              /* enable compare-match A interrupt */
}

ISR(TIMER0_COMPA_vect) {
    s_ticks_ms++;
}

uint32_t timing_ms(void) {
    uint32_t now;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {  /* 32-bit read is 4 byte-ops on AVR */
        now = s_ticks_ms;
    }
    return now;
}
