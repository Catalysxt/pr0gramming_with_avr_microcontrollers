#ifndef DIGITAL_COMPANION_DOTMATRIX_H_
#define DIGITAL_COMPANION_DOTMATRIX_H_

#include <stdint.h>
#include "max7219.h"

/* Single 8x8 dot-matrix framebuffer + hardware flush.
 *
 * The face is one MAX7221 driving one 8x8 panel. This module owns an 8-byte
 * framebuffer -- fb[row], one byte per row, row 0 = top, bit 7 = leftmost
 * column -- and hides the row-order / orientation mapping to the physical
 * module. The animator composes a frame into this buffer; main.c streams it to
 * the chip at the refresh rate. No emotion logic lives here.
 *
 * Orientation note: DIGIT0..7 of the MAX7221 map to the panel's 8 rows and the
 * SEG lines to its 8 columns. If a given module is mounted rotated/mirrored,
 * this is the ONE place to flip row order or reverse the bit weight -- callers
 * keep the logical "row 0 = top, bit 7 = left" convention regardless. The
 * current hardware is mounted rotated 90 deg CW, so dotmatrix_flush() rotates
 * the frame 90 deg CCW to compensate (confirmed upright on hardware). */

#define DOT_ROWS 8u

/* Blank the framebuffer (all pixels off). */
void dotmatrix_clear(void);

/* Set one raw row of the framebuffer (row 0 = top, bit 7 = leftmost column). */
void dotmatrix_set_row(uint8_t row, uint8_t bits);

/* Blit all 8 rows at once from a RAM array (the animator's composed frame). */
void dotmatrix_set_rows(const uint8_t rows[DOT_ROWS]);

/* Pointer to the 8 framebuffer row bytes (row 0 = top). Read-only use. */
const uint8_t *dotmatrix_rows(void);

/* Stream the framebuffer to the MAX7221 over SPI: one DIGIT register per row,
 * each latched by a CS pulse. `chain` is the single-device (count = 1) chain. */
void dotmatrix_flush(max7219_chain_t chain);

#endif /* DIGITAL_COMPANION_DOTMATRIX_H_ */
