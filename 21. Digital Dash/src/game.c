/*
 * game.c
 *
 * Purpose : Core game logic — faithful port of the original Arduino loop().
 * Hardware : LCD display (via lcd_driver), buzzer (via buzzer module).
 * Datasheet: HD44780 §8 (custom char slots 0-7 writeable via WriteChar)
 *
 * Design notes:
 *   - No blocking delays anywhere; timing is driven by sys_elapsed() in main.
 *   - Jump and duck SFX are fired by the button ISR, not here.
 *   - game_tick_update() mirrors the original loop() body line-for-line,
 *     translated to bare-metal idioms.
 */

#include "game.h"
#include "sprites.h"
#include "buttons.h"
#include "buzzer.h"
#include "rng.h"
#include "high_score.h"
#include "lcd_driver.h"

#include <stdint.h>
#include <stdbool.h>

/* Initial obstacle positions (off-screen right, same as original sketch). */
#define HILL_INIT_X   25
#define CROW_INIT_X   40

/* Respawn base positions after a successful dodge. */
#define HILL_RESPAWN_BASE  16u
#define HILL_RESPAWN_RAND   8u   /* rand()%8 in original  */
#define CROW_RESPAWN_BASE  24u
#define CROW_RESPAWN_RAND  16u   /* rand()%16 in original */

/* ---- Internal helpers --------------------------------------------------- */

/*
 * Draw the player sprite at column 0.
 * Sprite selection replicates the original drawSprites() exactly.
 * Also clears columns 1-15 of both rows to erase old obstacle positions
 * before the obstacles are re-drawn at their new columns.
 */
static void draw_player(const game_t *g)
{
    /* Choose sprite based on state (mirrors original drawSprites logic). */
    uint8_t sprite;
    if (buttons_ducking()) {
        sprite = SPRITE_DUCK;
    } else if (g->player_y) {
        sprite = SPRITE_JUMP;
    } else {
        /* Alternate walking frames every 2 ticks: 0,1 → step1; 2,3 → step2 */
        sprite = ((g->game_tick % 4u) < 2u) ? SPRITE_STEP1 : SPRITE_STEP2;
    }

    /* Player drawn at col 0; row 0 when jumping (player_y=true), row 1 otherwise. */
    uint8_t player_row = g->player_y ? 0u : 1u;
    Lcd_SetCursor(0u, player_row);
    Lcd_WriteChar(sprite);

    /* Clear the trail on both rows (cols 1-15) so old obstacle positions
     * disappear before obstacles are redrawn at their new columns.
     * Original: lcd.setCursor(1,1); lcd.print("               ");  (15 spaces)
     *           lcd.setCursor(1,0); lcd.print("               ");           */
    Lcd_SetCursor(1u, 1u);
    for (uint8_t c = 1u; c < 16u; c++) { Lcd_WriteChar(' '); }
    Lcd_SetCursor(1u, 0u);
    for (uint8_t c = 1u; c < 16u; c++) { Lcd_WriteChar(' '); }
}

/* ---- Public API --------------------------------------------------------- */

void game_init(game_t *g)
{
    g->jump_phase = JUMP_LENGTH + 1u;   /* grounded */
    g->game_tick  = 0u;
    g->hill_x     = HILL_INIT_X;
    g->crow_x     = CROW_INIT_X;
    g->player_y   = false;
    g->crow_go    = false;
    g->score      = 0u;
}

bool game_tick_update(game_t *g)
{
    /* ---- Update player vertical position ---- */
    /* player_y = true means airborne (row 0 = top). */
    g->player_y = (g->jump_phase < JUMP_LENGTH);

    /* Consume jump edge from button ISR. Trigger only when grounded and
     * not ducking (mirrors original seeJumping() guard on jumpPhase). */
    if (buttons_consume_jump_edge()) {
        if ((g->jump_phase > (JUMP_LENGTH + 2u)) && !buttons_ducking()) {
            g->jump_phase = 0u;
        }
    }

    /* While duck is held, clamp jump phase so player stays on ground. */
    if (buttons_ducking() && (g->jump_phase < JUMP_LENGTH)) {
        g->jump_phase = JUMP_LENGTH;
    }

    /* ---- Draw player + clear trail ---- */
    draw_player(g);

    /* ---- Hill logic ---- */
    bool loop_breaker = true;

    if (g->hill_x < 16) {
        /* If crow is overtaking the hill, push hill back (original behaviour). */
        if (g->crow_x < g->hill_x) {
            g->hill_x   += 8;
            loop_breaker = false;
        }
        if (loop_breaker) {
            Lcd_SetCursor((uint8_t)g->hill_x, 1u);
            Lcd_WriteChar(SPRITE_HILL);
        }
    }

    if (g->hill_x < 1) {
        if (g->player_y) {
            /* Jumped over the hill — award a point and respawn. */
            g->score++;
            g->hill_x = (int8_t)(HILL_RESPAWN_BASE
                                  + rng_range(0u, HILL_RESPAWN_RAND));
        } else {
            /* Collision — hill reached player column while not jumping. */
            return true;
        }
    }

    /* ---- Crow logic ---- */
    if (g->crow_x < 16) {
        Lcd_SetCursor((uint8_t)g->crow_x, 0u);
        /* Animate between crow1 and crow2 every 4 ticks (original: tick%8<4). */
        uint8_t crow_sprite = ((g->game_tick % 8u) < 4u)
                              ? SPRITE_CROW1 : SPRITE_CROW2;
        Lcd_WriteChar(crow_sprite);
    }

    if (g->crow_x < 1) {
        if (buttons_ducking()) {
            /* Ducked under the crow — award a point and respawn. */
            g->score++;
            g->crow_x = (int8_t)(CROW_RESPAWN_BASE
                                  + rng_range(0u, CROW_RESPAWN_RAND));
        } else {
            /* Collision — crow reached player column while not ducking. */
            return true;
        }
    }

    /* ---- Erase old player position (the row the player just left) ---- */
    /* Original: lcd.setCursor(0, playerY); lcd.print(" ");
     * player_y is already updated at top of tick to reflect current state,
     * so we erase the opposite row — where the player was last tick. */
    Lcd_SetCursor(0u, g->player_y ? 1u : 0u);
    Lcd_WriteChar(' ');

    /* ---- Advance state ---- */
    g->jump_phase++;
    g->hill_x--;
    g->crow_go  = !g->crow_go;
    g->crow_x  -= (int8_t)g->crow_go;   /* crow moves every other tick */
    g->game_tick++;

    return false;   /* no collision */
}

void game_end(game_t *g, uint16_t current_hi, uint16_t *new_hi_out)
{
    /* Play the death sound (blocking in hardware, non-blocking for CPU). */
    buzzer_tone_nb(DIE_PITCH, DIE_MS);

    /* Update EEPROM high score if beaten. */
    if (g->score > current_hi) {
        high_score_save(g->score);
        *new_hi_out = g->score;
    } else {
        *new_hi_out = current_hi;
    }
}
