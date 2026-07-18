/*
 * ember_fsm.h
 *
 * The Pomodoro state machine. Owns the relay (lamp), the display, the buzzer,
 * and all countdown timing. Driven each superloop pass by the freshly-consumed
 * encoder delta and button gesture. Never blocks.
 *
 *   IDLE       display blank, lamp OFF, MCU sleeps. Any encoder/press wakes
 *              into setup.
 *   SET_WORK   rotate to choose work minutes; tap to confirm, hold to cancel.
 *   SET_BREAK  rotate to choose break minutes; tap to save+start, hold cancel.
 *   WORK       MM:SS counts down, lamp OFF. At 0:00 -> BREAK (see ember_fsm.c
 *              on_work_complete()).
 *   BREAK      MM:SS counts down, lamp ON (the "take a break" signal). At 0:00
 *              -> chime, lamp OFF, back to IDLE.
 *
 *   A long-press in WORK/BREAK aborts back to IDLE (lamp forced OFF).
 */

#ifndef EMBER_FSM_H_
#define EMBER_FSM_H_

#include <stdint.h>
#include <stdbool.h>
#include "pin.h"
#include "button.h"

typedef enum {
    EMBER_IDLE = 0,
    EMBER_SET_WORK,
    EMBER_SET_BREAK,
    EMBER_WORK,
    EMBER_BREAK,
} ember_state_t;

/* Bind the relay pin, load presets from EEPROM, and enter IDLE. Call after
 * display/buzzer/storage init, before sei(). */
void ember_fsm_init(pin_t relay);

/* Advance one superloop pass. enc_delta = detents since last call (CW +),
 * btn = consumed button gesture (BTN_NONE if none). */
void ember_fsm_tick(int8_t enc_delta, button_event_t btn);

/* True only in IDLE — main.c uses this to decide when to power down. */
bool ember_fsm_is_idle(void);

#endif /* EMBER_FSM_H_ */
