/*
 * leds.h
 *
 * Green/red status LED helpers. All blink/pulse timing is driven from
 * sys_millis() polling inside leds_service() — call it every main-loop
 * iteration. No blocking delays.
 */

#ifndef ANVIL_LEDS_H_
#define ANVIL_LEDS_H_

#include <stdint.h>
#include <stdbool.h>

/* Configures both LED pins as outputs, initially off. Call once during init. */
void leds_init(void);

/* Direct on/off control. Cancels any pulse/blink in progress on that LED. */
void leds_green(bool on);
void leds_red(bool on);

/* Starts the red LED blinking at 1/period_ms Hz (toggles every
 * period_ms/2). Pass period_ms == 0 to stop blinking and turn it off.
 * Used for the lockout countdown's 1 Hz slow-blink. */
void leds_red_blink(uint16_t period_ms);

/* Fire-and-forget one-shot pulses: turn the LED on now, auto-off after
 * duration_ms (serviced by leds_service()). A red pulse takes precedence
 * over an in-progress red_blink for its duration. */
void leds_pulse_green(uint16_t duration_ms);
void leds_pulse_red(uint16_t duration_ms);

/* Poll every main-loop iteration to advance blink/pulse timers. */
void leds_service(void);

#endif /* ANVIL_LEDS_H_ */
