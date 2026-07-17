/*
 * main.c
 *
 * Pocket Casino — ATmega328P @ 16 MHz external crystal.
 *
 * Initialisation order (interrupts disabled throughout):
 *   1. Timer0  — 1 ms system tick (CTC, TIMER0_COMPA_vect)
 *   2. buttons — PCINT1_vect on PC0/PC1 (needs g_ms_tick from Timer0)
 *   3. rng     — ADC0 seed (uses ADC hardware; done before sei())
 *   4. buzzer  — Timer2 CTC on OC2A/PB3
 *   5. LCD     — Lcd_Init() (uses _delay_ms internally)
 *   6. CGRAM   — load 8 custom glyphs
 *   7. sei()   — global interrupt enable
 *   8. Splash  — draw initial screen
 *
 * Superloop (runs forever):
 *   - Read one button event
 *   - Advance buzzer (auto-stop + melody step)
 *   - Dispatch to the active game or UI handler
 *   - Check credit exhaustion → game-over transition
 *
 * ATmega328P datasheet references:
 *   Section 15   — Timer/Counter0 (8-bit)
 *   Section 15.7 — TCCR0A: WGM01:00 = 10 → CTC
 *   Section 15.9 — TCCR0B: CS01:00 = 011 → /64 prescaler
 *   Section 15.8 — OCR0A: compare value for 1 ms tick at 16 MHz
 *   Section 15.6 — TIMSK0: OCIE0A → enable COMPA interrupt
 *
 * 1 ms tick calculation:
 *   Timer0 CTC, /64 prescaler → tick rate = 16 000 000 / 64 = 250 000 Hz
 *   OCR0A = (250 000 / 1000) - 1 = 249
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "hardware_connections.h"
#include "buttons.h"
#include "buzzer.h"
#include "rng.h"
#include "ui.h"
#include "lcd_driver.h"
#include "games/slots.h"
#include "games/coinflip.h"
#include "games/higherlower.h"
#include "games/dice.h"

/* Shared 1 ms counter — used by buttons, buzzer, and all game timers.
 * volatile: written in ISR, read in main context. */
volatile uint32_t g_ms_tick = 0U;

/* ==========================================================================
 * Timer0 Compare Match A ISR — fires every 1 ms
 * ATmega328P datasheet Section 15.6 (TIMSK0), Section 15.9 (TCCR0B)
 * ========================================================================== */
ISR(TIMER0_COMPA_vect)
{
    g_ms_tick++;
}

/* ==========================================================================
 * Timer0 initialisation for 1 ms CTC tick
 * ========================================================================== */
static void timer0_init(void)
{
    /* CTC mode: WGM01:00 = 10 (TCCR0A bits WGM01=1, WGM00=0).
     * ATmega328P datasheet Section 15.7.1, Table 15-8. */
    TCCR0A = (1U << WGM01);

    /* /64 prescaler: CS01:CS00 = 011.
     * ATmega328P datasheet Section 15.9.2, Table 15-9. */
    TCCR0B = (1U << CS01) | (1U << CS00);

    /* OCR0A = 249 → interrupt fires every 250 Timer0 counts = 1 ms.
     * Calculation: 16 000 000 Hz / 64 / 1000 Hz − 1 = 249. */
    OCR0A = 249U;

    /* Enable Output Compare A interrupt.
     * ATmega328P datasheet Section 15.6.1 (TIMSK0). */
    TIMSK0 = (1U << OCIE0A);
}

/* ==========================================================================
 * Return to menu helper — called on BTN_B long-press from any game.
 * ========================================================================== */
static void return_to_menu(void)
{
    switch (g_state)
    {
        case STATE_SLOTS:       slots_exit();       break;
        case STATE_COINFLIP:    coinflip_exit();    break;
        case STATE_HIGHERLOWER: higherlower_exit(); break;
        case STATE_DICE:        dice_exit();        break;
        default: break;
    }
    buzzer_stop();
    g_state = STATE_MENU;
    ui_draw_menu();
}

/* ==========================================================================
 * main
 * ========================================================================== */
int main(void)
{
    btn_event_t e;

    /* ------ Initialisations (interrupts off) ------------------------------ */
    timer0_init();
    buttons_init();
    rng_init();
    buzzer_init();
    Lcd_Init();
    ui_load_cgram();

    sei();  /* ATmega328P datasheet Section 6.8 — SREG bit I */

    /* ------ Initial screen ------------------------------------------------ */
    ui_draw_splash();

    /* ------ Superloop ----------------------------------------------------- */
    for (;;)
    {
        e = buttons_poll();
        buzzer_update();

        /* BTN_B long press always returns to menu, from any game state. */
        if ((e == BTN_B_LONG) &&
            (g_state != STATE_MENU) &&
            (g_state != STATE_SPLASH) &&
            (g_state != STATE_GAMEOVER))
        {
            return_to_menu();
            continue;
        }

        /* Dispatch to active state handler. */
        switch (g_state)
        {
            case STATE_SPLASH:
            case STATE_MENU:
                ui_menu_update(e);
                break;

            case STATE_SLOTS:
                slots_update(e);
                break;

            case STATE_COINFLIP:
                coinflip_update(e);
                break;

            case STATE_HIGHERLOWER:
                higherlower_update(e);
                break;

            case STATE_DICE:
                dice_update(e);
                break;

            case STATE_GAMEOVER:
                ui_gameover_update(e);
                break;

            default:
                break;
        }

        /* Credit-exhaustion check — any game can deplete credits to 0. */
        if ((g_credits == 0U) &&
            (g_state != STATE_MENU) &&
            (g_state != STATE_SPLASH) &&
            (g_state != STATE_GAMEOVER))
        {
            switch (g_state)
            {
                case STATE_SLOTS:       slots_exit();       break;
                case STATE_COINFLIP:    coinflip_exit();    break;
                case STATE_HIGHERLOWER: higherlower_exit(); break;
                case STATE_DICE:        dice_exit();        break;
                default: break;
            }
            g_state = STATE_GAMEOVER;
            buzzer_play_melody(SFX_LOSE, SFX_LOSE_LEN);
            ui_draw_gameover();
        }
    }

    return 0; /* never reached */
}
