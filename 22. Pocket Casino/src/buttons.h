/*
 * buttons.h
 *
 * Interrupt-driven button driver with 20 ms debounce and 800 ms long-press.
 * Buttons are active-low on PC0 (BTN_A) and PC1 (BTN_B) with internal pull-ups.
 * Uses PCINT1_vect (Pin Change Interrupt 1) for edge detection.
 * Uses the g_ms_tick counter (incremented by Timer0 COMPA ISR) for timing.
 *
 * ATmega328P datasheet:
 *   Section 13.2  — Pin Change Interrupts
 *   Section 13.2.4 — PCICR: Pin Change Interrupt Control Register
 *   Section 13.2.7 — PCMSK1: Pin Change Mask Register 1
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

/* Button events returned by buttons_poll(). */
typedef enum
{
    BTN_NONE    = 0,
    BTN_A_PRESS = 1,   /* BTN_A short press (< 800 ms) */
    BTN_B_PRESS = 2,   /* BTN_B short press            */
    BTN_A_LONG  = 3,   /* BTN_A held >= 800 ms         */
    BTN_B_LONG  = 4    /* BTN_B held >= 800 ms         */
} btn_event_t;

/* Configure PC0/PC1 as inputs with pull-ups; enable PCINT1 group. */
void buttons_init(void);

/* Call once per superloop iteration. Drains one event from the queue.
 * Returns BTN_NONE if no event is pending. */
btn_event_t buttons_poll(void);

#endif /* BUTTONS_H */
