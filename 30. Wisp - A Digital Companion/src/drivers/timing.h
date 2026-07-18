#ifndef DIGITAL_COMPANION_TIMING_H_
#define DIGITAL_COMPANION_TIMING_H_

#include <stdint.h>

/* 1 ms system tick on Timer0 (CTC).
 *
 * Timer0 is chosen for the tick so Timer1 is left entirely free for the
 * HC-SR04 input-capture unit (ICP1). Everything time-based in the companion --
 * button debounce, the 25 Hz animation tick, the 75 Hz display refresh, mood
 * decay, sensor cadence -- is paced off timing_ms(), so the main loop and ISRs
 * never call _delay_ms().
 *
 * avr-libc used: <avr/interrupt.h> ISR(), <util/atomic.h> for a torn-free read
 * of the 32-bit counter (the .c owns those includes). */

/* Start Timer0 as a 1 ms CTC time base and enable its compare-A interrupt.
 * Call once at boot; the caller must sei() to actually run the tick. */
void timing_init(void);

/* Milliseconds since timing_init(). Wraps after ~49.7 days (uint32_t). Safe to
 * call with interrupts enabled: the read is guarded so a mid-update tick cannot
 * tear the 32-bit value. */
uint32_t timing_ms(void);

#endif /* DIGITAL_COMPANION_TIMING_H_ */
