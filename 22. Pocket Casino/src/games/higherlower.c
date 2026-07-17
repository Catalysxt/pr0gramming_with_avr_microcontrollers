/*
 * higherlower.c
 *
 * Higher or Lower — guess whether the next card value is higher or lower
 * than the current one.  Values are 1–13; equal = lose.
 *
 * States:
 *   HL_GUESSING — display current card, await BTN_A (Higher) or BTN_B (Lower)
 *   HL_RESULT   — show correct/wrong feedback for 1 s then auto-advance
 *
 * Screen layout:
 *   Row 0: "Card: K  > or <?"    (card displayed as A/2-10/J/Q/K)
 *   Row 1: "Streak:03  +30 Cr:"  + 4-digit score
 *
 * Result row:
 *   Row 0: "Card: K  CORRECT!"   or  "Card: 5  WRONG!"
 */

#include "higherlower.h"
#include "ui.h"
#include "buzzer.h"
#include "rng.h"
#include "lcd_driver.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HL_WIN_CREDITS  10U
#define RESULT_HOLD_MS  1000U   /* show result for 1 s then auto-proceed */
#define JACKPOT_STREAK  10U

extern volatile uint32_t g_ms_tick;

typedef enum { HL_GUESSING = 0, HL_RESULT = 1 } hl_state_t;

static hl_state_t s_state;
static uint8_t    s_current;   /* 1–13 */
static uint8_t    s_next;      /* 1–13, revealed on HL_RESULT */
static uint8_t    s_streak;
static uint8_t    s_last_correct;
static uint32_t   s_result_time;

static const char * const CARD_NAMES[] =
{
    "",    /* unused index 0 */
    "A",  "2",  "3",  "4",  "5",  "6",  "7",
    "8",  "9",  "10", "J",  "Q",  "K"
};

static void draw_guess(void)
{
    char buf[17];
    const char *name = CARD_NAMES[s_current];
    uint8_t nlen;

    /* Row 0: "Card: K   Hi/Lo?" — 16 chars exactly */
    Lcd_SetCursor(0U, 0U);
    Lcd_WriteString("Card: ");
    Lcd_WriteString(name);
    nlen = (uint8_t)strlen(name);
    while (nlen < 3U) { Lcd_WriteChar(' '); nlen++; }
    Lcd_WriteString(" Hi/Lo?");

    /* Row 1: "Str:03 +030 Cr:XXXX" — streak * 10 max = 990 for streak 99 */
    Lcd_SetCursor(0U, 1U);
    snprintf(buf, sizeof(buf), "Str:%02u +%3u ", s_streak,
             (uint16_t)s_streak * HL_WIN_CREDITS);
    Lcd_WriteString(buf);
    Lcd_WriteString("Cr:");
    ui_show_score(g_credits);
}

static void draw_result_screen(uint8_t correct)
{
    char buf[17];
    const char *cn = CARD_NAMES[s_current];
    const char *nn = CARD_NAMES[s_next];
    uint8_t nlen;

    /* Row 0: "K   >K   CORRECT!" — always 16 chars: 3+1+3+1+8 */
    Lcd_SetCursor(0U, 0U);
    Lcd_WriteString(cn);
    nlen = (uint8_t)strlen(cn);
    while (nlen < 3U) { Lcd_WriteChar(' '); nlen++; }
    Lcd_WriteChar(correct ? '>' : 'x');
    Lcd_WriteString(nn);
    nlen = (uint8_t)strlen(nn);
    while (nlen < 3U) { Lcd_WriteChar(' '); nlen++; }
    Lcd_WriteString(correct ? " WIN!   " : " WRONG! ");

    Lcd_SetCursor(0U, 1U);
    snprintf(buf, sizeof(buf), "Streak:%02u    ", s_streak);
    Lcd_WriteString(buf);
    Lcd_WriteString("Cr:");
    ui_show_score(g_credits);
}

void higherlower_enter(void)
{
    s_state   = HL_GUESSING;
    s_streak  = 0U;
    s_current = (uint8_t)(rng_u8(13U) + 1U);
    Lcd_Clear();
    draw_guess();
}

void higherlower_update(btn_event_t e)
{
    uint8_t guessed_higher;
    uint8_t correct;

    switch (s_state)
    {
        case HL_GUESSING:
            if (e != BTN_A_PRESS && e != BTN_B_PRESS)
            {
                return;
            }

            guessed_higher = (e == BTN_A_PRESS) ? 1U : 0U;
            s_next = (uint8_t)(rng_u8(13U) + 1U);

            /* Equal always loses */
            if (s_next == s_current)
            {
                correct = 0U;
            }
            else if (guessed_higher && (s_next > s_current))
            {
                correct = 1U;
            }
            else if (!guessed_higher && (s_next < s_current))
            {
                correct = 1U;
            }
            else
            {
                correct = 0U;
            }

            s_last_correct = correct;

            if (correct)
            {
                s_streak++;
                g_credits += HL_WIN_CREDITS;

                if (s_streak == JACKPOT_STREAK)
                {
                    buzzer_play_melody(SFX_JACKPOT, SFX_JACKPOT_LEN);
                }
                else
                {
                    buzzer_play_melody(SFX_CONFIRM, SFX_CONFIRM_LEN);
                }
            }
            else
            {
                s_streak = 0U;
                buzzer_play_melody(SFX_LOSE, SFX_LOSE_LEN);
            }

            s_result_time = g_ms_tick;
            s_state       = HL_RESULT;
            Lcd_Clear();
            draw_result_screen(correct);
            break;

        case HL_RESULT:
            /* Auto-advance after RESULT_HOLD_MS, or on button press */
            if ((e == BTN_A_PRESS) || (e == BTN_B_PRESS) ||
                ((g_ms_tick - s_result_time) >= RESULT_HOLD_MS))
            {
                s_current = s_next;
                s_state   = HL_GUESSING;
                Lcd_Clear();
                draw_guess();
            }
            break;
    }
}

void higherlower_exit(void)
{
    /* Nothing to clean up */
}
