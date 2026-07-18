#ifndef DIGITAL_COMPANION_FACE_H_
#define DIGITAL_COMPANION_FACE_H_

#include <stdint.h>
#include "expression.h"
#include "max7219.h"

/* High-level face API -- the app's single entry point to the display.
 *
 * A thin veneer over the animator so main.c and the FSM speak in intentions
 * ("show HAPPY", "blink") and never touch the frame buffer, tween state, or
 * MAX7221 registers directly. */

/* Bring up the MAX7221, set brightness, and paint the neutral face. */
void face_init(max7219_chain_t chain, uint8_t intensity);

/* Request an expression (tweens there; ignored if already targeted). */
void face_set_expression(expression_t e);

/* Blink now. */
void face_blink(void);

/* Advance animation one tick (call at ANIM_HZ). */
void face_tick(void);

/* Push the current frame to the panel (call at REFRESH_HZ). */
void face_flush(void);

#endif /* DIGITAL_COMPANION_FACE_H_ */
