/*
 * ui.c
 *
 * Top-level UI: splash screen, main menu FSM, CGRAM glyph table, draw helpers.
 *
 * Menu navigation:
 *   BTN_B short press → move cursor to next item (wraps)
 *   BTN_A short press → enter highlighted game
 *   BTN_B long  press → return to menu from any game (handled in main.c)
 *
 * CGRAM glyph patterns are 5×8 bitmaps (5 used bits per row).
 * Datasheet: HD44780/ST7066, Section 9 (CGRAM), Section 11 (Set CGRAM Address).
 */

#include "ui.h"
#include "buzzer.h"
#include "games/slots.h"
#include "games/coinflip.h"
#include "games/higherlower.h"
#include "games/dice.h"
#include "lcd_driver.h"
#include <avr/pgmspace.h>
#include <string.h>

ui_state_t g_state   = STATE_SPLASH;
uint16_t   g_credits = 100U;

/* ==========================================================================
 * CGRAM glyph pixel patterns (5×8, stored in PROGMEM to save SRAM)
 * Each byte: bits 4..0 = columns left-to-right; bit 4 = leftmost pixel.
 * HD44780 datasheet Figure 14 (character generator RAM).
 * ========================================================================== */

static const uint8_t GLYPH_CHERRY_DATA[8] PROGMEM =
{
    0b00110,   /* ..##. */
    0b01100,   /* .##.. */
    0b00010,   /* ...#. */
    0b01110,   /* .###. */
    0b11111,   /* ##### */
    0b11111,   /* ##### */
    0b01110,   /* .###. */
    0b00000    /* ..... */
};

static const uint8_t GLYPH_BELL_DATA[8] PROGMEM =
{
    0b00100,   /* ..#.. */
    0b01110,   /* .###. */
    0b01110,   /* .###. */
    0b11111,   /* ##### */
    0b11111,   /* ##### */
    0b00000,   /* ..... */
    0b00100,   /* ..#.. */
    0b00000    /* ..... */
};

static const uint8_t GLYPH_SEVEN_DATA[8] PROGMEM =
{
    0b11111,   /* ##### */
    0b00001,   /* ....# */
    0b00011,   /* ...## */
    0b00110,   /* ..##. */
    0b01100,   /* .##.. */
    0b01100,   /* .##.. */
    0b01100,   /* .##.. */
    0b00000    /* ..... */
};

static const uint8_t GLYPH_COIN_H_DATA[8] PROGMEM =
{
    0b01110,   /* .###. */
    0b11111,   /* ##### */
    0b11011,   /* ##.## */
    0b11111,   /* ##### */
    0b11011,   /* ##.## */
    0b11111,   /* ##### */
    0b01110,   /* .###. */
    0b00000    /* ..... */
};

static const uint8_t GLYPH_COIN_T_DATA[8] PROGMEM =
{
    0b01110,   /* .###. */
    0b11111,   /* ##### */
    0b11111,   /* ##### */
    0b01110,   /* .###. */
    0b11111,   /* ##### */
    0b11111,   /* ##### */
    0b01110,   /* .###. */
    0b00000    /* ..... */
};

static const uint8_t GLYPH_UP_ARROW_DATA[8] PROGMEM =
{
    0b00100,   /* ..#.. */
    0b01110,   /* .###. */
    0b11111,   /* ##### */
    0b00100,   /* ..#.. */
    0b00100,   /* ..#.. */
    0b00100,   /* ..#.. */
    0b00100,   /* ..#.. */
    0b00000    /* ..... */
};

static const uint8_t GLYPH_DOWN_ARROW_DATA[8] PROGMEM =
{
    0b00100,   /* ..#.. */
    0b00100,   /* ..#.. */
    0b00100,   /* ..#.. */
    0b00100,   /* ..#.. */
    0b11111,   /* ##### */
    0b01110,   /* .###. */
    0b00100,   /* ..#.. */
    0b00000    /* ..... */
};

static const uint8_t GLYPH_DICE_PIP_DATA[8] PROGMEM =
{
    0b11111,   /* ##### */
    0b10001,   /* #...# */
    0b10001,   /* #...# */
    0b10101,   /* #.#.# */
    0b10001,   /* #...# */
    0b10001,   /* #...# */
    0b11111,   /* ##### */
    0b00000    /* ..... */
};

/* Pointer table — same order as GLYPH_* constants 0..7 */
typedef const uint8_t * const pgm_ptr_t;
static const pgm_ptr_t GLYPH_TABLE[8] =
{
    GLYPH_CHERRY_DATA,
    GLYPH_BELL_DATA,
    GLYPH_SEVEN_DATA,
    GLYPH_COIN_H_DATA,
    GLYPH_COIN_T_DATA,
    GLYPH_UP_ARROW_DATA,
    GLYPH_DOWN_ARROW_DATA,
    GLYPH_DICE_PIP_DATA
};

void ui_load_cgram(void)
{
    uint8_t slot;
    uint8_t row_buf[8];
    uint8_t r;

    for (slot = 0U; slot < 8U; slot++)
    {
        /* Copy PROGMEM pattern to stack buffer before passing to driver
         * (Lcd_WriteCustomChar takes a RAM pointer). */
        for (r = 0U; r < 8U; r++)
        {
            row_buf[r] = pgm_read_byte(&GLYPH_TABLE[slot][r]);
        }
        Lcd_WriteCustomChar(slot, row_buf);
    }
}

/* ==========================================================================
 * Draw helpers
 * ========================================================================== */

void ui_draw_centered(uint8_t row, const char *s)
{
    char buf[17];
    uint8_t len, pad;

    len = (uint8_t)strlen(s);
    if (len > 16U) { len = 16U; }
    pad = (uint8_t)((16U - len) / 2U);

    memset(buf, ' ', 16U);
    memcpy(&buf[pad], s, len);
    buf[16] = '\0';

    Lcd_SetCursor(0U, row);
    Lcd_WriteString(buf);
}

void ui_show_score(uint16_t score)
{
    char buf[5];
    buf[0] = (char)('0' + (score / 1000U) % 10U);
    buf[1] = (char)('0' + (score / 100U)  % 10U);
    buf[2] = (char)('0' + (score / 10U)   % 10U);
    buf[3] = (char)('0' + score            % 10U);
    buf[4] = '\0';
    Lcd_WriteString(buf);
}

/* ==========================================================================
 * Splash screen
 * ========================================================================== */

void ui_draw_splash(void)
{
    Lcd_Clear();
    ui_draw_centered(0U, "Pocket Casino");
    ui_draw_centered(1U, "Press any key");
}

/* ==========================================================================
 * Main menu
 * ========================================================================== */

static const char * const MENU_ITEMS[] =
{
    "Slots",
    "Coin Flip",
    "Hi or Lo",
    "Dice"
};
#define MENU_COUNT 4U

static uint8_t s_menu_sel = 0U;

void ui_draw_menu(void)
{
    const char *top;
    const char *bot;

    /* Show the selected item on row 0 with '>' cursor and the next on row 1. */
    top = MENU_ITEMS[s_menu_sel];
    bot = MENU_ITEMS[(s_menu_sel + 1U) % MENU_COUNT];

    Lcd_SetCursor(0U, 0U);
    Lcd_WriteString(">");
    Lcd_WriteChar(' ');
    Lcd_WriteString(top);
    /* pad to column 9 */
    {
        uint8_t col = (uint8_t)(2U + strlen(top));
        while (col < 9U) { Lcd_WriteChar(' '); col++; }
    }
    Lcd_WriteString("Cr:");
    ui_show_score(g_credits);

    /* Row 1: "  <next item>" */
    Lcd_SetCursor(0U, 1U);
    Lcd_WriteChar(' ');
    Lcd_WriteChar(' ');
    Lcd_WriteString(bot);
    /* Clear remainder of row */
    {
        uint8_t col = (uint8_t)(2U + strlen(bot));
        while (col < 16U) { Lcd_WriteChar(' '); col++; }
    }

}

void ui_menu_update(btn_event_t e)
{
    ui_state_t next_game;

    if (g_state == STATE_SPLASH)
    {
        if (e != BTN_NONE)
        {
            g_state = STATE_MENU;
            ui_draw_menu();
        }
        return;
    }

    /* STATE_MENU */
    switch (e)
    {
        case BTN_B_PRESS:
            s_menu_sel = (uint8_t)((s_menu_sel + 1U) % MENU_COUNT);
            buzzer_play_melody(SFX_NAV_TICK, SFX_NAV_TICK_LEN);
            ui_draw_menu();
            break;

        case BTN_A_PRESS:
            switch (s_menu_sel)
            {
                case 0U: next_game = STATE_SLOTS;       break;
                case 1U: next_game = STATE_COINFLIP;    break;
                case 2U: next_game = STATE_HIGHERLOWER; break;
                case 3U: next_game = STATE_DICE;        break;
                default: next_game = STATE_SLOTS;       break;
            }
            g_state = next_game;
            buzzer_play_melody(SFX_CONFIRM, SFX_CONFIRM_LEN);
            switch (next_game)
            {
                case STATE_SLOTS:       slots_enter();       break;
                case STATE_COINFLIP:    coinflip_enter();    break;
                case STATE_HIGHERLOWER: higherlower_enter(); break;
                case STATE_DICE:        dice_enter();        break;
                default: break;
            }
            break;

        default:
            break;
    }
}

/* ==========================================================================
 * Game-over screen
 * ========================================================================== */

void ui_draw_gameover(void)
{
    Lcd_Clear();
    ui_draw_centered(0U, "GAME OVER");
    ui_draw_centered(1U, "A=Restart");
}

void ui_gameover_update(btn_event_t e)
{
    if (e == BTN_A_PRESS)
    {
        g_credits = 100U;
        g_state   = STATE_MENU;
        ui_draw_menu();
    }
}
