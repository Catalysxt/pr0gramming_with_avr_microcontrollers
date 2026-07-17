/*
 * relay_fsm.c
 *
 * Purpose : the 4-state relay controller (see relay_fsm.h).
 * Hardware: one relay-drive pin (pin_t), UART for state reporting.
 * Datasheet: ATmega328P §8 (EEPROM), avr-libc <avr/eeprom.h>.
 *
 * Design notes:
 *  - Two-function FSM pattern (mirrors keypad/anvil/src/anvil.c):
 *      relay_fsm_enter() performs on-entry side effects (drive relay, print,
 *      persist); relay_fsm_tick() decides transitions and runs per-state
 *      background behaviour. Transitions are always explicit enter() calls.
 *  - EEPROM is wear-aware: eeprom_update_byte only burns a cell when the value
 *    actually changed, and LOCKOUT is deliberately never written so that a
 *    reset always escapes it (boots to the last *operational* state).
 */

#include "relay_fsm.h"
#include "sys_tick.h"
#include "USART.h"      /* printString */

#include <avr/eeprom.h> /* eeprom_read_byte / eeprom_update_byte */
#include <stdint.h>
#include <stdbool.h>

/* AUTO toggles the relay at 1 Hz: 1000 ms ON, 1000 ms OFF (matches the
 * relay/hello_world base blink, but scheduled instead of _delay_ms). */
#define AUTO_TOGGLE_MS   1000u

/* EEPROM-backed last operational state. ee_ prefix per naming convention.
 * Initialised to AUTO so a freshly programmed chip has a defined default. */
static uint8_t EEMEM ee_last_state = RELAY_AUTO;

/* Live FSM context. */
static pin_t         s_relay;
static relay_state_t s_state;
static uint32_t      s_auto_anchor = 0;
static bool          s_auto_level  = false;  /* current relay level in AUTO */

static void relay_drive(bool on)
{
    if (on) {
        pin_high(s_relay);
    } else {
        pin_low(s_relay);
    }
}

static const char *state_name(relay_state_t st)
{
    switch (st) {
        case RELAY_AUTO:       return "AUTO";
        case RELAY_MANUAL_ON:  return "MANUAL_ON";
        case RELAY_MANUAL_OFF: return "MANUAL_OFF";
        case RELAY_LOCKOUT:    return "LOCKOUT";
        default:               return "?";
    }
}

/* Perform the on-entry actions for a state: set the relay, persist if this is
 * an operational state, and report the new state over UART. */
static void relay_fsm_enter(relay_state_t st)
{
    s_state = st;

    switch (st) {
        case RELAY_AUTO:
            s_auto_level  = true;            /* start the blink ON        */
            relay_drive(s_auto_level);
            s_auto_anchor = sys_millis();
            break;
        case RELAY_MANUAL_ON:
            relay_drive(true);
            break;
        case RELAY_MANUAL_OFF:
            relay_drive(false);
            break;
        case RELAY_LOCKOUT:
            relay_drive(false);              /* safety: force the load off */
            break;
        default:
            break;
    }

    /* Persist only operational states; LOCKOUT stays volatile so reset clears it. */
    if (st != RELAY_LOCKOUT) {
        eeprom_update_byte(&ee_last_state, (uint8_t)st);
    }

    printString(state_name(st));
    printString("\r\n");
}

/* The last operational state persisted in EEPROM, sanitised to a valid value.
 * Used both at boot and when leaving LOCKOUT (which is never persisted, so this
 * still holds whatever operational state we were in before locking out). Blank
 * EEPROM reads 0xFF; anything that is not AUTO/MANUAL_ON/MANUAL_OFF -> safe AUTO. */
static relay_state_t load_operational_state(void)
{
    relay_state_t st = (relay_state_t)eeprom_read_byte(&ee_last_state);
    if (st != RELAY_AUTO && st != RELAY_MANUAL_ON && st != RELAY_MANUAL_OFF) {
        st = RELAY_AUTO;
    }
    return st;
}

void relay_fsm_init(pin_t relay)
{
    s_relay = relay;
    relay_fsm_enter(load_operational_state());
}

void relay_fsm_tick(button_event_t evt)
{
    /* A long hold (>= 2 s) TOGGLES the LOCKOUT safety state from anywhere:
     *   operational -> LOCKOUT                (relay forced off)
     *   LOCKOUT     -> resume last operational state (restored from EEPROM)
     * This is the only gesture that can leave LOCKOUT — short presses stay
     * ignored while locked out, so it takes a deliberate hold to unlock. */
    if (evt == BTN_LONG) {
        relay_fsm_enter((s_state == RELAY_LOCKOUT) ? load_operational_state()
                                                   : RELAY_LOCKOUT);
        return;
    }

    /* Everything below is operational-only — LOCKOUT ignores it (short presses
     * and the AUTO toggle do nothing until a long hold unlocks). */
    if (s_state == RELAY_LOCKOUT) {
        return;
    }

    /* Short press cycles the operational states. */
    if (evt == BTN_SHORT) {
        switch (s_state) {
            case RELAY_AUTO:       relay_fsm_enter(RELAY_MANUAL_ON);  break;
            case RELAY_MANUAL_ON:  relay_fsm_enter(RELAY_MANUAL_OFF); break;
            case RELAY_MANUAL_OFF: relay_fsm_enter(RELAY_AUTO);       break;
            default:               break;
        }
        return;
    }

    /* AUTO background behaviour: toggle the relay every AUTO_TOGGLE_MS. Runs
     * only while the machine is still in AUTO (a BTN_SHORT above may have moved
     * it elsewhere this pass). */
    if (s_state == RELAY_AUTO) {
        if (sys_elapsed(&s_auto_anchor, AUTO_TOGGLE_MS)) {
            s_auto_level = !s_auto_level;
            relay_drive(s_auto_level);
        }
    }
}
