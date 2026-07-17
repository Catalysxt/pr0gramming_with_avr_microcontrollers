/*
 * keypad.h
 *
 * 4x4 matrix keypad scanner: debounced press/long-press events.
 * Call keypad_service() every main-loop iteration (it self-gates to a
 * 5 ms scan cadence); drain events with keypad_consume_event().
 *
 * Unlike Anvil's keypad.c, this module never plays audio itself — Ascent's
 * top-level state machine (ascent.c) decides which SFX to play per key
 * CLASS (digit/operator/equals/clear/error), a decision keypad.c has no
 * way to make on its own. Don't "fix" queue_press() by re-adding a
 * buzzer_play() call.
 */

#ifndef ASCENT_KEYPAD_H_
#define ASCENT_KEYPAD_H_

typedef enum {
    KEY_EVT_NONE = 0,
    KEY_EVT_PRESS,
    KEY_EVT_LONG
} key_event_kind_t;

typedef struct {
    key_event_kind_t kind;
    char             ch;
} key_event_t;

/* Configures column pins as pulled-up inputs and row pins as outputs
 * idling high. Call once during init. */
void keypad_init(void);

/* Runs the scan + debounce state machine. Self-gated to a 5 ms cadence
 * via sys_millis() — safe (and expected) to call every superloop pass. */
void keypad_service(void);

/* Returns and clears the next pending event, or {KEY_EVT_NONE, 0} if
 * nothing is pending. */
key_event_t keypad_consume_event(void);

/* Drives all 4 row pins LOW. Called by sleep.c immediately before
 * SLEEP_MODE_PWR_DOWN — any keypress then pulls its column low and fires
 * PCINT8-11 (PCINT1_vect), waking the core. */
void keypad_rows_sleep(void);

/* Restores all 4 row pins to their normal idle-HIGH scan state. Called by
 * sleep.c right after waking (and by keypad_init() itself at boot, so the
 * idle-high setup lives in exactly one place). */
void keypad_rows_wake(void);

#endif /* ASCENT_KEYPAD_H_ */
