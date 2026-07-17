/*
 * coinflip.c
 *
 * Coin Flip game logic.
 *
 * States:
 *   CF_IDLE     — waiting for player to choose and press BTN_A
 *   CF_FLIPPING — 600 ms coin animation (alternates HEADS/TAILS glyph)
 *   CF_RESULT   — result displayed; waiting for any button to continue
 *
 * Screen layout:
 *   Row 0: "Call: [glyph] HEADS"  or "Call: [glyph] TAILS"
 *   Row 1: "Bet:10   Cr:XXXX"
 *   During flip:
 *   Row 0: "[glyph]  FLIPPING..."
 *   After result:
 *   Row 0: "[glyph]  WIN! +10"  or  "[glyph]  LOSE -10"
 */

#include "coinflip.h"
#include "ui.h"
#include "buzzer.h"
#include "rng.h"
#include "lcd_driver.h"
#include <stdint.h>

#define CF_BET          10U
#define CF_WIN_PAYOUT   20U

#define CF_FLIP_MS      600U    /* total animation duration */
#define CF_FRAME_MS     80U     /* ms per animation frame   */
#define CF_FLIP_FRAMES  (CF_FLIP_MS / CF_FRAME_MS)

/* Shared 1 ms tick */
extern volatile uint32_t g_ms_tick;

typedef enum
{
    CF_IDLE     = 0,
    CF_FLIPPING = 1,
    CF_RESULT   = 2
} cf_state_t;

static cf_state_t s_cf_state;
static uint8_t    s_call;           /* 0 = HEADS, 1 = TAILS */
static uint8_t    s_result;         /* 0 = HEADS, 1 = TAILS */
static uint32_t   s_flip_start;
static uint8_t    s_frame;
static uint16_t   s_tick_hz;       /* rising pitch tracker */

static void draw_idle(void)
{
    /* Row 0: "Call: [glyph] HEADS" */
    Lcd_SetCursor(0U, 0U);
    Lcd_WriteString("Call: ");
    Lcd_WriteChar((uint8_t)(s_call == 0U ? GLYPH_COIN_H : GLYPH_COIN_T));
    Lcd_WriteChar(' ');
    Lcd_WriteString(s_call == 0U ? "HEADS   " : "TAILS   ");

    /* Row 1: "Bet:10   Cr:XXXX" */
    Lcd_SetCursor(0U, 1U);
    Lcd_WriteString("Bet:10   Cr:");
    ui_show_score(g_credits);
}

static void draw_flip_frame(uint8_t heads_showing)
{
    uint8_t glyph = (heads_showing ? GLYPH_COIN_H : GLYPH_COIN_T);
    Lcd_SetCursor(0U, 0U);
    Lcd_WriteChar(glyph);
    Lcd_WriteString("  Flipping... ");
}

static void draw_result(uint8_t won)
{
    uint8_t glyph = (s_result == 0U ? GLYPH_COIN_H : GLYPH_COIN_T);
    Lcd_SetCursor(0U, 0U);
    Lcd_WriteChar(glyph);
    Lcd_WriteChar(' ');
    if (won)
    {
        Lcd_WriteString(s_result == 0U ? "HEADS" : "TAILS");
        Lcd_WriteString(" WIN!+10");
    }
    else
    {
        Lcd_WriteString(s_result == 0U ? "HEADS" : "TAILS");
        Lcd_WriteString(" LOSE-10");
    }
    Lcd_SetCursor(0U, 1U);
    Lcd_WriteString("Bet:10   Cr:");
    ui_show_score(g_credits);
}

void coinflip_enter(void)
{
    s_cf_state = CF_IDLE;
    s_call     = 0U;   /* default: HEADS */
    Lcd_Clear();
    draw_idle();
}

void coinflip_update(btn_event_t e)
{
    uint32_t now = g_ms_tick;
    uint8_t  elapsed_frames;
    uint8_t  cur_frame;
    uint8_t  won;

    switch (s_cf_state)
    {
        case CF_IDLE:
            if (e == BTN_B_PRESS)
            {
                s_call ^= 1U;   /* toggle HEADS/TAILS */
                buzzer_play_melody(SFX_NAV_TICK, SFX_NAV_TICK_LEN);
                draw_idle();
            }
            else if (e == BTN_A_PRESS)
            {
                if (g_credits < CF_BET)
                {
                    /* Not enough credits — flash row 1 */
                    Lcd_SetCursor(0U, 1U);
                    Lcd_WriteString("Need 10 credits!");
                    return;
                }
                g_credits  -= CF_BET;
                s_flip_start = now;
                s_frame      = 0U;
                s_tick_hz    = 400U;
                s_result     = rng_u8(2U);
                s_cf_state   = CF_FLIPPING;
                Lcd_Clear();
                draw_flip_frame(1U);  /* start showing HEADS side */
            }
            break;

        case CF_FLIPPING:
            elapsed_frames = (uint8_t)((now - s_flip_start) / CF_FRAME_MS);

            if (elapsed_frames >= CF_FLIP_FRAMES)
            {
                /* Animation done — reveal result */
                s_cf_state = CF_RESULT;
                won = (s_result == s_call) ? 1U : 0U;
                if (won)
                {
                    g_credits += CF_WIN_PAYOUT;
                    buzzer_play_melody(SFX_WIN, SFX_WIN_LEN);
                }
                else
                {
                    buzzer_play_melody(SFX_LOSE, SFX_LOSE_LEN);
                }
                Lcd_Clear();
                draw_result(won);
            }
            else if (elapsed_frames > s_frame)
            {
                /* New frame — advance animation and play rising tick */
                s_frame = elapsed_frames;
                cur_frame = (uint8_t)(elapsed_frames & 1U); /* alternate 0/1 */
                draw_flip_frame(cur_frame);
                buzzer_tone(s_tick_hz, 25U);
                s_tick_hz += 50U;   /* pitch rises each frame */
            }
            break;

        case CF_RESULT:
            if (e == BTN_A_PRESS || e == BTN_B_PRESS)
            {
                /* Return to idle to play again */
                s_cf_state = CF_IDLE;
                Lcd_Clear();
                draw_idle();
            }
            break;
    }
}

void coinflip_exit(void)
{
    /* Nothing to clean up */
}
