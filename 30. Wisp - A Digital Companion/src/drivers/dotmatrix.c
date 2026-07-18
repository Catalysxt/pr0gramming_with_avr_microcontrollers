#include "dotmatrix.h"

#define DOT_BLANK 0x00u

/* fb[row]; row 0 = top, bit 7 = leftmost column. */
static uint8_t s_fb[DOT_ROWS];

void dotmatrix_clear(void) {
    for (uint8_t r = 0; r < DOT_ROWS; r++) {
        s_fb[r] = DOT_BLANK;
    }
}

void dotmatrix_set_row(uint8_t row, uint8_t bits) {
    if (row >= DOT_ROWS) {
        return;
    }
    s_fb[row] = bits;
}

void dotmatrix_set_rows(const uint8_t rows[DOT_ROWS]) {
    for (uint8_t r = 0; r < DOT_ROWS; r++) {
        s_fb[r] = rows[r];
    }
}

const uint8_t *dotmatrix_rows(void) {
    return s_fb;
}

void dotmatrix_flush(max7219_chain_t chain) {
    /* Physical module is mounted rotated 90 deg CW relative to the logical
     * frame, so rotate the image 90 deg CCW here to bring the face upright.
     * Rotating an 8x8 bitmap CCW maps output pixel (r,c) <- input pixel
     * (c, 7-r); in bit terms, output row r takes bit i of every source row j
     * (source column j -> output bit 7-j). Callers keep the logical
     * "row 0 = top, bit 7 = left" convention regardless. */
    uint8_t out[DOT_ROWS];
    for (uint8_t r = 0; r < DOT_ROWS; r++) {
        uint8_t d = DOT_BLANK;
        for (uint8_t j = 0; j < DOT_ROWS; j++) {
            if ((s_fb[j] >> r) & 1u) {
                d |= (uint8_t)(1u << (7u - j));
            }
        }
        out[r] = d;
    }

    /* One MAX7221 digit register per matrix row. For a single-device chain the
     * data array is just the one row byte; chain_write pulses CS to latch it. */
    for (uint8_t r = 0; r < DOT_ROWS; r++) {
        uint8_t d = out[r];
        max7219_chain_write(chain, (uint8_t)(MAX7219_REG_DIGIT0 + r), &d);
    }
}
