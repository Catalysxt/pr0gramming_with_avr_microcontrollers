/*
 * sleep.h
 *
 * SLEEP_MODE_PWR_DOWN entry/exit for the IDLE state. Wake source is the
 * encoder's PCINT2 group (CLK/DT/SW), which encoder_init() already enabled —
 * so this module owns no ISR of its own; ISR(PCINT2_vect) in encoder.c both
 * decodes the wake-turn and returns the core to life.
 *
 * State-preserving: sleep_enter_power_down() stops the display, silences the
 * buzzer, gates the peripheral clocks, sleeps, then restores everything and
 * returns. The FSM state is untouched, so IDLE is still IDLE after wake.
 *
 * ATmega328P datasheet §9.5-9.6 (Sleep / Power-down), §12.2 (PCINT)
 */

#ifndef EMBER_SLEEP_H_
#define EMBER_SLEEP_H_

/*
 * Enter SLEEP_MODE_PWR_DOWN. Blocks until an encoder turn or SW press fires
 * PCINT2 and wakes the core, at which point it restores the display and
 * peripheral clocks and returns. Call only when the FSM is IDLE and the
 * buzzer is quiet.
 */
void sleep_enter_power_down(void);

#endif /* EMBER_SLEEP_H_ */
