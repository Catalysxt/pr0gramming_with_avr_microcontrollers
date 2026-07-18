/*
 * button.h
 *
 * The KY-040's push-button (SW), debounced, with long-press detection.
 * Active-low (idle HIGH via internal pull-up, pressed = LOW).
 *
 *   BTN_SHORT  confirm current setting / start        (tap)
 *   BTN_LONG   abort a running session back to IDLE    (hold >= 2 s)
 *
 * Debounce is done by a sampling timer (from relay/sentinel/src/button.c):
 * the raw level is sampled every BTN_SCAN_MS and must repeat for
 * BTN_DEBOUNCE_SCANS consecutive samples before the change is accepted. No
 * blocking _delay_* is used — button_service() self-gates off sys_elapsed().
 */

#ifndef EMBER_BUTTON_H_
#define EMBER_BUTTON_H_

#include "pin.h"   /* pin_t */

/* One button gesture, drained by button_consume_event(). */
typedef enum {
    BTN_NONE = 0,   /* nothing since last consume            */
    BTN_SHORT,      /* debounced press released before long   */
    BTN_LONG,       /* held >= BTN_LONGPRESS_MS (fires once)   */
} button_event_t;

/* Configure the pin as input with pull-up and reset internal state. */
void button_init(pin_t p);

/* Call every superloop pass. Self-gates to one raw sample per BTN_SCAN_MS. */
void button_service(void);

/* Return the pending gesture (BTN_NONE if none) and clear it. */
button_event_t button_consume_event(void);

#endif /* EMBER_BUTTON_H_ */
