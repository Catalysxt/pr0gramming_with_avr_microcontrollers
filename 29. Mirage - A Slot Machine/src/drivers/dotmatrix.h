#ifndef SLOT_MACHINE_DOTMATRIX_H_
#define SLOT_MACHINE_DOTMATRIX_H_

#include <stdint.h>

/* 4-panel 8x8 dot-matrix framebuffer + reel rendering.
 *
 * The module is four cascaded MAX7219s (one per 8x8 panel), devices 1..4 of the
 * chain. This driver owns a 4x8 pixel buffer -- fb[panel][row], one byte per row,
 * bit 7 = leftmost column, row 0 = top -- and the refresh loop in main.c streams
 * it out. Symbol bitmaps (assets/symbols.h) live in PROGMEM and follow the same
 * bit convention, so a blit is a straight PROGMEM->buffer copy.
 *
 * Panel usage (see the justification comment in game.c):
 *   panel 0 = left reel   panel 1 = center reel   panel 2 = right reel
 *   panel 3 = payline / activity accent column
 *
 * Reel spin is a *vertical* scroll: dotmatrix_scroll_panel() slides an incoming
 * symbol up from the bottom as the current one exits the top. The caller drives
 * the scroll offset over time with easing (fast -> slow -> snap); this module
 * just renders one frozen frame per call. */

#define DOT_PANELS 4u
#define DOT_ROWS   8u

/* Blank every panel. */
void dotmatrix_clear(void);

/* Copy an 8-byte symbol (PROGMEM) straight into one panel. */
void dotmatrix_set_symbol(uint8_t panel, const uint8_t *symbol_pgm);

/* Render a vertical scroll frame into `panel`: `current` exits upward while
 * `incoming` enters from the bottom. offset 0 = current fully shown, offset 8 =
 * incoming fully shown; both symbols are PROGMEM 8-byte bitmaps. Used to animate
 * a spinning reel (cycle offset 0..8, then advance to the next symbol pair). */
void dotmatrix_scroll_panel(uint8_t panel, const uint8_t *current_pgm,
                            const uint8_t *incoming_pgm, uint8_t offset);

/* Set one raw row of a panel (RAM value, not PROGMEM) -- used for the panel-3
 * accent column and win-flash effects. */
void dotmatrix_set_row(uint8_t panel, uint8_t row, uint8_t bits);

/* Invert every pixel on every panel -- one frame of a jackpot flash. */
void dotmatrix_invert(void);

/* The 8 row bytes of one panel (row 0 = top). */
const uint8_t *dotmatrix_rows(uint8_t panel);

#endif /* SLOT_MACHINE_DOTMATRIX_H_ */
