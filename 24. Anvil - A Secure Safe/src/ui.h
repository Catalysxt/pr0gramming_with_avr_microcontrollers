/*
 * ui.h
 *
 * LCD draw helpers for every ANVIL screen. Wraps lcd_driver.h — anvil.c
 * never calls Lcd_* directly, it only decides *what* to show and calls
 * these. Masking/reveal-timing decisions belong to anvil.c; ui.c is a
 * dumb renderer of whatever it's handed.
 */

#ifndef ANVIL_UI_H_
#define ANVIL_UI_H_

#include <stdint.h>

/* Loads the CGRAM lock glyph into slot 0. Call once during init, after
 * Lcd_Init() and before any ui_show_* call. */
void ui_load_glyphs(void);

/* Boot screen: row0 "<lock> ANVIL", row1 "Enter PIN:". */
void ui_show_splash(void);

/*
 * Live PIN-entry screen, shared by ST_ENTRY and the three change-PIN
 * sub-states (only the prompt string differs between them).
 * masked[4]: what to show at each of the 4 slots — '*' , a literal
 *   revealed digit, or ' ' for a not-yet-typed slot.
 * pos: current entry position (0-4); also where the blinking cursor sits.
 * prompt: row 0 text, e.g. "Enter PIN:", "Current PIN:", "New PIN:",
 *   "Confirm PIN:".
 */
void ui_show_entry(const char masked[4], uint8_t pos, const char *prompt);

/* row0 "UNLOCKED", row1 "Auto-lock: NNs". */
void ui_show_unlocked(uint8_t seconds_left);

/* row0 "WRONG PIN", row1 "Tries left: NN". */
void ui_show_wrong(uint8_t attempts_left);

/* row0 "LOCKED", row1 "Wait: NNs". */
void ui_show_lockout(uint8_t seconds_left);

/* row0 "PIN CHANGED" — change-PIN happy path, brief display. */
void ui_show_change_ok(void);

/* row0 "MISMATCH" — change-PIN abort path, brief display. */
void ui_show_change_fail(void);

/* Blanks the display (Lcd_Clear + display off) for the idle screensaver.
 * The next ui_show_* call re-enables the display automatically. */
void ui_screensaver(void);

#endif /* ANVIL_UI_H_ */
