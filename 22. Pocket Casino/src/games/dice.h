/*
 * dice.h
 *
 * Dice game interface — Yahtzee-lite with 2 dice.
 * BTN_A: ROLL (costs 5 credits).
 * BTN_B: cycle target (DOUBLES / OVER 7 / UNDER 7 / EXACT 7).
 */

#ifndef DICE_H
#define DICE_H

#include "buttons.h"

void dice_enter(void);
void dice_update(btn_event_t e);
void dice_exit(void);

#endif /* DICE_H */
