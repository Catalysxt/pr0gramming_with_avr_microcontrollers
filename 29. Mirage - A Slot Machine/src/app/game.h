#ifndef SLOT_MACHINE_GAME_H_
#define SLOT_MACHINE_GAME_H_

#include <stdint.h>
#include "buttons.h"   /* button_event_t */

/* Slot-machine application: the finite-state machine, reel logic, payout, audio
 * cueing, and settings menu. The module owns all game state and drives the
 * display drivers (seg7, dotmatrix) and the buzzer; main.c only feeds it time +
 * button events and then streams the display buffers to the chain.
 *
 * States: ATTRACT, READY, SPINNING, EVALUATE, JACKPOT, SETTINGS. Every
 * transition passes through a state_transition(from,to) seam (a no-op today,
 * reserved for future UART debug). See game.c for the payout table and tuning. */

/* Initialize state (balance, reels, PRNG boot entropy) and paint the first
 * frame. Call once before the main loop, after the display + buzzer are up. */
void game_init(void);

/* Advance the game one step. Call every millisecond with the current tick and
 * the debounced events from the two buttons (BUTTON_NONE when idle). Renders
 * into the seg7 / dotmatrix buffers and issues buzzer sequences; it never
 * blocks. */
void game_update(uint32_t now_ms, button_event_t ev_a, button_event_t ev_b);

#endif /* SLOT_MACHINE_GAME_H_ */
