/*
 * sleep.h
 *
 * SLEEP_MODE_PWR_DOWN entry/exit with PCINT8-11 wake on the keypad
 * columns. State-preserving: sleep_enter_power_down() blocks until a
 * keypress wakes the core, then restores every peripheral it touched and
 * returns — it never resets or even looks at ascent.c's state, so
 * whatever screen/state was active before sleep is still active after.
 * The caller (main.c) is responsible for repainting that screen post-wake
 * (see ascent_redraw() in ascent.h) — sleep.c deliberately does not
 * #include ascent.h, keeping the dependency graph one-directional.
 */

#ifndef ASCENT_SLEEP_H_
#define ASCENT_SLEEP_H_

/* Enables PCINT8-11 (PCMSK1/PCICR). Call once before sei(). */
void sleep_init(void);

/*
 * Blanks the LCD, silences the buzzer, drives the keypad rows LOW, gates
 * Timer0/Timer2/ADC, then enters SLEEP_MODE_PWR_DOWN. Blocks until any
 * keypress fires PCINT1_vect and wakes the core, at which point it
 * restores every peripheral and the keypad rows before returning.
 */
void sleep_enter_power_down(void);

#endif /* ASCENT_SLEEP_H_ */
