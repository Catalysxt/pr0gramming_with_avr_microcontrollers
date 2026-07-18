#ifndef SLOT_MACHINE_TIMING_H_
#define SLOT_MACHINE_TIMING_H_

#include <stdint.h>

/* 1 ms system tick on Timer0 (CTC).
 *
 * The stopwatch used Timer1 for its tick; the slot machine needs Timer1 for the
 * buzzer (CTC-toggle on OC1A), so the tick moves to the 8-bit Timer0. Everything
 * time-based in the app -- button debounce, reel-stop timers, melody note
 * durations, count-up animations -- is paced off timing_ms(), so the main loop
 * and ISRs never call _delay_ms().
 *
 * avr-libc used: <avr/interrupt.h> ISR(), <util/atomic.h> for a torn-free read
 * of the 32-bit counter (the .c owns those includes). */

/* Start Timer0 as a 1 ms CTC time base and enable its compare-A interrupt.
 * Call once at boot; the caller must sei() to actually run the tick. */
void timing_init(void);

/* Milliseconds since timing_init(). Wraps after ~49.7 days (uint32_t) -- far
 * beyond any play session. Safe to call with interrupts enabled: the read is
 * guarded so a mid-update tick cannot tear the 32-bit value. */
uint32_t timing_ms(void);

#endif /* SLOT_MACHINE_TIMING_H_ */
