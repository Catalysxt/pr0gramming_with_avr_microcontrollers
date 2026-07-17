/*
 * anvil.c
 *
 * Purpose : Top-level state machine tying keypad/buzzer/leds/storage/ui
 *           together into the full ANVIL safe behaviour.
 * Hardware: none directly — orchestrates the other modules only.
 * Datasheet: n/a (application logic).
 *
 * Design notes:
 *  - PRESS vs LONG: keypad.c already defers KEY_EVT_PRESS to the
 *    debounced release, firing KEY_EVT_LONG instead (once) if the key
 *    was held >= 1000ms — so a long-press never also yields a PRESS.
 *  - Countdown/entry redraws are gated so ui_show_*() is only called when
 *    the on-screen content actually needs to change, not every tick.
 *  - Idle-screensaver: resets on ANY key event (press or long), not just
 *    KEY_EVT_PRESS literally, so a long-press-and-hold gesture can't be
 *    interrupted by the idle timer mid-gesture.
 *  - ST_LOCKOUT is exempted from the idle-screensaver transition: its own
 *    30s countdown is the same order of magnitude as the 30s idle
 *    threshold, and swapping to the screensaver mid-countdown would
 *    freeze the lockout's own tick processing (its case in the switch
 *    below simply wouldn't run any more).
 *  - Waking from the screensaver only ever resumes ST_ENTRY in place;
 *    every other prior state (including the change-PIN sub-flow) wakes
 *    back to ST_SPLASH — resuming a partially-typed current/new PIN
 *    after an idle timeout is a security-sensitive edge case best
 *    avoided by just aborting the flow, consistent with "entry is wiped
 *    on screensaver entry for safety" in the spec.
 */

#include "anvil.h"
#include "keypad.h"
#include "buzzer.h"
#include "leds.h"
#include "storage.h"
#include "ui.h"
#include "sys_tick.h"

#include <stdint.h>
#include <stdbool.h>

#define PIN_LEN                   4u

#define SUBMIT_KEY                'A'
#define BACKSPACE_KEY              'B'
#define CLEAR_KEY                   'C'
#define CHANGE_KEY                   'D'

#define WRONG_ATTEMPT_LIMIT        3u
#define LOCKOUT_DURATION_MS        30000ul
#define UNLOCK_WINDOW_MS           10000ul
#define IDLE_SCREENSAVER_MS        30000ul
#define WRONG_DISPLAY_MS           1000ul
#define DIGIT_REVEAL_MS            200ul
/* Not an exact figure from the spec ("brief success/mismatch display") —
 * a deliberately chosen, readable duration for the change-PIN result
 * screens before returning to splash. */
#define CHANGE_RESULT_DISPLAY_MS   2000ul

#define REVEAL_NONE                0xFFu   /* sentinel: no active reveal slot */
#define SEC_UNSET                  0xFFu    /* sentinel: force next countdown redraw */

typedef struct {
    state_t  state;
    state_t  prev_state;        /* screensaver wake target */
    uint32_t state_entered_ms;
    uint32_t last_activity_ms;  /* last KEY_EVT_PRESS/LONG, drives idle timeout */

    uint8_t  attempts_left;

    char     entry[PIN_LEN];
    uint8_t  entry_pos;
    char     new_pin_buf[PIN_LEN];

    uint8_t  reveal_pos;         /* REVEAL_NONE, or the slot mid-reveal    */
    uint32_t reveal_expiry_ms;
    bool     reveal_pending;     /* still owes a re-mask redraw at expiry  */

    uint8_t  last_shown_sec;     /* countdown redraw gate for UNLOCKED/LOCKOUT */
} anvil_ctx_t;

/* attempts_left must start at WRONG_ATTEMPT_LIMIT, not the 0 that plain
 * BSS zero-init would give it — otherwise the very first wrong PIN typed
 * after boot would look like the 3rd strike and jump straight to
 * lockout. Everything else is fine starting at 0 (state defaults to
 * ST_SPLASH == 0, buffers empty, timers zeroed). */
static anvil_ctx_t g = { .attempts_left = WRONG_ATTEMPT_LIMIT };
static const char *s_entry_prompt = "";

typedef enum { EDIT_NONE, EDIT_CHANGED, EDIT_SUBMIT } edit_result_t;

static void clear_entry(void)
{
    for (uint8_t i = 0; i < PIN_LEN; i++) {
        g.entry[i] = 0;
    }
    g.entry_pos      = 0;
    g.reveal_pos      = REVEAL_NONE;
    g.reveal_expiry_ms = 0;
    g.reveal_pending   = false;
}

/* Builds what ui_show_entry should display at each of the 4 slots: the
 * live digit while its 200ms reveal window is open, '*' once typed and
 * masked, or ' ' for a not-yet-typed slot. */
static void build_masked(char out[PIN_LEN])
{
    for (uint8_t i = 0; i < PIN_LEN; i++) {
        if (i >= g.entry_pos) {
            out[i] = ' ';
        } else if ((i == g.reveal_pos) && (sys_millis() < g.reveal_expiry_ms)) {
            out[i] = g.entry[i];
        } else {
            out[i] = '*';
        }
    }
}

static void refresh_entry_display(void)
{
    char masked[PIN_LEN];
    build_masked(masked);
    ui_show_entry(masked, g.entry_pos, s_entry_prompt);
}

/* Called every tick while in an entry-family state: closes out the
 * reveal window with a re-mask redraw even if no new key was pressed. */
static void service_reveal_expiry(void)
{
    if (g.reveal_pending && (sys_millis() >= g.reveal_expiry_ms)) {
        g.reveal_pending = false;
        refresh_entry_display();
    }
}

/* Digit/B/C/A editing shared by ST_ENTRY and the three change-PIN
 * sub-states. Long-press / D-change-trigger is handled by the caller. */
static edit_result_t handle_entry_key(char ch)
{
    if ((ch >= '0') && (ch <= '9')) {
        if (g.entry_pos < PIN_LEN) {
            g.entry[g.entry_pos] = ch;
            g.reveal_pos        = g.entry_pos;
            g.reveal_expiry_ms  = sys_millis() + DIGIT_REVEAL_MS;
            g.reveal_pending    = true;
            g.entry_pos++;
            return EDIT_CHANGED;
        }
        return EDIT_NONE;   /* entry already full — extra digits ignored */
    }

    if (ch == (char)BACKSPACE_KEY) {
        if (g.entry_pos > 0u) {
            g.entry_pos--;
            g.entry[g.entry_pos] = 0;
            if (g.reveal_pos == g.entry_pos) {
                g.reveal_pending = false;   /* backspaced the revealed slot */
            }
            return EDIT_CHANGED;
        }
        return EDIT_NONE;
    }

    if (ch == (char)CLEAR_KEY) {
        clear_entry();
        return EDIT_CHANGED;
    }

    if (ch == (char)SUBMIT_KEY) {
        if (g.entry_pos == PIN_LEN) {
            return EDIT_SUBMIT;
        }
        buzzer_play(SFX_FAIL, SFX_FAIL_LEN);   /* "ignore and beep error" */
        return EDIT_NONE;
    }

    /* '*', '#', D short-press: reserved, ignored. */
    return EDIT_NONE;
}

static state_t screensaver_wake_target(state_t prev)
{
    if (prev == ST_ENTRY) {
        return ST_ENTRY;
    }
    /* Splash, and every change-PIN sub-state, wake back to splash. */
    return ST_SPLASH;
}

static bool is_entry_family(state_t s)
{
    return (s == ST_ENTRY) || (s == ST_CHANGE_CURR) ||
           (s == ST_CHANGE_NEW) || (s == ST_CHANGE_CONFIRM);
}

void anvil_enter(state_t state)
{
    leds_green(false);
    leds_red(false);

    g.state             = state;
    g.state_entered_ms  = sys_millis();
    g.last_shown_sec    = SEC_UNSET;

    switch (state) {
    case ST_SPLASH:
        ui_show_splash();
        break;

    case ST_ENTRY:
        s_entry_prompt = "Enter PIN:";
        clear_entry();
        refresh_entry_display();
        break;

    case ST_UNLOCKED:
        leds_green(true);
        buzzer_play(SFX_SUCCESS, SFX_SUCCESS_LEN);
        g.last_shown_sec = (uint8_t)(UNLOCK_WINDOW_MS / 1000ul);
        ui_show_unlocked(g.last_shown_sec);
        break;

    case ST_WRONG:
        leds_pulse_red(500u);
        buzzer_play(SFX_FAIL, SFX_FAIL_LEN);
        ui_show_wrong(g.attempts_left);
        break;

    case ST_LOCKOUT:
        leds_red_blink(1000u);
        g.last_shown_sec = (uint8_t)(LOCKOUT_DURATION_MS / 1000ul);
        ui_show_lockout(g.last_shown_sec);
        break;

    case ST_CHANGE_CURR:
        s_entry_prompt = "Current PIN:";
        clear_entry();
        refresh_entry_display();
        break;

    case ST_CHANGE_NEW:
        s_entry_prompt = "New PIN:";
        clear_entry();
        refresh_entry_display();
        break;

    case ST_CHANGE_CONFIRM:
        s_entry_prompt = "Confirm PIN:";
        clear_entry();
        refresh_entry_display();
        break;

    case ST_CHANGE_OK:
        leds_pulse_green((uint16_t)CHANGE_RESULT_DISPLAY_MS);
        buzzer_play(SFX_CHANGE_OK, SFX_CHANGE_OK_LEN);
        ui_show_change_ok();
        break;

    case ST_CHANGE_FAIL:
        leds_pulse_red((uint16_t)CHANGE_RESULT_DISPLAY_MS);
        buzzer_play(SFX_FAIL, SFX_FAIL_LEN);
        ui_show_change_fail();
        break;

    case ST_SCREENSAVER:
        clear_entry();
        ui_screensaver();
        break;

    default:
        break;   /* unreachable — all 11 states are handled above */
    }
}

/* Shared dispatcher for ST_ENTRY and the three change-PIN sub-states. */
static void process_entry_key(key_event_t evt)
{
    if (evt.kind == KEY_EVT_NONE) {
        return;
    }

    if ((evt.kind == KEY_EVT_LONG) && (evt.ch == (char)CHANGE_KEY) && (g.state == ST_ENTRY)) {
        anvil_enter(ST_CHANGE_CURR);
        return;
    }

    if (evt.kind != KEY_EVT_PRESS) {
        return;   /* other long-presses, or D-long outside ST_ENTRY: ignored */
    }

    edit_result_t result = handle_entry_key(evt.ch);

    if (result == EDIT_CHANGED) {
        refresh_entry_display();
        return;
    }
    if (result != EDIT_SUBMIT) {
        return;
    }

    switch (g.state) {
    case ST_ENTRY:
        if (storage_check_pin(g.entry)) {
            g.attempts_left = WRONG_ATTEMPT_LIMIT;
            anvil_enter(ST_UNLOCKED);
        } else {
            if (g.attempts_left > 0u) {
                g.attempts_left--;
            }
            anvil_enter((g.attempts_left == 0u) ? ST_LOCKOUT : ST_WRONG);
        }
        break;

    case ST_CHANGE_CURR:
        anvil_enter(storage_check_pin(g.entry) ? ST_CHANGE_NEW : ST_CHANGE_FAIL);
        break;

    case ST_CHANGE_NEW:
        for (uint8_t i = 0; i < PIN_LEN; i++) {
            g.new_pin_buf[i] = g.entry[i];
        }
        anvil_enter(ST_CHANGE_CONFIRM);
        break;

    case ST_CHANGE_CONFIRM: {
        bool match = true;
        for (uint8_t i = 0; i < PIN_LEN; i++) {
            if (g.entry[i] != g.new_pin_buf[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            storage_set_pin(g.new_pin_buf);
            anvil_enter(ST_CHANGE_OK);
        } else {
            anvil_enter(ST_CHANGE_FAIL);
        }
        break;
    }

    default:
        break;   /* unreachable: only entry-family states reach here */
    }
}

static void service_unlocked(key_event_t evt)
{
    if (evt.kind != KEY_EVT_NONE) {
        g.state_entered_ms = sys_millis();  /* window-extension on any keypress */
        g.last_shown_sec   = SEC_UNSET;
    }

    uint32_t elapsed = sys_millis() - g.state_entered_ms;
    if (elapsed >= UNLOCK_WINDOW_MS) {
        anvil_enter(ST_SPLASH);
        return;
    }

    uint8_t remaining_sec = (uint8_t)(((UNLOCK_WINDOW_MS - elapsed) + 999ul) / 1000ul);
    if (remaining_sec != g.last_shown_sec) {
        g.last_shown_sec = remaining_sec;
        ui_show_unlocked(remaining_sec);
    }
}

static void service_wrong(void)
{
    if ((sys_millis() - g.state_entered_ms) >= WRONG_DISPLAY_MS) {
        anvil_enter(ST_ENTRY);
    }
}

static void service_lockout(void)
{
    uint32_t elapsed = sys_millis() - g.state_entered_ms;
    if (elapsed >= LOCKOUT_DURATION_MS) {
        g.attempts_left = WRONG_ATTEMPT_LIMIT;
        anvil_enter(ST_ENTRY);
        return;
    }

    uint8_t remaining_sec = (uint8_t)(((LOCKOUT_DURATION_MS - elapsed) + 999ul) / 1000ul);
    if (remaining_sec != g.last_shown_sec) {
        g.last_shown_sec = remaining_sec;
        ui_show_lockout(remaining_sec);
        buzzer_play(SFX_LOCKOUT_TICK, SFX_LOCKOUT_TICK_LEN);
    }
}

static void service_change_result(void)
{
    if ((sys_millis() - g.state_entered_ms) >= CHANGE_RESULT_DISPLAY_MS) {
        anvil_enter(ST_SPLASH);
    }
}

void anvil_tick(void)
{
    key_event_t evt = keypad_consume_event();

    if (evt.kind != KEY_EVT_NONE) {
        g.last_activity_ms = sys_millis();
        if (g.state == ST_SCREENSAVER) {
            anvil_enter(screensaver_wake_target(g.prev_state));
            /* fall through: `evt` is now processed against the new state */
        }
    }

    switch (g.state) {
    case ST_SPLASH:
        if (evt.kind != KEY_EVT_NONE) {
            anvil_enter(ST_ENTRY);
            process_entry_key(evt);
            if (is_entry_family(g.state)) {
                service_reveal_expiry();
            }
        }
        break;

    case ST_ENTRY:
    case ST_CHANGE_CURR:
    case ST_CHANGE_NEW:
    case ST_CHANGE_CONFIRM:
        process_entry_key(evt);
        /* process_entry_key may have submitted out of the entry family
         * (e.g. ST_ENTRY -> ST_UNLOCKED) — only chase the reveal-expiry
         * redraw if we're still showing a masked-entry screen, otherwise
         * it would clobber the screen we just transitioned to. */
        if (is_entry_family(g.state)) {
            service_reveal_expiry();
        }
        break;

    case ST_UNLOCKED:
        service_unlocked(evt);
        break;

    case ST_WRONG:
        service_wrong();
        break;

    case ST_LOCKOUT:
        service_lockout();
        break;

    case ST_CHANGE_OK:
    case ST_CHANGE_FAIL:
        service_change_result();
        break;

    case ST_SCREENSAVER:
        break;   /* waking is handled above; nothing else while blanked */

    default:
        anvil_enter(ST_SPLASH);   /* unreachable — defend against corrupted state */
        break;
    }

    if ((g.state != ST_SCREENSAVER) && (g.state != ST_LOCKOUT)) {
        if ((sys_millis() - g.last_activity_ms) >= IDLE_SCREENSAVER_MS) {
            g.prev_state = g.state;
            anvil_enter(ST_SCREENSAVER);
        }
    }
}
