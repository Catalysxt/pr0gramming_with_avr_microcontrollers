/*
 * button.c
 *
 * Purpose : debounce one active-low push-button and classify each press as a
 *           short tap or a long hold, without ever blocking.
 * Hardware: any GPIO passed as a pin_t; input with internal pull-up.
 * Datasheet: ATmega328P §14.2.1 (pull-up config), §14.2.4 (reading PINx)
 *
 * Model (mirrors keypad/anvil's keypad.c, reduced to a single key):
 *   - Sample the raw level every BTN_SCAN_MS.
 *   - A level must repeat BTN_DEBOUNCE_SCANS times before it is "confirmed".
 *   - On a confirmed press-down edge, start the hold timer.
 *   - If still held BTN_LONGPRESS_MS later -> emit BTN_LONG once and suppress
 *     the tap that would otherwise fire on release.
 *   - Otherwise, on the confirmed release edge -> emit BTN_SHORT.
 */

#include "button.h"
#include "sys_tick.h"

#include <stdint.h>
#include <stdbool.h>

#define BTN_SCAN_MS         5u      /* one raw sample every 5 ms            */
#define BTN_DEBOUNCE_SCANS  3u      /* 3 equal samples = 15 ms stable       */
#define BTN_LONGPRESS_MS    2000u   /* >= 2 s hold => lockout gesture       */

static pin_t    s_pin;
static uint32_t s_scan_anchor = 0;

/* Debounce candidate tracking (raw pressed? + how many equal samples). */
static bool     s_candidate       = false;
static uint8_t  s_candidate_count = 0;

/* Debounced steady state and the hold timer. */
static bool     s_confirmed      = false;
static uint32_t s_press_start_ms = 0;
static bool     s_long_fired     = false;

/* Single-slot pending event. */
static button_event_t s_pending = BTN_NONE;

void button_init(pin_t p)
{
    s_pin = p;
    pin_set_input(s_pin, true);   /* input + internal pull-up (active-low) */
}

void button_service(void)
{
    if (!sys_elapsed(&s_scan_anchor, BTN_SCAN_MS)) {
        return;
    }

    /* Active-low: a LOW reading means pressed. */
    bool raw = !pin_read(s_pin);

    if (raw == s_candidate) {
        if (s_candidate_count < BTN_DEBOUNCE_SCANS) {
            s_candidate_count++;
        }
    } else {
        s_candidate       = raw;
        s_candidate_count = 1;
    }

    if ((s_candidate_count >= BTN_DEBOUNCE_SCANS) && (s_confirmed != s_candidate)) {
        s_confirmed = s_candidate;

        if (s_confirmed) {
            /* Debounced press-down edge: begin timing the hold. */
            s_press_start_ms = sys_millis();
            s_long_fired     = false;
        } else if (!s_long_fired) {
            /* Debounced release that never became a long hold: short tap. */
            s_pending = BTN_SHORT;
        }
    }

    /* While held, promote to a long-press exactly once at the threshold. */
    if (s_confirmed && !s_long_fired) {
        if ((sys_millis() - s_press_start_ms) >= BTN_LONGPRESS_MS) {
            s_long_fired = true;
            s_pending    = BTN_LONG;
        }
    }
}

button_event_t button_consume_event(void)
{
    button_event_t evt = s_pending;
    s_pending = BTN_NONE;
    return evt;
}
