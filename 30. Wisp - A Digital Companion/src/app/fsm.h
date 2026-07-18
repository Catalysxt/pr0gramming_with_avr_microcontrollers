#ifndef DIGITAL_COMPANION_FSM_H_
#define DIGITAL_COMPANION_FSM_H_

#include "expression.h"
#include "mood.h"

/* Emotional state machine.
 *
 * The transition graph is an explicit data table of legal edges (see fsm.c), not
 * a switch maze. Each tick fsm_step() asks the mood model which region the
 * current (v, a) point falls in (that is the desired target, already hysteresis-
 * filtered) and then moves the machine one legal step toward it:
 *   - if an edge current -> target exists, take it;
 *   - otherwise route through HESITANT (or NEUTRAL) -- this is what makes an
 *     illegal jump like HAPPY -> NERVOUS pass through HESITANT instead of
 *     snapping across.
 * A few transitions are legal AND abrupt (any state -> SURPRISED, including
 * SLEEPY -> SURPRISED). Every accepted transition calls fsm_on_transition(). */

/* Reset to EXPR_NEUTRAL. */
void fsm_init(void);

/* Advance at most one legal transition toward the mood-implied target and return
 * the (possibly unchanged) current state. Call once per animation tick. */
expression_t fsm_step(mood_t m);

/* The current state. */
expression_t fsm_current(void);

/* Transition hook (logging seam). A no-op unless DEBUG_UART is defined. */
void fsm_on_transition(expression_t from, expression_t to);

#endif /* DIGITAL_COMPANION_FSM_H_ */
