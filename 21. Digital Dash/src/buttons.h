/*
 * buttons.h
 *
 * Debounced button driver for JUMP (PC0) and DUCK (PC1).
 * Both buttons are active-low with internal pull-ups.
 * Shared PCINT1_vect services both lines.
 *
 * ATmega328P datasheet §13 (pin-change interrupts), §14.2 (pull-ups)
 */

#ifndef DIGITAL_DASH_BUTTONS_H_
#define DIGITAL_DASH_BUTTONS_H_

#include <stdbool.h>

/*
 * Configure PC0/PC1 as inputs with pull-ups and enable PCINT1 interrupts.
 * Call before sei().
 */
void buttons_init(void);

/*
 * Returns true and clears the one-shot flag if JUMP was pressed since the
 * last call. Used by the game to trigger a single jump per press.
 */
bool buttons_consume_jump_edge(void);

/*
 * Returns the current debounced state of the DUCK button.
 * true  = button held (player ducking)
 * false = button released
 */
bool buttons_ducking(void);

#endif /* DIGITAL_DASH_BUTTONS_H_ */
