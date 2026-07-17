/*
 * game.h
 *
 * Game state, tuning constants, and the public API for one game session.
 */

#ifndef DIGITAL_DASH_GAME_H_
#define DIGITAL_DASH_GAME_H_

#include <stdint.h>
#include <stdbool.h>

/* ---- Tuning constants --------------------------------------------------- */
/* Changing these is the primary way to adjust difficulty. */

#define TICKSPEED_MS   90u   /* ms between game logic updates              */
#define JUMP_LENGTH     3u   /* number of ticks the player stays airborne  */

/* Audio frequencies and durations (Hz / ms). */
#define JUMP_PITCH   2700u
#define JUMP_MS        50u
#define DUCK_PITCH   1350u
#define DUCK_MS        50u
#define DIE_PITCH     200u
#define DIE_MS        500u

/* ---- Game state --------------------------------------------------------- */

typedef struct {
    uint8_t  jump_phase;   /* 0..(JUMP_LENGTH-1) = airborne; JUMP_LENGTH+1 = grounded */
    uint16_t game_tick;    /* total ticks since game start; drives crow animation      */
    int8_t   crow_x;       /* column of crow (can be > 15 = off-screen right)          */
    int8_t   hill_x;       /* column of hill                                           */
    bool     player_y;     /* false = row 1 (ground); true = row 0 (jumping)           */
    bool     crow_go;      /* half-speed crow flip-flop: crow moves only when true      */
    uint16_t score;        /* obstacles successfully cleared this session               */
} game_t;

/* ---- Public API --------------------------------------------------------- */

/* Reset all fields to their start-of-game values. */
void game_init(game_t *g);

/*
 * Advance game logic by one tick.
 * Returns true if a collision was detected (game over).
 * Caller is responsible for calling game_end() on true.
 */
bool game_tick_update(game_t *g);

/*
 * Play the death sound and update the high score if beaten.
 * current_hi : high score loaded at startup.
 * new_hi_out : written with the new high score (may equal current_hi).
 */
void game_end(game_t *g, uint16_t current_hi, uint16_t *new_hi_out);

#endif /* DIGITAL_DASH_GAME_H_ */
