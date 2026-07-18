#include <avr/pgmspace.h>   /* symbol bitmaps live in flash */
#include "dotmatrix.h"

#define DOT_BLANK 0x00u

/* fb[panel][row]; row 0 = top, bit 7 = leftmost column. */
static uint8_t s_fb[DOT_PANELS][DOT_ROWS];

void dotmatrix_clear(void) {
    for (uint8_t p = 0; p < DOT_PANELS; p++) {
        for (uint8_t r = 0; r < DOT_ROWS; r++) {
            s_fb[p][r] = DOT_BLANK;
        }
    }
}

void dotmatrix_set_symbol(uint8_t panel, const uint8_t *symbol_pgm) {
    if (panel >= DOT_PANELS) {
        return;
    }
    for (uint8_t r = 0; r < DOT_ROWS; r++) {
        s_fb[panel][r] = pgm_read_byte(&symbol_pgm[r]);
    }
}

void dotmatrix_scroll_panel(uint8_t panel, const uint8_t *current_pgm,
                            const uint8_t *incoming_pgm, uint8_t offset) {
    if (panel >= DOT_PANELS) {
        return;
    }
    if (offset > DOT_ROWS) {
        offset = DOT_ROWS;
    }
    /* Visible row r shows the current symbol shifted up by `offset`; once that
     * runs past the top edge, the incoming symbol fills in from the bottom.
     *   r + offset <  8 -> current[r + offset]
     *   r + offset >= 8 -> incoming[r + offset - 8] */
    for (uint8_t r = 0; r < DOT_ROWS; r++) {
        uint8_t src = (uint8_t)(r + offset);
        if (src < DOT_ROWS) {
            s_fb[panel][r] = pgm_read_byte(&current_pgm[src]);
        } else {
            s_fb[panel][r] = pgm_read_byte(&incoming_pgm[src - DOT_ROWS]);
        }
    }
}

void dotmatrix_set_row(uint8_t panel, uint8_t row, uint8_t bits) {
    if (panel >= DOT_PANELS || row >= DOT_ROWS) {
        return;
    }
    s_fb[panel][row] = bits;
}

void dotmatrix_invert(void) {
    for (uint8_t p = 0; p < DOT_PANELS; p++) {
        for (uint8_t r = 0; r < DOT_ROWS; r++) {
            s_fb[p][r] = (uint8_t)~s_fb[p][r];
        }
    }
}

const uint8_t *dotmatrix_rows(uint8_t panel) {
    if (panel >= DOT_PANELS) {
        panel = 0;
    }
    return s_fb[panel];
}
