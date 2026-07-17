/*
 * keypad.h
 *
 * 4x4 matrix keypad scanner: debounced press/long-press events.
 * Call keypad_service() every main-loop iteration (it self-gates to a
 * 5 ms scan cadence); drain events with keypad_consume_event().
 */

#ifndef ANVIL_KEYPAD_H_
#define ANVIL_KEYPAD_H_

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

#endif /* ANVIL_KEYPAD_H_ */
