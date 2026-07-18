#include <stdbool.h>
#include "seg7.h"
#include "font7seg.h"

#define DIGITS_PER_DISPLAY 4u
#define DISPLAY_A_POS      0u    /* Credits / Balance -> leftmost 4 digits */
#define DISPLAY_B_POS      4u    /* Last Win          -> rightmost 4 digits */
#define SEG7_MAX_VALUE     9999u
#define SEG7_BLANK         0x00u

static uint8_t s_rows[SEG7_DIGITS];

/* Render a 0..9999 value into four consecutive digit slots, right-aligned, with
 * leading zeros blanked (but the ones digit always printed so 0 shows as "0"). */
static void render4(uint8_t *dst, uint16_t value) {
    if (value > SEG7_MAX_VALUE) {
        value = SEG7_MAX_VALUE;
    }
    uint16_t divisor  = 1000u;
    bool     leading  = true;
    for (uint8_t i = 0; i < DIGITS_PER_DISPLAY; i++) {
        uint8_t digit = (uint8_t)(value / divisor);
        value  %= divisor;
        divisor /= 10u;

        if (digit == 0 && leading && i < (DIGITS_PER_DISPLAY - 1u)) {
            dst[i] = SEG7_BLANK;                       /* suppress leading zero */
        } else {
            leading = false;
            dst[i]  = char_to_segments((char)('0' + digit));
        }
    }
}

void seg7_show_balance(uint16_t credits) {
    render4(&s_rows[DISPLAY_A_POS], credits);
}

void seg7_show_win(uint16_t last_win) {
    render4(&s_rows[DISPLAY_B_POS], last_win);
}

void seg7_show_text(uint8_t which, const char *text) {
    uint8_t base = (which == SEG7_DISPLAY_B) ? DISPLAY_B_POS : DISPLAY_A_POS;
    for (uint8_t i = 0; i < DIGITS_PER_DISPLAY; i++) {
        char c = text[i];
        if (c == '\0') {                 /* short string -> blank-pad the rest */
            for (; i < DIGITS_PER_DISPLAY; i++) {
                s_rows[base + i] = SEG7_BLANK;
            }
            break;
        }
        s_rows[base + i] = char_to_segments(c);
    }
}

void seg7_clear(void) {
    for (uint8_t i = 0; i < SEG7_DIGITS; i++) {
        s_rows[i] = SEG7_BLANK;
    }
}

const uint8_t *seg7_rows(void) {
    return s_rows;
}
