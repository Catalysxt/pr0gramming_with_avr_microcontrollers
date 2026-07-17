/*
 * sys_tick.c
 *
 * Purpose : 1 ms system timebase using Timer0 in CTC mode.
 * Hardware: Timer0 (no GPIO touched).
 * Datasheet: ATmega328P §15.7 (CTC mode), §15.9.2 (TIMER0_COMPA_vect)
 *
 * Sleep interaction: sleep.c calls power_timer0_disable()/
 * power_timer0_enable() (<avr/power.h>) around SLEEP_MODE_PWR_DOWN. Those
 * gate Timer0's clock via PRR without touching TCCR0A/TCCR0B/OCR0A/TIMSK0,
 * so no re-call to sys_tick_init() is needed after wake — enabling the
 * clock alone resumes ticking at the same 1 kHz configuration. g_ms simply
 * does not advance for the sleep duration; that's a benign one-time jump
 * in absolute time, fine here because every consumer only ever uses
 * sys_millis() for relative deadlines (sys_elapsed anchors), never wall time.
 */

#include "sys_tick.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>    /* ATOMIC_BLOCK — safe multi-byte read of g_ms */

/* Incremented by ISR every 1 ms. volatile: written from an ISR, read from
 * main context. Not declared in the header — sys_millis() is the only
 * sanctioned accessor so every reader goes through the atomic wrapper. */
static volatile uint32_t g_ms = 0;

void sys_tick_init(void)
{
    /*
     * Timer0 CTC mode: counter resets to 0 when TCNT0 == OCR0A.
     * WGM01 = 1, WGM00 = 0  ->  CTC  (datasheet Table 15-8)
     *
     * Desired tick: 1 ms @ F_CPU = 16 MHz
     *   prescaler 64  ->  f_timer = 16e6 / 64 = 250 kHz
     *   OCR0A = (250000 / 1000) - 1 = 249
     *
     * CS01|CS00 = prescaler /64  (datasheet Table 15-9)
     */
    TCCR0A = (1u << WGM01);                /* CTC mode                */
    TCCR0B = (1u << CS01) | (1u << CS00);  /* prescaler /64            */
    OCR0A  = 249u;                          /* 1 ms period              */
    TIMSK0 = (1u << OCIE0A);               /* enable compare-match A   */
}

/* ISR fires every 1 ms. Keep it minimal — just bump the counter. */
ISR(TIMER0_COMPA_vect)
{
    g_ms++;
}

uint32_t sys_millis(void)
{
    uint32_t t;
    /* ATOMIC_BLOCK prevents a torn read of the 4-byte counter on an 8-bit
     * CPU. ATOMIC_RESTORESTATE preserves the I flag so this is safe both
     * before and after sei(). (avr-libc util/atomic.h) */
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        t = g_ms;
    }
    return t;
}

bool sys_elapsed(uint32_t *anchor, uint32_t period_ms)
{
    if ((sys_millis() - *anchor) >= period_ms) {
        *anchor += period_ms;   /* advance by the period, not to "now" — avoids drift */
        return true;
    }
    return false;
}
