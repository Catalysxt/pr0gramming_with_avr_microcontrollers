#include "buttons.h"

/* A changed raw level must hold this many 1 ms samples to be accepted. 15 ms
 * comfortably outlasts contact bounce (typically < 5 ms) without felt lag. */
#define BUTTON_DEBOUNCE_MS   15u

/* Hold time that promotes a press to a long-press (~800 ms). */
#define BUTTON_LONG_PRESS_MS 800u

#define BUTTON_RELEASED true    /* active-low: idle/released reads HIGH */
#define BUTTON_PRESSED  false

void button_init(button_t *btn, pin_t pin) {
    btn->pin         = pin;
    btn->stable      = BUTTON_RELEASED;
    btn->settling_ms = 0;
    btn->held_ms     = 0;
    btn->long_fired  = false;
    pin_set_input(pin, true);   /* input + internal pull-up -> idles HIGH */
}

button_event_t button_poll(button_t *btn) {
    bool level = pin_read(btn->pin);

    if (level == btn->stable) {
        btn->settling_ms = 0;                 /* steady: raw agrees with confirmed */

        if (btn->stable == BUTTON_PRESSED && !btn->long_fired) {
            if (++btn->held_ms >= BUTTON_LONG_PRESS_MS) {
                btn->long_fired = true;        /* fire LONG once, still held down */
                return BUTTON_LONG;
            }
        }
        return BUTTON_NONE;
    }

    /* Raw level disagrees with the confirmed state -- it may be a real change or
     * just bounce. Only commit once it has held steady long enough. */
    if (++btn->settling_ms >= BUTTON_DEBOUNCE_MS) {
        btn->stable      = level;
        btn->settling_ms = 0;

        if (level == BUTTON_PRESSED) {         /* released -> pressed edge */
            btn->held_ms    = 0;
            btn->long_fired = false;
            return BUTTON_NONE;                /* classify short/long on release */
        }
        /* pressed -> released edge: a short press only if it never became long. */
        return btn->long_fired ? BUTTON_NONE : BUTTON_SHORT;
    }

    return BUTTON_NONE;                        /* still settling */
}
