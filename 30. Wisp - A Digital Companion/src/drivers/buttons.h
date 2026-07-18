#ifndef DIGITAL_COMPANION_BUTTONS_H_
#define DIGITAL_COMPANION_BUTTONS_H_

#include <stdint.h>
#include <stdbool.h>
#include "pin.h"

/* Debounced, edge-detected pushbutton with long-press support.
 *
 * The pet button is active-low with the ATmega's internal pull-up (idle = HIGH),
 * so a press pulls the pin LOW. A changed raw level must hold
 * BUTTON_DEBOUNCE_MS consecutive samples before it is trusted. button_poll()
 * MUST be called once per millisecond (paced off the Timer0 tick) -- the
 * settling and hold counters are in units of those calls.
 *
 * Event model (one event per physical press):
 *   - released before the long threshold -> BUTTON_SHORT on release (a "pet")
 *   - held past BUTTON_LONG_PRESS_MS      -> BUTTON_LONG once, mid-press
 * The companion uses SHORT as a pet tap; LONG is available (e.g. a future
 * "sleep now" gesture) but not required by the mood model. */

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

#endif /* DIGITAL_COMPANION_BUTTONS_H_ */
