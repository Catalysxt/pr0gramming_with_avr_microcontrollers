/*
 * relay_fsm.h
 *
 * Finite state machine that owns the relay. Four states; transitions are
 * driven by debounced button events (see button.h) and, in AUTO, by time.
 *
 *   RELAY_AUTO       relay toggles at 1 Hz (non-blocking)
 *   RELAY_MANUAL_ON  relay frozen ON
 *   RELAY_MANUAL_OFF relay frozen OFF
 *   RELAY_LOCKOUT    safety state: relay forced OFF, short presses ignored.
 *                    Toggled by a long hold (>= 2 s); a reset also clears it.
 *                    Never persisted to EEPROM.
 *
 * The current state name is printed over UART on every transition.
 */

#ifndef SENTINEL_RELAY_FSM_H_
#define SENTINEL_RELAY_FSM_H_

#include "pin.h"      /* pin_t */
#include "button.h"   /* button_event_t */

typedef enum {
    RELAY_AUTO = 0,
    RELAY_MANUAL_ON,
    RELAY_MANUAL_OFF,
    RELAY_LOCKOUT,
} relay_state_t;

/*
 * Bind the relay output pin, restore the last operational state from EEPROM
 * (falling back to AUTO if the stored byte is blank or was LOCKOUT), and
 * enter it. Call after pin/USART/tick init but before sei() is fine — UART TX
 * is polled, not interrupt-driven.
 */
void relay_fsm_init(pin_t relay);

/*
 * Advance the machine by one superloop pass. Pass the freshly consumed button
 * event (BTN_NONE when there is none). Never blocks.
 */
void relay_fsm_tick(button_event_t evt);

#endif /* SENTINEL_RELAY_FSM_H_ */
