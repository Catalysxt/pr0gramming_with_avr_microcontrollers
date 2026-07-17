/*
 * ui.c
 *
 * Purpose : Render every ASCENT screen onto the 16x2 HD44780 LCD via the
 *           shared lcd_driver. No pin access here — lcd_driver.h owns
 *           the LCD bus entirely.
 * Hardware: HD44780-compatible 16x2 LCD, 4-bit mode (see lcd_driver.h).
 * Datasheet: HD44780/ST7066 — DDRAM holds visible characters. No CGRAM
 *            use here (the truncation glyph is the literal ASCII '<').
 *
 * Cursor design note: "right-aligned in columns 0-15" plus "cursor one
 * position past the last digit" is geometrically impossible for a flush
 * -right field (one-past column 15 is off-screen). Resolution: column 15
 * is permanently reserved as the cursor slot while an operand is being
 * typed. Digits right-align against a virtual edge at column 14
 * (pad = 16 - 1 - operand->len, always >= 5 since the digit cap is 10),
 * so the cursor's screen column stays visually stable as digits shift
 * left. This matches the trailing '_' in prompt.md's own Display Layout
 * mockup. ui_show_expr_and_result (read-only screens) does NOT reserve
 * that column — a finished result has no "next character" to make room
 * for, so it writes flush to column 15 and overlays the cursor on the
 * last digit when show_cursor is true (ST_OPERATOR only).
 *
 * Every ui_show_* function fully repaints its row(s) and re-issues
 * Lcd_DisplayControl(), so a call after ui_blank() (which turns the
 * display off) always re-enables it — no separate "wake" call needed.
 */

#include "ui.h"
#include "lcd_driver.h"
#include "calc.h"

#include <stdint.h>

/* Writes text starting at (col_start, row), then pads with spaces out to
 * LCD_COLS. Overwrites any leftover characters from a previous, longer
 * string without needing a full Lcd_Clear() (which costs a ~2ms
 * busy-flag-bounded delay per the driver's Clear Display timing). For
 * short, NUL-terminated literal strings (splash/error screens). */
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

/* Length-bounded variant for buffers that aren't NUL-terminated at their
 * logical end (g_expr / expr_len is tracked explicitly by ascent.c, the
 * same convention as operand_buf_t). Pads the remainder of the row with
 * spaces, same as ui_write_padded. */
static void ui_write_row_n(uint8_t col_start, uint8_t row, const char *text, uint8_t len)
{
    uint8_t col = col_start;

    (void)Lcd_SetCursor(col_start, row);
    for (uint8_t i = 0; (i < len) && (col < (uint8_t)LCD_COLS); i++, col++) {
        (void)Lcd_WriteChar((uint8_t)text[i]);
    }
    while (col < (uint8_t)LCD_COLS) {
        (void)Lcd_WriteChar(' ');
        col++;
    }
}

/* Row 0: rightmost 16 chars of expr, or a '<' truncation glyph at column 0
 * followed by the rightmost 15 chars, whenever expr_len exceeds the row
 * width — shared by every screen that shows the running expression. */
static void ui_draw_expr_row(const char *expr, uint8_t expr_len)
{
    if (expr_len <= (uint8_t)LCD_COLS) {
        ui_write_row_n(0, 0, expr, expr_len);
    } else {
        (void)Lcd_SetCursor(0, 0);
        (void)Lcd_WriteChar('<');                              /* truncation glyph */
        ui_write_row_n(1, 0, expr + (expr_len - 15u), 15u);     /* rightmost 15 chars */
    }
}

void ui_show_splash(void)
{
    ui_write_padded(0, 0, "ASCENT v1");
    ui_write_padded(0, 1, "Calc Ready");
    (void)Lcd_DisplayControl(1, 0, 0);  /* display on, no cursor/blink */
}

void ui_show_expr_and_operand(const char *expr, uint8_t expr_len,
                               const operand_buf_t *operand)
{
    ui_draw_expr_row(expr, expr_len);

    /* Right-align digits against column 14; column 15 is the reserved
     * cursor slot (see file banner comment). pad is always >= 5 since
     * operand->len is capped at 10. */
    uint8_t pad = (uint8_t)((uint8_t)LCD_COLS - 1u - operand->len);

    (void)Lcd_SetCursor(0, 1);
    for (uint8_t i = 0; i < pad; i++) {
        (void)Lcd_WriteChar(' ');
    }
    for (uint8_t i = 0; i < operand->len; i++) {
        (void)Lcd_WriteChar((uint8_t)operand->digits[i]);
    }
    (void)Lcd_WriteChar(' ');   /* clear column 15 before parking the cursor there */

    (void)Lcd_SetCursor((uint8_t)((uint8_t)LCD_COLS - 1u), 1u);
    (void)Lcd_DisplayControl(1, 1, 1);   /* display on, cursor+blink on */
}

void ui_show_expr_and_result(const char *expr, uint8_t expr_len,
                              int32_t result, bool show_cursor)
{
    ui_draw_expr_row(expr, expr_len);

    char    buf[12];
    uint8_t len = calc_format(result, buf);
    uint8_t pad = (uint8_t)((uint8_t)LCD_COLS - len);   /* flush-right, no reserved column */

    (void)Lcd_SetCursor(0, 1);
    for (uint8_t i = 0; i < pad; i++) {
        (void)Lcd_WriteChar(' ');
    }
    for (uint8_t i = 0; i < len; i++) {
        (void)Lcd_WriteChar((uint8_t)buf[i]);
    }

    if (show_cursor) {
        (void)Lcd_SetCursor((uint8_t)((uint8_t)LCD_COLS - 1u), 1u);
        (void)Lcd_DisplayControl(1, 1, 0);   /* solid, no blink — ST_OPERATOR */
    } else {
        (void)Lcd_DisplayControl(1, 0, 0);   /* no cursor — ST_RESULT */
    }
}

void ui_show_error(const char *line0, const char *line1)
{
    ui_write_padded(0, 0, line0);
    ui_write_padded(0, 1, line1);
    (void)Lcd_DisplayControl(1, 0, 0);   /* cursor off */
}

void ui_blank(void)
{
    (void)Lcd_Clear();
    (void)Lcd_DisplayControl(0, 0, 0);   /* display off */
}
