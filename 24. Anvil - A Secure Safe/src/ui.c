/*
 * ui.c
 *
 * Purpose : Render every ANVIL screen onto the 16x2 HD44780 LCD via the
 *           shared lcd_driver. No pin access here — lcd_driver.h owns
 *           the LCD bus entirely.
 * Hardware: HD44780-compatible 16x2 LCD, 4-bit mode (see lcd_driver.h).
 * Datasheet: HD44780/ST7066 — DDRAM holds visible characters, CGRAM
 *            holds up to 8 user-defined 5x8 glyphs (slot 0 used here for
 *            the lock icon). Vishay LCD-016N002B-CFH-ET datasheet §9.
 *
 * Every ui_show_* function fully repaints its row(s) and re-issues
 * Lcd_DisplayControl(), so a call after ui_screensaver() (which turns the
 * display off) always re-enables it — no separate "wake" call is needed.
 */

#include "ui.h"
#include "lcd_driver.h"

#include <stdint.h>

/* CGRAM slot 0: a 5x8 padlock — shackle (rows 0-2) over a body with a
 * keyhole slot (rows 3-6). Only bits 4:0 of each byte are used. */
static const uint8_t k_lock_glyph[LCD_CGRAM_BYTES_PER_CHAR] = {
    0x0Eu, /* 0b01110 */
    0x11u, /* 0b10001 */
    0x11u, /* 0b10001 */
    0x1Fu, /* 0b11111 */
    0x1Bu, /* 0b11011 — keyhole slot */
    0x11u, /* 0b10001 */
    0x1Fu, /* 0b11111 */
    0x00u, /* 0b00000 */
};

/* Writes text starting at (col_start, row), then pads with spaces out to
 * LCD_COLS. Overwrites any leftover characters from a previous, longer
 * string without needing a full Lcd_Clear() (which costs a ~2ms
 * busy-flag-bounded delay per the driver's Clear Display timing). */
static void ui_write_padded(uint8_t col_start, uint8_t row, const char *text)
{
    uint8_t col = col_start;

    (void)Lcd_SetCursor(col_start, row);
    while ((*text != '\0') && (col < (uint8_t)LCD_COLS)) {
        (void)Lcd_WriteChar((uint8_t)*text);
        text++;
        col++;
    }
    while (col < (uint8_t)LCD_COLS) {
        (void)Lcd_WriteChar(' ');
        col++;
    }
}

/* Writes a zero-padded 2-digit decimal (0-99) into dst[0..1]. */
static void write_2digit(char *dst, uint8_t val)
{
    dst[0] = (char)('0' + ((val / 10u) % 10u));
    dst[1] = (char)('0' + (val % 10u));
}

void ui_load_glyphs(void)
{
    (void)Lcd_WriteCustomChar(0u, k_lock_glyph);
}

void ui_show_splash(void)
{
    (void)Lcd_SetCursor(0, 0);
    (void)Lcd_WriteChar(0u);            /* CGRAM slot 0: lock glyph */
    ui_write_padded(1, 0, " ANVIL");
    ui_write_padded(0, 1, "Enter PIN:");
    (void)Lcd_DisplayControl(1, 0, 0);  /* display on, no cursor/blink yet */
}

void ui_show_entry(const char masked[4], uint8_t pos, const char *prompt)
{
    /* Fixed 16-char layout: "[****]  Pos: N/4" */
    char line[17];
    line[0]  = '[';
    line[1]  = masked[0];
    line[2]  = masked[1];
    line[3]  = masked[2];
    line[4]  = masked[3];
    line[5]  = ']';
    line[6]  = ' ';
    line[7]  = ' ';
    line[8]  = 'P';
    line[9]  = 'o';
    line[10] = 's';
    line[11] = ':';
    line[12] = ' ';
    line[13] = (char)('0' + pos);
    line[14] = '/';
    line[15] = '4';
    line[16] = '\0';

    ui_write_padded(0, 0, prompt);
    ui_write_padded(0, 1, line);

    /* Cursor sits one column right of '[' per digit typed so far. */
    (void)Lcd_SetCursor((uint8_t)(1u + pos), 1u);
    (void)Lcd_DisplayControl(1, 1, 1);  /* display on, cursor+blink on */
}

void ui_show_unlocked(uint8_t seconds_left)
{
    char line[17];
    static const char label[] = "Auto-lock: ";  /* 11 chars */
    uint8_t i;

    for (i = 0; label[i] != '\0'; i++) {
        line[i] = label[i];
    }
    write_2digit(&line[i], seconds_left);
    i = (uint8_t)(i + 2u);
    line[i] = 's';
    i++;
    line[i] = '\0';

    ui_write_padded(0, 0, "UNLOCKED");
    ui_write_padded(0, 1, line);
    (void)Lcd_DisplayControl(1, 0, 0);
}

void ui_show_wrong(uint8_t attempts_left)
{
    char line[17];
    static const char label[] = "Tries left: ";  /* 12 chars */
    uint8_t i;

    for (i = 0; label[i] != '\0'; i++) {
        line[i] = label[i];
    }
    write_2digit(&line[i], attempts_left);
    i = (uint8_t)(i + 2u);
    line[i] = '\0';

    ui_write_padded(0, 0, "WRONG PIN");
    ui_write_padded(0, 1, line);
    (void)Lcd_DisplayControl(1, 0, 0);
}

void ui_show_lockout(uint8_t seconds_left)
{
    char line[17];
    static const char label[] = "Wait: ";  /* 6 chars */
    uint8_t i;

    for (i = 0; label[i] != '\0'; i++) {
        line[i] = label[i];
    }
    write_2digit(&line[i], seconds_left);
    i = (uint8_t)(i + 2u);
    line[i] = 's';
    i++;
    line[i] = '\0';

    ui_write_padded(0, 0, "LOCKED");
    ui_write_padded(0, 1, line);
    (void)Lcd_DisplayControl(1, 0, 0);
}

void ui_show_change_ok(void)
{
    ui_write_padded(0, 0, "PIN CHANGED");
    ui_write_padded(0, 1, "");
    (void)Lcd_DisplayControl(1, 0, 0);
}

void ui_show_change_fail(void)
{
    ui_write_padded(0, 0, "MISMATCH");
    ui_write_padded(0, 1, "");
    (void)Lcd_DisplayControl(1, 0, 0);
}

void ui_screensaver(void)
{
    (void)Lcd_Clear();
    (void)Lcd_DisplayControl(0, 0, 0);  /* display off */
}
