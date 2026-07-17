/*
 * ascent.h
 *
 * Top-level ASCENT state machine: splash -> chained int32 arithmetic
 * (digit entry / operator commit / equals) -> result, with an overflow /
 * divide-by-zero error freeze and transparent idle-sleep. Ties together
 * keypad, buzzer, calc, and ui. No blocking waits anywhere.
 */

#ifndef ASCENT_ASCENT_H_
#define ASCENT_ASCENT_H_

#include <stdbool.h>

typedef enum {
    ST_SPLASH,      /* boot screen, waiting for first keypress             */
    ST_FRESH,       /* no operand yet, expression cleared, result = 0     */
    ST_OPERAND,     /* operand currently being typed                     */
    ST_OPERATOR,    /* operator just committed, waiting for next operand  */
    ST_RESULT,      /* '=' pressed; any key wipes and restarts            */
    ST_ERROR,       /* OVERFLOW or DIV BY ZERO; 1.5s freeze, then reset   */
    ST_SLEEP_PEND   /* unused: sleep is state-preserving via the accessors
                       below rather than a real FSM state; kept only for
                       spec-shape parity with prompt.md's enum. */
} state_t;

/*
 * Transitions immediately to `state`: resets its entry timestamp and
 * redraws its screen. No SFX here — every SFX decision is made at the
 * point a key is accepted, in the per-state handlers inside ascent.c.
 * main() calls this once with ST_SPLASH after all peripherals are
 * initialised and sei() has run.
 */
void ascent_enter(state_t state);

/*
 * Consumes at most one keypad event and advances the error-freeze timer.
 * Call every superloop iteration — never blocks.
 */
void ascent_tick(void);

/*
 * Checks ascent.c's private 30s idle-activity anchor (reset on every
 * accepted keypress). Returns true exactly once, on the tick where the
 * idle threshold is crossed, UNLESS the calculator is in ST_ERROR (the
 * 1.5s freeze must never be interrupted by a sleep transition). main()
 * calls sleep_enter_power_down() only when this returns true.
 */
bool ascent_check_idle_sleep(void);

/*
 * Pure repaint of whatever ui_show_* matches the CURRENT state, using the
 * CURRENT values — no SFX, no timestamp reset, no state change. Distinct
 * from ascent_enter(), which is for real transitions. main() calls this
 * once, immediately after sleep_enter_power_down() returns, so the screen
 * that was showing before sleep reappears unchanged.
 */
void ascent_redraw(void);

#endif /* ASCENT_ASCENT_H_ */
