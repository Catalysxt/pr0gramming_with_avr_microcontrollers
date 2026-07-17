/*
 * ui.h
 *
 * LCD draw helpers for every ASCENT screen. Wraps lcd_driver.h — ascent.c
 * never calls Lcd_* directly, it only decides *what* to show and calls
 * these. No SFX or state decisions belong here; ui.c is a dumb renderer
 * of whatever it's handed.
 */

#ifndef ASCENT_UI_H_
#define ASCENT_UI_H_

#include <stdint.h>
#include <stdbool.h>
#include "calc.h"

/* Boot screen: row0 "ASCENT v1", row1 "Calc Ready". */
void ui_show_splash(void);

/*
 * Live operand-entry screen (ST_FRESH shows a synthetic "0" operand;
 * ST_OPERAND shows the real one). expr/expr_len is the committed
 * expression so far (operand->len is always >= 1 in both call sites, so
 * this function never needs to handle an empty operand).
 * Row 0: rightmost 16 chars of expr, or '<' + rightmost 15 if expr_len > 16.
 * Row 1: operand right-aligned against column 14, with a blinking cursor
 * permanently parked at column 15 (the reserved cursor slot).
 */
void ui_show_expr_and_operand(const char *expr, uint8_t expr_len,
                               const operand_buf_t *operand);

/*
 * Result/running-total screen, shared by ST_OPERATOR (show_cursor=true —
 * solid cursor at column 15, "armed, ready for next operand") and
 * ST_RESULT (show_cursor=false — no cursor, final answer).
 * Row 0: same scrolling rule as ui_show_expr_and_operand.
 * Row 1: result right-aligned flush to column 15 via calc_format.
 */
void ui_show_expr_and_result(const char *expr, uint8_t expr_len,
                              int32_t result, bool show_cursor);

/* OVERFLOW / DIV BY ZERO screens. Cursor off. */
void ui_show_error(const char *line0, const char *line1);

/* Clears both rows and turns the display off, for sleep. The next
 * ui_show_* call re-enables the display automatically (no separate
 * "wake" call is needed). */
void ui_blank(void);

#endif /* ASCENT_UI_H_ */
