/*
 * anvil.h
 *
 * Top-level ANVIL state machine: splash -> PIN entry -> unlocked / wrong /
 * lockout, plus the guarded change-PIN sub-flow. Ties together keypad,
 * buzzer, leds, storage, and ui. No blocking waits anywhere.
 */

#ifndef ANVIL_ANVIL_H_
#define ANVIL_ANVIL_H_

typedef enum {
    ST_SPLASH = 0,
    ST_ENTRY,
    ST_UNLOCKED,
    ST_WRONG,
    ST_LOCKOUT,
    ST_CHANGE_CURR,
    ST_CHANGE_NEW,
    ST_CHANGE_CONFIRM,
    ST_CHANGE_OK,
    ST_CHANGE_FAIL,
    ST_SCREENSAVER
} state_t;

/*
 * Transitions immediately to `state`: resets its deadline, resets the
 * LEDs, redraws its screen, and fires whatever entry-time SFX/LEDs that
 * state calls for. main() calls this once with ST_SPLASH after all
 * peripherals are initialised and sei() has run.
 */
void anvil_enter(state_t state);

/*
 * Consumes at most one keypad event and advances every state-machine
 * timer (entry deadlines, countdowns, reveal-then-mask, idle screensaver).
 * Call every superloop iteration — never blocks.
 */
void anvil_tick(void);

#endif /* ANVIL_ANVIL_H_ */
