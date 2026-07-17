/*
 * ascent.c
 *
 * Purpose : Top-level state machine tying keypad/buzzer/calc/ui together
 *           into the full ASCENT calculator behaviour: strict left-to-
 *           right chained int32 arithmetic, overflow/div-zero error
 *           freeze, and transparent idle-sleep.
 * Hardware: none directly — orchestrates the other modules only.
 * Datasheet: n/a (application logic).
 *
 * Design notes:
 *  - Every SFX decision is made HERE, at the point a key is accepted —
 *    never in keypad.c — since only this file knows the key's semantic
 *    CLASS (digit/operator/equals/clear/error), not just its ASCII value.
 *  - ST_SPLASH and ST_RESULT both reduce to "wipe to ST_FRESH, then
 *    re-dispatch the same key" (handle_key() below) — one re-dispatch
 *    implements every rule in both transition tables (any key wipes; a
 *    digit re-seeds into ST_OPERAND; '*' plays the clear SFX via
 *    handle_fresh's own branch; operator/'=' get the error beep) with
 *    zero duplicated logic.
 *  - The operator-commit math (running_result vs. op_pending==NONE) is a
 *    single shared function, commit_operand_into_running_result(), used
 *    by BOTH the operator-key path and the '=' path in ST_OPERAND. A
 *    second hand-written copy would be exactly the kind of place the
 *    overflow check could silently diverge between the two call sites.
 *  - KEY_EVT_LONG is never consumed anywhere below — long-press is
 *    reserved for a stretch feature (signed-operand negate); keypad.c's
 *    API stays symmetric so that feature costs nothing to add later.
 *  - Idle-sleep bookkeeping (g_last_key_ms) stays private to this file;
 *    main.c never sees the raw anchor or g_state, only the two accessors
 *    declared in ascent.h. This keeps sleep.c decoupled from ascent.h
 *    too (main.c is the only thing that ever calls ascent_redraw()).
 */

#include "ascent.h"
#include "keypad.h"
#include "buzzer.h"
#include "calc.h"
#include "ui.h"
#include "sys_tick.h"

#include <stdint.h>
#include <stdbool.h>

#define KEY_CLEAR         '*'
#define KEY_EQUALS        '#'
#define IDLE_SLEEP_MS      30000ul
#define ERROR_FREEZE_MS     1500ul

/* What ST_FRESH displays on row 1: a synthetic single '0', bit-for-bit
 * identical to what typing '0' as the very first digit would produce —
 * so handle_fresh() needs no leading-zero special case of its own (see
 * its comment below). Shared by ascent_enter() and ascent_redraw(). */
static const operand_buf_t k_fresh_operand_display = { .digits = { '0' }, .len = 1u };

static int32_t       g_running_result;
static calc_op_t     g_op_pending;
static operand_buf_t g_operand;
static char          g_expr[64];
static uint8_t       g_expr_len;
static state_t       g_state;
static uint32_t      g_state_entered_ms;
static uint32_t      g_last_key_ms;   /* private idle-activity anchor */

static char op_glyph(calc_op_t op)
{
    switch (op) {
    case OP_ADD: return '+';
    case OP_SUB: return '-';
    case OP_MUL: return 'x';
    case OP_DIV: return '/';
    default:     return '?';   /* unreachable: only armed ops reach here */
    }
}

static calc_op_t key_to_op(char ch)
{
    switch (ch) {
    case 'A': return OP_ADD;
    case 'B': return OP_SUB;
    case 'C': return OP_MUL;
    case 'D': return OP_DIV;
    default:  return OP_NONE;
    }
}

static bool is_digit_key(char ch)
{
    return (ch >= '0') && (ch <= '9');
}

static bool is_operator_key(char ch)
{
    return key_to_op(ch) != OP_NONE;
}

/* Appends one glyph to the expression buffer, silently stopping once the
 * 63-char clamp is hit (spec: "quietly stop appending if the user goes
 * wild" — 1 byte reserved of the 64 isn't needed since expr_len alone
 * gates every read, but keeps the invariant expr_len <= sizeof(g_expr)). */
static void expr_append_char(char c)
{
    if (g_expr_len < sizeof(g_expr)) {
        g_expr[g_expr_len++] = c;
    }
}

/*
 * Called only while already in ST_OPERAND (g_operand.len >= 1 always).
 * Implements: "typing 0 then 5 displays 5" (replace-in-place) and
 * "00 collapses to 0" (silent no-op — not an error, no SFX, unlike the
 * digit-cap-exceeded case below which DOES beep).
 *
 * Relies on the invariant that, throughout ST_OPERAND, g_expr's last
 * character is always exactly g_operand's last digit: every digit typed
 * since the operand started is appended to both buffers in lockstep, and
 * no operator glyph is appended until commit time (which also leaves
 * ST_OPERAND). That invariant is what makes the in-place replace below
 * safe to mirror into g_expr with a single index write.
 */
static void operand_append_with_leading_zero_rule(char digit)
{
    if ((g_operand.len == 1u) && (g_operand.digits[0] == '0')) {
        if (digit == '0') {
            return;   /* "00" -> stays "0", silent no-op */
        }
        g_operand.digits[0]      = digit;   /* "0" -> digit, replace in place */
        g_expr[g_expr_len - 1u]  = digit;   /* mirror the same replace into g_expr */
        buzzer_play(SFX_DIGIT, SFX_DIGIT_LEN);
        return;
    }

    if (!operand_append_digit(&g_operand, digit)) {
        buzzer_play(SFX_ERROR, SFX_ERROR_LEN);   /* digit cap (10) reached */
        return;
    }
    expr_append_char(digit);
    buzzer_play(SFX_DIGIT, SFX_DIGIT_LEN);
}

/*
 * Shared commit math for both the operator-key path and '=' in
 * ST_OPERAND (prompt.md's "Commit rule" box): if no operator is armed
 * yet, the typed operand simply becomes the running result; otherwise
 * apply the armed operator against the running result. new_op is what
 * gets armed afterward — the real operator glyph for an operator key,
 * or OP_NONE for '=' (nothing left to arm once the expression ends).
 */
static void commit_operand_into_running_result(calc_op_t new_op, bool *ok, calc_status_t *status)
{
    int32_t operand_val = operand_to_int(&g_operand);

    if (g_op_pending == OP_NONE) {
        g_running_result = operand_val;
        *ok = true;
    } else {
        calc_status_t st = calc_apply(g_op_pending, g_running_result, operand_val, &g_running_result);
        *ok     = (st == CALC_OK);
        *status = st;
    }
    g_op_pending = new_op;
}

static void ascent_enter_impl(state_t state)
{
    g_state            = state;
    g_state_entered_ms = sys_millis();

    switch (state) {
    case ST_SPLASH:
        ui_show_splash();
        break;

    case ST_FRESH:
        g_running_result = 0;
        g_op_pending     = OP_NONE;
        operand_clear(&g_operand);
        g_expr_len = 0u;
        ui_show_expr_and_operand(g_expr, g_expr_len, &k_fresh_operand_display);
        break;

    case ST_OPERAND:
        ui_show_expr_and_operand(g_expr, g_expr_len, &g_operand);
        break;

    case ST_OPERATOR:
        ui_show_expr_and_result(g_expr, g_expr_len, g_running_result, true);
        break;

    case ST_RESULT:
        ui_show_expr_and_result(g_expr, g_expr_len, g_running_result, false);
        break;

    case ST_ERROR:
        /* Message/SFX supplied by enter_error() below, which calls this
         * with state == ST_ERROR and then draws the specific screen —
         * ascent_enter()'s state_t-only signature can't carry which
         * error occurred. */
        break;

    default:
        break;   /* ST_SLEEP_PEND: unreachable, see ascent.h */
    }
}

void ascent_enter(state_t state)
{
    ascent_enter_impl(state);
}

static void enter_error(calc_status_t status)
{
    ascent_enter_impl(ST_ERROR);
    if (status == CALC_ERR_OVERFLOW) {
        ui_show_error("OVERFLOW", "Press * to reset");
    } else {
        ui_show_error("DIV BY ZERO", "Press * to reset");
    }
    buzzer_play(SFX_ERROR, SFX_ERROR_LEN);
}

/*
 * ST_FRESH handler. No leading-zero special case is needed here: typing
 * '0' as the very first digit produces {digits:{'0'}, len:1} — bit-for-
 * bit identical to k_fresh_operand_display already on screen, so there's
 * no visible change and no double-'0' risk. The "disallow multiple
 * leading zeros" rule only has teeth once a SECOND digit is typed, which
 * is operand_append_with_leading_zero_rule's job once in ST_OPERAND.
 */
static void handle_fresh(char ch)
{
    if (is_digit_key(ch)) {
        operand_clear(&g_operand);
        (void)operand_append_digit(&g_operand, ch);
        expr_append_char(ch);
        ascent_enter_impl(ST_OPERAND);
        buzzer_play(SFX_DIGIT, SFX_DIGIT_LEN);
    } else if (ch == KEY_CLEAR) {
        buzzer_play(SFX_CLEAR, SFX_CLEAR_LEN);   /* state already clean; just the SFX */
    } else {
        buzzer_play(SFX_ERROR, SFX_ERROR_LEN);   /* operator or '=': no operand yet */
    }
}

static void handle_operand(char ch)
{
    if (is_digit_key(ch)) {
        operand_append_with_leading_zero_rule(ch);   /* handles cap-beep/replace/collapse + redraw below */
        ui_show_expr_and_operand(g_expr, g_expr_len, &g_operand);
        return;
    }

    if (is_operator_key(ch)) {
        bool          ok     = false;
        calc_status_t status = CALC_OK;
        calc_op_t     new_op = key_to_op(ch);

        commit_operand_into_running_result(new_op, &ok, &status);
        if (!ok) {
            enter_error(status);
            return;
        }
        expr_append_char(op_glyph(new_op));
        operand_clear(&g_operand);
        ascent_enter_impl(ST_OPERATOR);
        buzzer_play(SFX_OPERATOR, SFX_OPERATOR_LEN);
        return;
    }

    if (ch == KEY_EQUALS) {
        bool          ok     = false;
        calc_status_t status = CALC_OK;

        /* '=' reuses the exact same commit math with new_op = OP_NONE —
         * nothing is left to arm once the expression ends. */
        commit_operand_into_running_result(OP_NONE, &ok, &status);
        if (!ok) {
            enter_error(status);
            return;
        }
        expr_append_char('=');
        ascent_enter_impl(ST_RESULT);
        buzzer_play(SFX_EQUALS, SFX_EQUALS_LEN);
        return;
    }

    if (ch == KEY_CLEAR) {
        ascent_enter_impl(ST_FRESH);
        buzzer_play(SFX_CLEAR, SFX_CLEAR_LEN);
    }
}

static void handle_operator(char ch)
{
    if (is_digit_key(ch)) {
        operand_clear(&g_operand);
        (void)operand_append_digit(&g_operand, ch);
        expr_append_char(ch);
        ascent_enter_impl(ST_OPERAND);
        buzzer_play(SFX_DIGIT, SFX_DIGIT_LEN);
        return;
    }

    if (is_operator_key(ch)) {
        /* Operator-replace: the operand buffer is still empty here, so
         * swap the last glyph already written to g_expr for the new
         * operator's glyph instead of appending a second one. E.g.
         * "5+" with 'B'(-) pressed becomes "5-", not "5+-". */
        calc_op_t new_op = key_to_op(ch);
        g_op_pending = new_op;
        g_expr[g_expr_len - 1u] = op_glyph(new_op);
        ascent_enter_impl(ST_OPERATOR);   /* re-enter is draw-only here; safe, refreshes state_entered_ms */
        buzzer_play(SFX_OPERATOR, SFX_OPERATOR_LEN);
        return;
    }

    if (ch == KEY_EQUALS) {
        buzzer_play(SFX_ERROR, SFX_ERROR_LEN);   /* no second operand yet */
        return;
    }

    if (ch == KEY_CLEAR) {
        ascent_enter_impl(ST_FRESH);
        buzzer_play(SFX_CLEAR, SFX_CLEAR_LEN);
    }
}

static void handle_key(key_event_t evt)
{
    if (evt.kind != KEY_EVT_PRESS) {
        return;   /* KEY_EVT_NONE and KEY_EVT_LONG both ignored in v1 */
    }
    char ch = evt.ch;

    if ((g_state == ST_SPLASH) || (g_state == ST_RESULT)) {
        ascent_enter_impl(ST_FRESH);
        handle_fresh(ch);   /* re-dispatch: the waking/wiping key isn't lost */
        return;
    }

    switch (g_state) {
    case ST_FRESH:    handle_fresh(ch);    break;
    case ST_OPERAND:  handle_operand(ch);  break;
    case ST_OPERATOR: handle_operator(ch); break;
    case ST_ERROR:    /* ignore unconditionally; service_error() owns the auto-reset */ break;
    default:          break;   /* ST_SLEEP_PEND: unreachable */
    }
}

static void service_error(void)
{
    if ((g_state == ST_ERROR) && ((sys_millis() - g_state_entered_ms) >= ERROR_FREEZE_MS)) {
        ascent_enter_impl(ST_FRESH);
    }
}

void ascent_tick(void)
{
    key_event_t evt = keypad_consume_event();

    if (evt.kind == KEY_EVT_PRESS) {
        g_last_key_ms = sys_millis();   /* any accepted press resets the 30s idle clock */
    }

    handle_key(evt);
    service_error();
}

bool ascent_check_idle_sleep(void)
{
    if (g_state == ST_ERROR) {
        return false;   /* never interrupt the overflow/div-zero freeze */
    }
    return sys_elapsed(&g_last_key_ms, IDLE_SLEEP_MS);
}

void ascent_redraw(void)
{
    switch (g_state) {
    case ST_SPLASH:
        ui_show_splash();
        break;
    case ST_FRESH:
        ui_show_expr_and_operand(g_expr, g_expr_len, &k_fresh_operand_display);
        break;
    case ST_OPERAND:
        ui_show_expr_and_operand(g_expr, g_expr_len, &g_operand);
        break;
    case ST_OPERATOR:
        ui_show_expr_and_result(g_expr, g_expr_len, g_running_result, true);
        break;
    case ST_RESULT:
        ui_show_expr_and_result(g_expr, g_expr_len, g_running_result, false);
        break;
    case ST_ERROR:
        /* ascent_check_idle_sleep() never allows sleep during ST_ERROR,
         * so this redraw path is unreachable in practice; no message
         * text is cached to redraw, so intentionally left blank. */
        break;
    default:
        break;   /* ST_SLEEP_PEND: unreachable */
    }
}
