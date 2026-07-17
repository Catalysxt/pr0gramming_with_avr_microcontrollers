/*
 * keypad.c
 *
 * Purpose : 4x4 matrix keypad scan, debounce, and long-press detection.
 * Hardware: columns PC0-PC3 (pulled-up inputs), rows split across two
 *           ports — ROW0/ROW1 on PORTC (PC4,PC5), ROW2/ROW3 on PORTD
 *           (PD2,PD3). See hardware.h for the exact pin macros.
 * Datasheet: ATmega328P §14.2.1 (DDRx/PORTx as input/pull-up config),
 *            §14.2.4 (reading PINx)
 *
 * Scan algorithm: drive one row low at a time (all others stay high),
 * settle briefly, then read the column bus. A column bit reading 0 means
 * the key at (row, col) is pressed (active-low: pull-up beaten by the
 * low-driven row through the closed contact).
 *
 * Debounce: a raw scan result must repeat for KEY_DEBOUNCE_SCANS
 * consecutive 5 ms scans before it is accepted as the new stable state.
 *
 * Press/long-press model: KEY_EVT_PRESS fires when a debounced press is
 * followed by a debounced release (short tap). If the key is still held
 * KEY_LONGPRESS_MS after the debounced press, KEY_EVT_LONG fires once
 * immediately and the eventual release is suppressed — no KEY_EVT_PRESS
 * follows a KEY_EVT_LONG for the same hold. Ascent v1 never consumes
 * KEY_EVT_LONG (reserved for a stretch feature); the API stays symmetric
 * with Anvil's so a future long-press feature costs nothing here.
 *
 * No SFX here — see keypad.h banner comment. ascent.c owns all audio.
 */

#include "keypad.h"
#include "hardware.h"
#include "sys_tick.h"

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

#define KEY_SCAN_PERIOD_MS   5u
#define KEY_DEBOUNCE_SCANS   3u
#define KEY_LONGPRESS_MS     1000u
#define ROW_SETTLE_US        10u    /* pull-up/stray-capacitance settle */
#define KEY_NONE             0xFFu  /* sentinel: no key pressed          */

/* Row output pins, indexed 0..3. Rows are split across PORTC and PORTD,
 * so each entry names its own DDR/PORT register — never assume one
 * contiguous nibble (unlike the single-port hello_world_keypad reference). */
typedef struct {
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    uint8_t            bit;
} row_pin_t;

static const row_pin_t k_row[KEY_ROW_COUNT] = {
    { &KEY_ROW0_DDR, &KEY_ROW0_PORT, KEY_ROW0_BIT },
    { &KEY_ROW1_DDR, &KEY_ROW1_PORT, KEY_ROW1_BIT },
    { &KEY_ROW2_DDR, &KEY_ROW2_PORT, KEY_ROW2_BIT },
    { &KEY_ROW3_DDR, &KEY_ROW3_PORT, KEY_ROW3_BIT },
};

static const char k_keymap[KEY_ROW_COUNT * KEY_COL_COUNT] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D',
};

static uint32_t s_scan_anchor    = 0;

/* Debounce candidate tracking. */
static uint8_t  s_candidate       = KEY_NONE;
static uint8_t  s_candidate_count = 0;

/* Debounced steady state. */
static uint8_t  s_confirmed      = KEY_NONE;
static uint32_t s_press_start_ms = 0;
static bool     s_long_fired     = false;

/* Single-slot pending event queue (see keypad.h). */
static key_event_t s_pending = { KEY_EVT_NONE, 0 };

void keypad_rows_sleep(void)
{
    for (uint8_t r = 0; r < KEY_ROW_COUNT; r++) {
        *k_row[r].port &= (uint8_t)~(1u << k_row[r].bit);  /* row low */
    }
}

void keypad_rows_wake(void)
{
    for (uint8_t r = 0; r < KEY_ROW_COUNT; r++) {
        *k_row[r].port |= (uint8_t)(1u << k_row[r].bit);   /* row idle-high */
    }
}

void keypad_init(void)
{
    KEY_COL_DDR  &= (uint8_t)~KEY_COL_MASK;  /* columns: inputs           */
    KEY_COL_PORT |= KEY_COL_MASK;             /* columns: enable pull-ups  */

    for (uint8_t r = 0; r < KEY_ROW_COUNT; r++) {
        *k_row[r].ddr |= (uint8_t)(1u << k_row[r].bit);  /* row: output  */
    }
    keypad_rows_wake();                                   /* row: idle hi */
}

/* One full row-by-row scan. Returns a flattened 0..15 index into
 * k_keymap, or KEY_NONE if no key (or more than one key) is pressed. */
static uint8_t scan_raw(void)
{
    for (uint8_t r = 0; r < KEY_ROW_COUNT; r++) {
        *k_row[r].port &= (uint8_t)~(1u << k_row[r].bit); /* row low */
        _delay_us(ROW_SETTLE_US);

        uint8_t cols = (uint8_t)(KEY_COL_PIN & KEY_COL_MASK);

        *k_row[r].port |= (uint8_t)(1u << k_row[r].bit);  /* row back high */

        if (cols != KEY_COL_MASK) {
            uint8_t pressed = (uint8_t)(~cols & KEY_COL_MASK);

            /* More than one column bit low -> ghosting/multi-press.
             * Spec: "at most one key is acted on" -> ignore this scan. */
            if ((pressed & (uint8_t)(pressed - 1u)) != 0u) {
                return KEY_NONE;
            }

            for (uint8_t c = 0; c < KEY_COL_COUNT; c++) {
                if (pressed & (uint8_t)(1u << c)) {
                    return (uint8_t)((r * KEY_COL_COUNT) + c);
                }
            }
        }
    }
    return KEY_NONE;
}

static void queue_press(uint8_t idx)
{
    s_pending.kind = KEY_EVT_PRESS;
    s_pending.ch   = k_keymap[idx];
    /* No buzzer_play() here — Ascent's top-level FSM (ascent.c) decides
     * SFX per key CLASS (digit/operator/equals/clear/error), which
     * keypad.c cannot know. See keypad.h banner comment. */
}

static void queue_long(uint8_t idx)
{
    s_pending.kind = KEY_EVT_LONG;
    s_pending.ch   = k_keymap[idx];
}

void keypad_service(void)
{
    if (!sys_elapsed(&s_scan_anchor, KEY_SCAN_PERIOD_MS)) {
        return;
    }

    uint8_t raw = scan_raw();

    if (raw == s_candidate) {
        if (s_candidate_count < KEY_DEBOUNCE_SCANS) {
            s_candidate_count++;
        }
    } else {
        s_candidate       = raw;
        s_candidate_count = 1;
    }

    if ((s_candidate_count >= KEY_DEBOUNCE_SCANS) && (s_confirmed != s_candidate)) {
        uint8_t previous = s_confirmed;
        s_confirmed = s_candidate;

        if (s_confirmed != KEY_NONE) {
            /* Debounced press-down edge: start the long-press timer. */
            s_press_start_ms = sys_millis();
            s_long_fired     = false;
        } else if (!s_long_fired) {
            /* Debounced release of `previous`, and it never went long:
             * this is the short-tap KEY_EVT_PRESS. */
            queue_press(previous);
        }
    }

    if ((s_confirmed != KEY_NONE) && !s_long_fired) {
        if ((sys_millis() - s_press_start_ms) >= KEY_LONGPRESS_MS) {
            s_long_fired = true;
            queue_long(s_confirmed);
        }
    }
}

key_event_t keypad_consume_event(void)
{
    key_event_t evt = s_pending;
    s_pending.kind = KEY_EVT_NONE;
    s_pending.ch   = 0;
    return evt;
}
