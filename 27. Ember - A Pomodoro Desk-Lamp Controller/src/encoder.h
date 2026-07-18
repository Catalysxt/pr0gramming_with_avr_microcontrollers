/*
 * encoder.h
 *
 * KY-040 rotary encoder: quadrature decoding driven by pin-change interrupts
 * (PCINT2). A transition-table decoder in the ISR rejects contact bounce and
 * accumulates whole detents; the main loop drains them with
 * encoder_consume_delta(). The ISR (ISR(PCINT2_vect)) is shared with the
 * push-button and the power-down wake source — encoder.c is its sole owner.
 *
 * Datasheet: KY-040 (docs/KY-040.pdf, Gray-code truth tables),
 *            ATmega328P §12.2 (PCINT), §12.2.4 (PCMSK2), §12.2.6 (PCIFR)
 */

#ifndef EMBER_ENCODER_H_
#define EMBER_ENCODER_H_

#include <stdint.h>
#include "pin.h"

/* Configure CLK/DT as inputs with pull-ups, seed the decoder state, and
 * enable PCINT2 on both. Call once before sei(). */
void encoder_init(pin_t clk, pin_t dt);

/* Return the net number of detents turned since the last call (clockwise
 * positive) and reset the accumulator. Typically -3..+3 per superloop pass.
 * Atomic w.r.t. the ISR. */
int8_t encoder_consume_delta(void);

#endif /* EMBER_ENCODER_H_ */
