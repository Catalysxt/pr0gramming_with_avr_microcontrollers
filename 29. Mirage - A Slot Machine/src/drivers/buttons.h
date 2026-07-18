#ifndef SLOT_MACHINE_BUTTONS_H_
#define SLOT_MACHINE_BUTTONS_H_

#include <stdint.h>
#include <stdbool.h>
#include "pin.h"

/* Debounced, edge-detected pushbutton with long-press support.
 *
 * Generalizes the stopwatch's inline button_pressed() into a reusable module.
 * Buttons are active-low with the ATmega's internal pull-up (idle = HIGH), so a
 * mechanical press pulls the pin LOW. A press bounces for a few ms, so a changed
 * raw level must hold BUTTON_DEBOUNCE_MS consecutive samples before it is
 * trusted. button_poll() MUST be called once per millisecond (paced off the
 * Timer0 tick) -- the settling and hold counters are in units of those calls.
 *
 * Event model (one event per physical press):
 *   - held past BUTTON_LONG_PRESS_MS  -> BUTTON_LONG  emitted once, mid-press
 *   - released before that threshold  -> BUTTON_SHORT emitted on release
 *   - a press that became a LONG emits nothing further on release
 * This split lets one button serve "short = next/back, long = enter settings". */

typedef enum {
    BUTTON_NONE = 0,
    BUTTON_SHORT,
    BUTTON_LONG
} button_event_t;

typedef struct {
    pin_t    pin;
    bool     stable;       /* last confirmed level (true = released) */
    uint8_t  settling_ms;  /* ms the raw level has disagreed with `stable` */
    uint16_t held_ms;      /* ms held down since the confirmed press */
    bool     long_fired;   /* BUTTON_LONG already emitted for this press */
} button_t;

/* Configure the pin as input + pull-up and seed the state as released. */
void button_init(button_t *btn, pin_t pin);

/* Sample once (call every 1 ms). Returns the event for this tick, or BUTTON_NONE. */
button_event_t button_poll(button_t *btn);

#endif /* SLOT_MACHINE_BUTTONS_H_ */
