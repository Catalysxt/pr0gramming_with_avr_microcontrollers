#ifndef SLOT_MACHINE_SEG7_H_
#define SLOT_MACHINE_SEG7_H_

#include <stdint.h>

/* Score readout: the single MAX7221 (device 0 of the chain) drives two 4-digit
 * 7-segment modules = 8 digits. This module owns that chip's 8-byte segment
 * buffer and renders two unsigned numbers into it:
 *
 *   positions 0..3  = Display A  -> Credits / Balance
 *   positions 4..7  = Display B  -> Last Win
 *
 * Position 0 is the leftmost digit of Display A; within each 4-digit field the
 * number is right-aligned with leading zeros blanked (the ones digit always
 * shows). Values are clamped to 9999 (four digits). Segment bytes come from the
 * shared font7seg table. The refresh loop in main.c copies seg7_rows() into the
 * chain's device-0 slot. */

#define SEG7_DIGITS 8u

#define SEG7_DISPLAY_A 0u   /* Credits / Balance (left 4 digits)  */
#define SEG7_DISPLAY_B 1u   /* Last Win          (right 4 digits) */

void seg7_show_balance(uint16_t credits);
void seg7_show_win(uint16_t last_win);

/* Write up to 4 characters (via the shared 7-seg font) into one display,
 * left-aligned and blank-padded. Used for settings-menu labels ("PLAY", "CASH",
 * "EXIT"). `which` is SEG7_DISPLAY_A or SEG7_DISPLAY_B. */
void seg7_show_text(uint8_t which, const char *text);

/* Blank both displays. */
void seg7_clear(void);

/* The 8 segment bytes, index 0 = leftmost digit. */
const uint8_t *seg7_rows(void);

#endif /* SLOT_MACHINE_SEG7_H_ */
