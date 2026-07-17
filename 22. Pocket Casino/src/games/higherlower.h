/*
 * higherlower.h
 *
 * Higher or Lower game interface.
 * BTN_A: guess HIGHER.
 * BTN_B: guess LOWER.
 * Equal number always loses.
 * +10 credits per correct guess; streak 10 triggers jackpot SFX.
 */

#ifndef HIGHERLOWER_H
#define HIGHERLOWER_H

#include "buttons.h"

void higherlower_enter(void);
void higherlower_update(btn_event_t e);
void higherlower_exit(void);

#endif /* HIGHERLOWER_H */
