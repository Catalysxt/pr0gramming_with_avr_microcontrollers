/*
 * ember_fsm.c
 *
 * Purpose : Pomodoro state machine — the glue that turns encoder/button input
 *           and elapsed time into display, buzzer, and lamp behaviour.
 * Hardware: relay pin (pin_t) for the lamp; display/buzzer via their drivers.
 * Timing  : all countdown timing rides sys_elapsed() off the 1 ms Timer0 tick;
 *           nothing here blocks.
 */

#include "ember_fsm.h"
#include "display.h"
#include "buzzer.h"
#include "storage.h"
#include "sys_tick.h"
#include "pin.h"

#define SECS_PER_MIN   60u

static pin_t         s_relay;
static ember_state_t s_state       = EMBER_IDLE;
static uint8_t       s_work_min    = WORK_DEFAULT;
static uint8_t       s_break_min   = BREAK_DEFAULT;

/* Countdown bookkeeping. */
static uint16_t s_remaining_sec = 0;
static uint32_t s_tick_anchor   = 0;   /* sys_millis() anchor for the 1 s tick */
static bool     s_colon_on      = true;

/* ---- small helpers ---- */

static void lamp_on(void)  { pin_high(s_relay); }   /* active-HIGH relay module */
static void lamp_off(void) { pin_low(s_relay); }

/* Clamp v + delta into [lo, hi]. */
static uint8_t adjust(uint8_t v, int8_t delta, uint8_t lo, uint8_t hi)
{
    int16_t n = (int16_t)v + (int16_t)delta;
    if (n < (int16_t)lo) { n = (int16_t)lo; }
    if (n > (int16_t)hi) { n = (int16_t)hi; }
    return (uint8_t)n;
}

static void render_countdown(void)
{
    display_show_mmss((uint8_t)(s_remaining_sec / SECS_PER_MIN),
                      (uint8_t)(s_remaining_sec % SECS_PER_MIN));
    display_set_colon(s_colon_on);
}

static void start_countdown(uint16_t total_sec)
{
    s_remaining_sec = total_sec;
    s_tick_anchor   = sys_millis();
    s_colon_on      = true;
    render_countdown();
}

/* ---- state entries ---- */

static void enter_idle(void)
{
    s_state = EMBER_IDLE;
    lamp_off();
    display_blank();
}

static void enter_set_work(void)
{
    s_state = EMBER_SET_WORK;
    display_show_uint(s_work_min);
}

static void enter_set_break(void)
{
    s_state = EMBER_SET_BREAK;
    display_show_uint(s_break_min);
}

static void enter_work(void)
{
    s_state = EMBER_WORK;
    lamp_off();                                 /* dark head-down work time */
    start_countdown((uint16_t)s_work_min * SECS_PER_MIN);
}

static void enter_break_countdown(void)
{
    s_state = EMBER_BREAK;
    start_countdown((uint16_t)s_break_min * SECS_PER_MIN);
}

/*
 * The work block just reached 0:00 — Ember's whole reason to exist: make the
 * break impossible to ignore. This is the one place with real policy latitude,
 * so it's laid out for you to tune (the default below is a sensible baseline):
 *
 *   - Session counted HERE (work finished) rather than at break end, so an
 *     aborted break still credits the completed work. Move
 *     storage_increment_session() into on_break_complete() if you'd rather
 *     only count fully-rested cycles.
 *   - One bright chime (SFX_BREAK). Swap for buzzer_tone_nb(freq, big_ms) if
 *     you want a longer, harder-to-ignore alarm.
 * Keep enter_break_countdown() last so the break timer starts.
 */
static void on_work_complete(void)
{
    lamp_on();                               /* relay -> desk lamp ON = "take a break" */
    buzzer_play(SFX_BREAK, SFX_BREAK_LEN);   /* bright attention chime                 */
    storage_increment_session();             /* persist one completed work session     */

    enter_break_countdown();                 /* start the BREAK MM:SS countdown         */
}

static void on_break_complete(void)
{
    buzzer_play(SFX_DONE, SFX_DONE_LEN);
    lamp_off();                 /* break's over — lamp goes dark again */
    enter_idle();
}

/* Advance a running countdown by wall-clock; return true when it hits 0. */
static bool countdown_service(void)
{
    if (!sys_elapsed(&s_tick_anchor, 1000u)) {
        return false;
    }
    /* One second elapsed: blink the colon and drop the remaining count. */
    s_colon_on = !s_colon_on;
    if (s_remaining_sec > 0u) {
        s_remaining_sec--;
    }
    render_countdown();
    return (s_remaining_sec == 0u);
}

/* ---- public API ---- */

void ember_fsm_init(pin_t relay)
{
    s_relay = relay;
    pin_set_output(s_relay);
    lamp_off();

    storage_init();
    storage_load_presets(&s_work_min, &s_break_min);
    /* Defend against a corrupt/out-of-range preset. */
    s_work_min  = adjust(s_work_min,  0, WORK_MIN_MIN,  WORK_MIN_MAX);
    s_break_min = adjust(s_break_min, 0, BREAK_MIN_MIN, BREAK_MIN_MAX);

    enter_set_work();  /* DEBUG: show "25" on power-up, no encoder needed.
                          Revert to enter_idle() once the display is verified. */
}

void ember_fsm_tick(int8_t enc_delta, button_event_t btn)
{
    switch (s_state) {

    case EMBER_IDLE:
        /* Any interaction leaves IDLE and begins setup. */
        if ((enc_delta != 0) || (btn != BTN_NONE)) {
            enter_set_work();
        }
        break;

    case EMBER_SET_WORK:
        if (enc_delta != 0) {
            s_work_min = adjust(s_work_min, enc_delta, WORK_MIN_MIN, WORK_MIN_MAX);
            display_show_uint(s_work_min);
        }
        if (btn == BTN_SHORT) {
            buzzer_play(SFX_CONFIRM, SFX_CONFIRM_LEN);
            enter_set_break();
        } else if (btn == BTN_LONG) {
            enter_idle();
        }
        break;

    case EMBER_SET_BREAK:
        if (enc_delta != 0) {
            s_break_min = adjust(s_break_min, enc_delta, BREAK_MIN_MIN, BREAK_MIN_MAX);
            display_show_uint(s_break_min);
        }
        if (btn == BTN_SHORT) {
            storage_save_presets(s_work_min, s_break_min);
            buzzer_play(SFX_CONFIRM, SFX_CONFIRM_LEN);
            enter_work();
        } else if (btn == BTN_LONG) {
            enter_idle();
        }
        break;

    case EMBER_WORK:
        if (btn == BTN_LONG) {          /* abort the session */
            enter_idle();
            break;
        }
        if (countdown_service()) {
            on_work_complete();
        }
        break;

    case EMBER_BREAK:
        if (btn == BTN_LONG) {          /* skip the break */
            on_break_complete();
            break;
        }
        if (countdown_service()) {
            on_break_complete();
        }
        break;

    default:
        enter_idle();
        break;
    }
}

bool ember_fsm_is_idle(void)
{
    return s_state == EMBER_IDLE;
}
