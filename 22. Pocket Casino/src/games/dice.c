/*
 * dice.c
 *
 * Dice (Yahtzee-lite) game — two dice, animated roll, configurable target.
 *
 * States:
 *   DICE_IDLE    — show current target and credit count; wait for input
 *   DICE_ROLLING — 700 ms animation (CGRAM slot 7 updated in-place per frame)
 *   DICE_RESULT  — show outcome; wait for button to continue
 *
 * Screen layout (IDLE / RESULT):
 *   Row 0: "[d1] [d2]  Sum:X "   (ASCII digits after animation)
 *   Row 1: "Tgt:OVER   Cr:XXXX"
 *
 * CGRAM slot 7 (GLYPH_DICE_PIP) is updated each frame during the roll
 * animation to show a tumbling pattern. After rolling, ASCII digits are
 * used to display the final values so CGRAM slot 7 can revert to its
 * default pattern without disturbing other games.
 *
 * Payouts:
 *   DOUBLES  → 5 × bet
 *   OVER 7   → 2 × bet
 *   UNDER 7  → 2 × bet
 *   EXACT 7  → 4 × bet
 *   No match → 0
 */

#include "dice.h"
#include "ui.h"
#include "buzzer.h"
#include "rng.h"
#include "lcd_driver.h"
#include <stdint.h>
#include <stdio.h>

#define DICE_BET        5U

#define DICE_ROLL_MS    700U
#define DICE_FRAME_MS   100U
#define DICE_FRAMES     (DICE_ROLL_MS / DICE_FRAME_MS)

extern volatile uint32_t g_ms_tick;

typedef enum { TGT_DOUBLES = 0, TGT_OVER7 = 1, TGT_UNDER7 = 2, TGT_EXACT7 = 3 } dice_target_t;
static const char * const TARGET_NAMES[] = { "DOUBLES", "OVER 7 ", "UNDER 7", "EXACT 7" };

typedef enum { DICE_IDLE = 0, DICE_ROLLING = 1, DICE_RESULT = 2 } dice_state_t;

static dice_state_t  s_state;
static dice_target_t s_target;
static uint8_t       s_d1, s_d2;
static uint32_t      s_roll_start;
static uint8_t       s_frame;

/* Animation pip patterns for CGRAM slot 7 (tumbling dice) */
static const uint8_t ANIM_PATTERNS[4][8] =
{
    { 0b11111, 0b10001, 0b10101, 0b10001, 0b10101, 0b10001, 0b11111, 0b00000 }, /* 5 pips */
    { 0b11111, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11111, 0b00000 }, /* blank  */
    { 0b11111, 0b10001, 0b10101, 0b10001, 0b10001, 0b10001, 0b11111, 0b00000 }, /* 2 pips */
    { 0b11111, 0b10101, 0b10001, 0b11111, 0b10001, 0b10101, 0b11111, 0b00000 }, /* 6 pips */
};

static void update_dice_cgram(uint8_t anim_idx)
{
    Lcd_WriteCustomChar(GLYPH_DICE_PIP, ANIM_PATTERNS[anim_idx % 4U]);
    /* Lcd_WriteCustomChar resets DDRAM to 0x00; restore cursor after. */
}

static void write_dice_row0(void)
{
    /* "[d1] [d2]  Sum:XX" — dice are 1-6 (single digit), sum is 2-12 */
    uint8_t sum = (uint8_t)(s_d1 + s_d2);
    Lcd_SetCursor(0U, 0U);
    Lcd_WriteChar('[');
    Lcd_WriteChar((uint8_t)('0' + s_d1));
    Lcd_WriteChar(']');
    Lcd_WriteChar(' ');
    Lcd_WriteChar('[');
    Lcd_WriteChar((uint8_t)('0' + s_d2));
    Lcd_WriteChar(']');
    Lcd_WriteString("  Sum:");
    if (sum >= 10U)
    {
        Lcd_WriteChar('1');
        Lcd_WriteChar((uint8_t)('0' + sum - 10U));
    }
    else
    {
        Lcd_WriteChar((uint8_t)('0' + sum));
        Lcd_WriteChar(' ');
    }
    Lcd_WriteChar(' ');
}

static void draw_idle(void)
{
    /* Row 0: "[d1] [d2]  Sum:X " */
    write_dice_row0();

    /* Row 1: "Tgt:OVER 7 Cr:XXXX" */
    Lcd_SetCursor(0U, 1U);
    Lcd_WriteString("Tgt:");
    Lcd_WriteString(TARGET_NAMES[s_target]);
    Lcd_WriteString(" ");
    Lcd_WriteString("Cr:");
    ui_show_score(g_credits);
}

static void draw_rolling(uint8_t anim_idx)
{
    /* Row 0: "[pip][pip]  Rolling" — use CGRAM slot 7 for both dice during anim */
    Lcd_SetCursor(0U, 0U);
    Lcd_WriteChar(GLYPH_DICE_PIP);
    Lcd_WriteString("  ");
    Lcd_WriteChar(GLYPH_DICE_PIP);
    Lcd_WriteString("  Rolling   ");
    (void)anim_idx;
}

static void draw_result(uint8_t won, uint16_t payout)
{
    char buf[17];

    write_dice_row0();

    Lcd_SetCursor(0U, 1U);
    if (won)
    {
        snprintf(buf, sizeof(buf), "WIN +%-4u  Cr:", payout);
        Lcd_WriteString(buf);
        ui_show_score(g_credits);
    }
    else
    {
        Lcd_WriteString("No match   Cr:");
        ui_show_score(g_credits);
    }
}

void dice_enter(void)
{
    s_state  = DICE_IDLE;
    s_target = TGT_OVER7;
    s_d1     = 1U;
    s_d2     = 1U;
    Lcd_Clear();
    draw_idle();
}

void dice_update(btn_event_t e)
{
    uint32_t now = g_ms_tick;
    uint8_t  elapsed_frames;
    uint8_t  sum;
    uint8_t  won;
    uint16_t payout;
    uint8_t  anim_idx;

    switch (s_state)
    {
        case DICE_IDLE:
            if (e == BTN_B_PRESS)
            {
                s_target = (dice_target_t)(((uint8_t)s_target + 1U) % 4U);
                buzzer_play_melody(SFX_NAV_TICK, SFX_NAV_TICK_LEN);
                draw_idle();
            }
            else if (e == BTN_A_PRESS)
            {
                if (g_credits < DICE_BET)
                {
                    Lcd_SetCursor(0U, 1U);
                    Lcd_WriteString("Need 5 credits! ");
                    return;
                }
                g_credits    -= DICE_BET;
                s_d1          = (uint8_t)(rng_u8(6U) + 1U);
                s_d2          = (uint8_t)(rng_u8(6U) + 1U);
                s_roll_start  = now;
                s_frame       = 0U;
                s_state       = DICE_ROLLING;
                update_dice_cgram(0U);
                Lcd_Clear();
                draw_rolling(0U);
            }
            break;

        case DICE_ROLLING:
            elapsed_frames = (uint8_t)((now - s_roll_start) / DICE_FRAME_MS);

            if (elapsed_frames >= DICE_FRAMES)
            {
                /* Roll complete — evaluate result */
                sum  = (uint8_t)(s_d1 + s_d2);
                won  = 0U;
                payout = 0U;

                switch (s_target)
                {
                    case TGT_DOUBLES:
                        if (s_d1 == s_d2)
                        {
                            won    = 1U;
                            payout = (uint16_t)(5U * DICE_BET);
                        }
                        break;
                    case TGT_OVER7:
                        if (sum > 7U)
                        {
                            won    = 1U;
                            payout = (uint16_t)(2U * DICE_BET);
                        }
                        break;
                    case TGT_UNDER7:
                        if (sum < 7U)
                        {
                            won    = 1U;
                            payout = (uint16_t)(2U * DICE_BET);
                        }
                        break;
                    case TGT_EXACT7:
                        if (sum == 7U)
                        {
                            won    = 1U;
                            payout = (uint16_t)(4U * DICE_BET);
                        }
                        break;
                    default:
                        break;
                }

                if (won)
                {
                    g_credits += payout;
                    buzzer_play_melody((payout >= 20U) ? SFX_JACKPOT : SFX_WIN,
                                       (payout >= 20U) ? SFX_JACKPOT_LEN : SFX_WIN_LEN);
                }
                else
                {
                    buzzer_play_melody(SFX_LOSE, SFX_LOSE_LEN);
                }

                /* Restore default dice pip pattern in CGRAM */
                {
                    static const uint8_t DEFAULT_PIP[8] =
                    {
                        0b11111, 0b10001, 0b10001, 0b10101,
                        0b10001, 0b10001, 0b11111, 0b00000
                    };
                    Lcd_WriteCustomChar(GLYPH_DICE_PIP, DEFAULT_PIP);
                }

                s_state = DICE_RESULT;
                Lcd_Clear();
                draw_result(won, payout);
            }
            else if (elapsed_frames > s_frame)
            {
                s_frame    = elapsed_frames;
                anim_idx   = elapsed_frames % 4U;
                update_dice_cgram(anim_idx);
                draw_rolling(anim_idx);
                buzzer_play_melody(SFX_REEL_TICK, SFX_REEL_TICK_LEN);
            }
            break;

        case DICE_RESULT:
            if (e == BTN_A_PRESS || e == BTN_B_PRESS)
            {
                s_state = DICE_IDLE;
                Lcd_Clear();
                draw_idle();
            }
            break;
    }
}

void dice_exit(void)
{
    /* Nothing to clean up */
}
