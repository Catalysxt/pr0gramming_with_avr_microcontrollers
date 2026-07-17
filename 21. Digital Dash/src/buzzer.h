/*
 * buzzer.h
 *
 * Non-blocking tone generator using Timer2 CTC on OC2A (PB3).
 * Call buzzer_service() from the main superloop to auto-stop expired tones.
 *
 * ATmega328P datasheet §17 (Timer2/Counter2)
 */

#ifndef DIGITAL_DASH_BUZZER_H_
#define DIGITAL_DASH_BUZZER_H_

#include <stdint.h>

/*
 * Configure PB3 as output and start a tone at freq_hz for duration_ms.
 * Returns immediately — tone runs in hardware.
 * Safe to call from an ISR (no blocking, no ATOMIC_BLOCK needed).
 */
void buzzer_tone_nb(uint16_t freq_hz, uint16_t duration_ms);

/*
 * Poll in the main superloop. Silences the buzzer once its duration expires.
 */
void buzzer_service(void);

/*
 * Immediately silence the buzzer and tristate PB3.
 */
void buzzer_off(void);

#endif /* DIGITAL_DASH_BUZZER_H_ */
