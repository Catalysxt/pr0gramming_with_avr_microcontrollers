/*
 * slots.c
 *
 * Slot machine — 3 independently animated reels, each cycling through 5
 * CGRAM symbol indices: cherry(0), bell(1), seven(2), coin(3), dice-pip(7).
 *
 * Reel state machine:
 *   SLOTS_IDLE      — show current reel symbols + bet; wait for input
 *   SLOTS_SPINNING  — all 3 reels spin; stop individually at 800/1100/1400 ms
 *   SLOTS_RESULT    — show payout; wait for button to continue
 *
 * Screen layout:
 *   Row 0: "[R1] [R2] [R3]  "   (3 CGRAM glyphs separated by spaces)
 *   Row 1: "Bet:5  Cr:0095  "
 *
 * Payouts (applied to bet amount):
 *   3x sevens (GLYPH_SEVEN) → ×50  (JACKPOT)
 *   3x any match            → ×10  (WIN)
 *   2x any match            → ×2   (WIN)
 *   else                    → 0    (LOSE)
 */

#include "slots.h"
#include "ui.h"
#include "buzzer.h"
#include "rng.h"
#include "lcd_driver.h"
#include <stdint.h>
#include <stdio.h>

/* The 5 CGRAM symbol indices used by the reels */
#define REEL_SYMBOLS    5U
static const uint8_t SYMBOLS[REEL_SYMBOLS] =
{
    GLYPH_CHERRY,   /* 0 */
    GLYPH_BELL,     /* 1 */
    GLYPH_SEVEN,    /* 2 */
    GLYPH_COIN_H,   /* 3 */
    GLYPH_DICE_PIP  /* 4 → maps to CGRAM slot 7 */
};

#define BET_COUNT   4U
static const uint16_t BET_OPTIONS[BET_COUNT] = { 1U, 5U, 10U, 25U };

/* Reel stop times (ms after spin start) */
#define STOP_R1_MS  800U
#define STOP_R2_MS  1100U
#define STOP_R3_MS  1400U
#define FRAME_MS    80U     /* animation frame rate */

extern volatile uint32_t g_ms_tick;

typedef enum
{
    SLOTS_IDLE     = 0,
    SLOTS_SPINNING = 1,
    SLOTS_RESULT   = 2
} slots_state_t;

static slots_state_t s_state;
static uint8_t       s_bet_idx;
static uint8_t       s_reel[3];         /* current symbol index (0–4) for each reel */
static uint8_t       s_final[3];        /* locked final symbol for each reel         */
static uint8_t       s_stopped[3];      /* 1 once a reel has stopped                 */
static uint32_t      s_spin_start;
static uint32_t      s_last_frame;
static uint8_t       s_spin_pos[3];     /* per-reel animation position               */

static void draw_reels(void)
{
    /* Row 0: up to 3 glyphs with spaces */
    Lcd_SetCursor(0U, 0U);
    Lcd_WriteChar(SYMBOLS[s_reel[0]]); Lcd_WriteChar(' ');
    Lcd_WriteChar(SYMBOLS[s_reel[1]]); Lcd_WriteChar(' ');
    Lcd_WriteChar(SYMBOLS[s_reel[2]]);
    Lcd_WriteString("           ");    /* clear rest of row */
}

static void draw_status(void)
{
    char buf[17];
    snprintf(buf, sizeof(buf), "Bet:%-2u Cr:", BET_OPTIONS[s_bet_idx]);
    Lcd_SetCursor(0U, 1U);
    Lcd_WriteString(buf);
    ui_show_score(g_credits);
}

static void draw_result_row(uint8_t won, uint16_t payout, uint8_t is_jackpot)
{
    Lcd_SetCursor(0U, 1U);
    if (is_jackpot)
    {
        Lcd_WriteString("JACKPOT!   Cr:");
    }
    else if (won)
    {
        char buf[17];
        snprintf(buf, sizeof(buf), "WIN +%-4u  Cr:", payout);
        Lcd_WriteString(buf);
    }
    else
    {
        Lcd_WriteString("No luck    Cr:");
    }
    ui_show_score(g_credits);
}

void slots_enter(void)
{
    s_state   = SLOTS_IDLE;
    s_bet_idx = 1U;    /* default 5 credits */
    s_reel[0] = 0U;
    s_reel[1] = 0U;
    s_reel[2] = 0U;
    Lcd_Clear();
    draw_reels();
    draw_status();
}

void slots_update(btn_event_t e)
{
    uint32_t now = g_ms_tick;
    uint16_t bet;
    uint8_t  i;

    switch (s_state)
    {
        case SLOTS_IDLE:
            if (e == BTN_B_PRESS)
            {
                s_bet_idx = (uint8_t)((s_bet_idx + 1U) % BET_COUNT);
                buzzer_play_melody(SFX_NAV_TICK, SFX_NAV_TICK_LEN);
                draw_status();
            }
            else if (e == BTN_A_PRESS)
            {
                bet = BET_OPTIONS[s_bet_idx];
                if (g_credits < bet)
                {
                    Lcd_SetCursor(0U, 1U);
                    Lcd_WriteString("Not enough Cr!  ");
                    return;
                }
                g_credits   -= bet;
                s_spin_start = now;
                s_last_frame = now;
                s_stopped[0] = 0U;
                s_stopped[1] = 0U;
                s_stopped[2] = 0U;
                /* Randomise final landing positions */
                for (i = 0U; i < 3U; i++)
                {
                    s_final[i] = rng_u8(REEL_SYMBOLS);
                    s_spin_pos[i] = s_reel[i];
                }
                s_state = SLOTS_SPINNING;
            }
            break;

        case SLOTS_SPINNING:
        {
            uint32_t elapsed = now - s_spin_start;
            uint8_t  any_change = 0U;

            /* Advance animation each FRAME_MS */
            if ((now - s_last_frame) >= FRAME_MS)
            {
                s_last_frame = now;
                any_change   = 1U;

                for (i = 0U; i < 3U; i++)
                {
                    if (!s_stopped[i])
                    {
                        s_spin_pos[i] = (uint8_t)((s_spin_pos[i] + 1U) % REEL_SYMBOLS);
                        s_reel[i]     = s_spin_pos[i];
                    }
                }

                /* Stop reels at their scheduled times */
                if (!s_stopped[0] && elapsed >= STOP_R1_MS)
                {
                    s_stopped[0] = 1U;
                    s_reel[0]    = s_final[0];
                }
                if (!s_stopped[1] && elapsed >= STOP_R2_MS)
                {
                    s_stopped[1] = 1U;
                    s_reel[1]    = s_final[1];
                }
                if (!s_stopped[2] && elapsed >= STOP_R3_MS)
                {
                    s_stopped[2] = 1U;
                    s_reel[2]    = s_final[2];
                }

                if (any_change)
                {
                    draw_reels();
                    buzzer_play_melody(SFX_REEL_TICK, SFX_REEL_TICK_LEN);
                }
            }

            /* All stopped — evaluate payout */
            if (s_stopped[0] && s_stopped[1] && s_stopped[2])
            {
                uint16_t bet_val = BET_OPTIONS[s_bet_idx];
                uint8_t  r0      = s_final[0];
                uint8_t  r1      = s_final[1];
                uint8_t  r2      = s_final[2];
                uint16_t payout  = 0U;
                uint8_t  is_jackpot = 0U;

                if (r0 == r1 && r1 == r2)
                {
                    /* All three match */
                    if (r0 == 2U)   /* index 2 = SEVEN */
                    {
                        payout     = (uint16_t)(50U * bet_val);
                        is_jackpot = 1U;
                        buzzer_play_melody(SFX_JACKPOT, SFX_JACKPOT_LEN);
                    }
                    else
                    {
                        payout = (uint16_t)(10U * bet_val);
                        buzzer_play_melody(SFX_WIN, SFX_WIN_LEN);
                    }
                }
                else if (r0 == r1 || r1 == r2 || r0 == r2)
                {
                    /* Exactly two match */
                    payout = (uint16_t)(2U * bet_val);
                    buzzer_play_melody(SFX_WIN, SFX_WIN_LEN);
                }
                else
                {
                    buzzer_play_melody(SFX_LOSE, SFX_LOSE_LEN);
                }

                g_credits += payout;
                s_state    = SLOTS_RESULT;
                draw_result_row((payout > 0U), payout, is_jackpot);
            }
            break;
        }

        case SLOTS_RESULT:
            if (e == BTN_A_PRESS || e == BTN_B_PRESS)
            {
                s_state = SLOTS_IDLE;
                /* Update reel display to show final stopped positions */
                draw_reels();
                draw_status();
            }
            break;
    }
}

void slots_exit(void)
{
    /* Nothing to clean up */
}
