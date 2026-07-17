/*
 * ui.h
 *
 * Top-level UI state machine, CGRAM glyph management, and LCD draw helpers.
 *
 * State transitions:
 *   SPLASH → MENU (on any button press or after 2 s timeout)
 *   MENU   → SLOTS / COINFLIP / HIGHERLOWER / DICE (BTN_A on highlighted item)
 *   Any game → MENU (BTN_B long-press)
 *   Any game → GAMEOVER (credits reach 0)
 *   GAMEOVER → MENU (BTN_A press)
 *
 * CGRAM slot assignments (loaded once at boot, slots 0–7):
 *   0 = cherry   1 = bell    2 = seven    3 = coin-heads
 *   4 = coin-tails  5 = up-arrow  6 = down-arrow  7 = dice-pip-frame
 */

#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "buttons.h"

/* Shared credit counter — defined in main.c, extern here for all modules. */
extern uint16_t g_credits;

/* Top-level state machine states. */
typedef enum
{
    STATE_SPLASH      = 0,
    STATE_MENU        = 1,
    STATE_SLOTS       = 2,
    STATE_COINFLIP    = 3,
    STATE_HIGHERLOWER = 4,
    STATE_DICE        = 5,
    STATE_GAMEOVER    = 6
} ui_state_t;

/* Current state, written by ui code and game modules. */
extern ui_state_t g_state;

/* CGRAM slot indices. */
#define GLYPH_CHERRY        0U
#define GLYPH_BELL          1U
#define GLYPH_SEVEN         2U
#define GLYPH_COIN_H        3U
#define GLYPH_COIN_T        4U
#define GLYPH_UP_ARROW      5U
#define GLYPH_DOWN_ARROW    6U
#define GLYPH_DICE_PIP      7U

/* Load all 8 CGRAM glyphs into the LCD controller.
 * Call once after Lcd_Init(), before sei(). */
void ui_load_cgram(void);

/* Draw the splash screen. */
void ui_draw_splash(void);

/* Draw the main menu with cursor at the current selection. */
void ui_draw_menu(void);

/* Handle button events while in MENU or SPLASH state. */
void ui_menu_update(btn_event_t e);

/* Write a null-terminated string centred on the given row (0 or 1).
 * Pads with spaces to fill the full 16-column line. */
void ui_draw_centered(uint8_t row, const char *s);

/* Write a 4-digit zero-padded credit count starting at the current cursor.
 * Example: g_credits = 42 → writes "0042". */
void ui_show_score(uint16_t score);

/* Draw the game-over screen. */
void ui_draw_gameover(void);

/* Handle button events on the game-over screen. */
void ui_gameover_update(btn_event_t e);

#endif /* UI_H */
