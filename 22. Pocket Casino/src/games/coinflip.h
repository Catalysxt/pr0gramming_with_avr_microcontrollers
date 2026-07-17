/*
 * coinflip.h
 *
 * Coin Flip game interface.
 * BTN_B toggles the player's call (HEADS / TAILS).
 * BTN_A flips the coin — 600 ms animation with rising-pitch ticks, then reveal.
 * Fixed bet: 10 credits. Win pays +20 credits (net +10).
 */

#ifndef COINFLIP_H
#define COINFLIP_H

#include "buttons.h"

void coinflip_enter(void);
void coinflip_update(btn_event_t e);
void coinflip_exit(void);

#endif /* COINFLIP_H */
