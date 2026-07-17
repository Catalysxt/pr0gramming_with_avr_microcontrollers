/*
 * sprites.c
 *
 * Purpose : Store 7 custom LCD glyphs in program flash and load them into
 *           CGRAM at startup.
 * Hardware : LCD CGRAM via lcd_driver (no direct GPIO).
 * Datasheet: HD44780 §8 (CGRAM), ATmega328P §10 (PROGMEM / LPM instruction)
 *
 * Byte patterns are copied verbatim from the original Arduino sketch.
 * 0b literals are replaced with hex for -Wpedantic / C99 compliance;
 * the original binary pattern is kept as a comment on each row.
 * Each pattern is 8 bytes; only bits [4:0] of each byte are displayed.
 */

#include "sprites.h"
#include "lcd_driver.h"

#include <avr/pgmspace.h>   /* PROGMEM, pgm_read_byte — keep glyph data in flash */
#include <stdint.h>

/* ---- Glyph pixel patterns (5 columns × 8 rows each) -------------------- */

static const uint8_t PROGMEM k_hill[8] = {
    0x00u, /* 00000 */
    0x04u, /* 00100 */
    0x0Au, /* 01010 */
    0x0Eu, /* 01110 */
    0x1Du, /* 11101 */
    0x15u, /* 10101 */
    0x19u, /* 11001 */
    0x1Fu, /* 11111 */
};

static const uint8_t PROGMEM k_step1[8] = {
    0x0Eu, /* 01110 */
    0x0Eu, /* 01110 */
    0x05u, /* 00101 */
    0x1Fu, /* 11111 */
    0x14u, /* 10100 */
    0x06u, /* 00110 */
    0x19u, /* 11001 */
    0x01u, /* 00001 */
};

static const uint8_t PROGMEM k_step2[8] = {
    0x0Eu, /* 01110 */
    0x0Eu, /* 01110 */
    0x05u, /* 00101 */
    0x1Fu, /* 11111 */
    0x14u, /* 10100 */
    0x06u, /* 00110 */
    0x0Bu, /* 01011 */
    0x08u, /* 01000 */
};

static const uint8_t PROGMEM k_jump[8] = {
    0x0Eu, /* 01110 */
    0x0Eu, /* 01110 */
    0x04u, /* 00100 */
    0x1Fu, /* 11111 */
    0x04u, /* 00100 */
    0x1Fu, /* 11111 */
    0x11u, /* 10001 */
    0x00u, /* 00000 */
};

static const uint8_t PROGMEM k_duck[8] = {
    0x00u, /* 00000 */
    0x00u, /* 00000 */
    0x00u, /* 00000 */
    0x0Eu, /* 01110 */
    0x0Eu, /* 01110 */
    0x1Fu, /* 11111 */
    0x04u, /* 00100 */
    0x1Fu, /* 11111 */
};

static const uint8_t PROGMEM k_crow1[8] = {
    0x07u, /* 00111 */
    0x04u, /* 00100 */
    0x06u, /* 00110 */
    0x0Fu, /* 01111 */
    0x1Fu, /* 11111 */
    0x0Fu, /* 01111 */
    0x06u, /* 00110 */
    0x07u, /* 00111 */
};

static const uint8_t PROGMEM k_crow2[8] = {
    0x07u, /* 00111 */
    0x06u, /* 00110 */
    0x0Fu, /* 01111 */
    0x1Fu, /* 11111 */
    0x0Fu, /* 01111 */
    0x06u, /* 00110 */
    0x06u, /* 00110 */
    0x07u, /* 00111 */
};

/* Map slot numbers to their PROGMEM arrays. */
static const uint8_t * const k_glyph_table[7] = {
    k_hill,
    k_step1,
    k_step2,
    k_jump,
    k_duck,
    k_crow1,
    k_crow2,
};

void sprites_load_all(void)
{
    uint8_t buf[8];

    for (uint8_t slot = 0; slot < 7u; slot++) {
        /* Copy 8 bytes from flash into a stack buffer, then hand to driver.
         * pgm_read_byte dereferences a flash pointer (LPM instruction).
         * HD44780 datasheet §8 — CGRAM write sequence. */
        for (uint8_t row = 0; row < 8u; row++) {
            buf[row] = pgm_read_byte(&k_glyph_table[slot][row]);
        }
        Lcd_WriteCustomChar(slot, buf);
    }

    /* After CGRAM writes the address counter points into CGRAM space.
     * Return it to DDRAM (0,0) so subsequent WriteChar/WriteString calls
     * address display memory, not the character generator.
     * HD44780 datasheet §8 — "Set DDRAM Address" command resets AC to DDRAM. */
    Lcd_SetCursor(0u, 0u);
}
