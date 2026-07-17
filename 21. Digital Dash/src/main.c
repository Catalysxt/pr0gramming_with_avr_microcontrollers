/*
 * main.c
 *
 * Purpose : Top-level state machine for digital_dash.
 * Hardware : All peripherals initialised here; main loop is non-blocking.
 * Datasheet: ATmega328P §6 (reset and interrupt handling), §7 (EEPROM)
 *
 * State machine:
 *   STATE_SPLASH   — title screen + high score; wait for JUMP press
 *   STATE_PLAYING  — run game tick every TICKSPEED_MS
 *   STATE_GAMEOVER — show score/HI; blink "Jump to Continue"; wait for JUMP
 */

#include "hardware_connections.h"
#include "sys_tick.h"
#include "lcd_driver.h"
#include "sprites.h"
#include "rng.h"
#include "buttons.h"
#include "buzzer.h"
#include "game.h"
#include "high_score.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>     /* _delay_ms — used only in init, not in game loop */
#include <stdlib.h>         /* itoa — integer to string without printf overhead */
#include <string.h>         /* memset — pad display rows with spaces */
#include <stdint.h>
#include <stdbool.h>

/* ---- Application state machine ----------------------------------------- */

typedef enum {
    STATE_SPLASH,
    STATE_PLAYING,
    STATE_GAMEOVER,
} state_t;

/* ---- Screen rendering helpers ------------------------------------------ */

/*
 * Write val as a decimal string, then pad with spaces up to min_cols total
 * characters. Cursor must be pre-positioned.
 */
static void lcd_write_u16(uint16_t val, uint8_t min_cols)
{
    char buf[6];            /* uint16_t max = 65535 → 5 digits + null */
    itoa((int)val, buf, 10);
    uint8_t len = (uint8_t)strlen(buf);
    Lcd_WriteString(buf);
    for (uint8_t i = len; i < min_cols; i++) {
        Lcd_WriteChar(' ');
    }
}

static void show_splash(uint16_t hi)
{
    Lcd_Clear();
    Lcd_SetCursor(0u, 0u);
    Lcd_WriteString("  Digital Dash  ");

    /* Row 1: "HI:XXXX PressJMP" (always 16 chars — hi capped to 4 digits display) */
    Lcd_SetCursor(0u, 1u);
    Lcd_WriteString("HI:");
    lcd_write_u16(hi, 4u);      /* 4 columns min, spaces fill remainder */
    Lcd_WriteString(" PressJMP");
}

static void show_gameover(uint16_t score, uint16_t hi)
{
    /* Row 0 example: "Score:42    HI:7" (always 16 chars) */
    Lcd_Clear();
    Lcd_SetCursor(0u, 0u);
    Lcd_WriteString("Score:");
    lcd_write_u16(score, 5u);   /* 5 columns keeps "HI:" anchored at col 11 */
    Lcd_WriteString("HI:");
    lcd_write_u16(hi, 3u);      /* 3 columns → row always fills to col 16    */
}

/* ---- main -------------------------------------------------------------- */

int main(void)
{
    /* ------------------------------------------------------------------ */
    /* Initialisation                                                       */
    /* ------------------------------------------------------------------ */

    sys_tick_init();

    /* LCD power-on sequence requires at least 40 ms after VCC rises to 2.7V.
     * This is the only _delay_ms call permitted outside init code.
     * HD44780 datasheet §4 — Initialisation by Instruction, Figure 24. */
    _delay_ms(50);
    Lcd_Init();

    sprites_load_all();
    rng_init();

    /* Tristate buzzer pin — Timer2 will set DDR when first tone fires. */
    BUZZER_DDR &= ~(1u << BUZZER_BIT);

    buttons_init();

    uint16_t hi = high_score_load();

    sei();  /* Enable global interrupts — PCINT1 and TIMER0_COMPA now active. */

    /* ------------------------------------------------------------------ */
    /* State machine                                                        */
    /* ------------------------------------------------------------------ */

    state_t  state    = STATE_SPLASH;
    game_t   game;
    uint32_t tick_anchor;
    uint32_t blink_anchor;
    bool     blink_on  = false;

    show_splash(hi);

    for (;;) {
        buzzer_service();   /* always poll — stops buzzer when duration expires */

        switch (state) {

        /* ---- Splash screen ---- */
        case STATE_SPLASH:
            if (buttons_consume_jump_edge()) {
                Lcd_Clear();
                game_init(&game);
                tick_anchor = sys_millis();
                state = STATE_PLAYING;
            }
            break;

        /* ---- Active gameplay ---- */
        case STATE_PLAYING:
            if (sys_elapsed(&tick_anchor, TICKSPEED_MS)) {
                bool hit = game_tick_update(&game);
                if (hit) {
                    game_end(&game, hi, &hi);
                    show_gameover(game.score, hi);
                    blink_anchor = sys_millis();
                    blink_on     = true;
                    /* Row 1: first blink state — visible */
                    Lcd_SetCursor(0u, 1u);
                    Lcd_WriteString("Jump to Continue");
                    state = STATE_GAMEOVER;
                }
            }
            break;

        /* ---- Game over screen ---- */
        case STATE_GAMEOVER:
            /* Blink "Jump to Continue" every 500 ms. */
            if (sys_elapsed(&blink_anchor, 500u)) {
                blink_on = !blink_on;
                Lcd_SetCursor(0u, 1u);
                if (blink_on) {
                    Lcd_WriteString("Jump to Continue");
                } else {
                    Lcd_WriteString("                ");
                }
            }

            if (buttons_consume_jump_edge()) {
                /* Clear screen, reload sprites (CGRAM survives power; re-load
                 * to be safe after Lcd_Clear), reset game and start playing. */
                Lcd_Clear();
                sprites_load_all();
                game_init(&game);
                tick_anchor = sys_millis();
                state = STATE_PLAYING;
            }
            break;

        default:
            /* Unreachable — defend against corrupted state. */
            state = STATE_SPLASH;
            show_splash(hi);
            break;
        }
    }

    return 0;   /* never reached */
}
