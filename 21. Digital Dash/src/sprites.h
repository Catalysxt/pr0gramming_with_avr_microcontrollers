/*
 * sprites.h
 *
 * CGRAM custom-character slot assignments and loader function.
 * Byte patterns are stored in program flash (PROGMEM) to save SRAM.
 *
 * HD44780 datasheet §8 — CGRAM, 5×8 font, 8 custom character slots.
 */

#ifndef DIGITAL_DASH_SPRITES_H_
#define DIGITAL_DASH_SPRITES_H_

/* CGRAM slot numbers — pass these to Lcd_WriteChar() to render a glyph. */
#define SPRITE_HILL        0u
#define SPRITE_STEP1       1u
#define SPRITE_STEP2       2u
#define SPRITE_JUMP        3u
#define SPRITE_DUCK        4u
#define SPRITE_CROW1       5u
#define SPRITE_CROW2       6u

/*
 * Load all 7 custom glyphs from PROGMEM into LCD CGRAM slots 0-6.
 * Restores the address counter to DDRAM row 0, col 0 on exit.
 * Call once after Lcd_Init(), before any display writes.
 */
void sprites_load_all(void);

#endif /* DIGITAL_DASH_SPRITES_H_ */
