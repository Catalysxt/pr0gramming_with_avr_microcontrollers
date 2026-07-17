/*
 * slots.h
 *
 * Slot machine game interface — 3 reels, 5 symbols, animated stops.
 * BTN_A: SPIN (costs bet).
 * BTN_B: cycle bet (1 / 5 / 10 / 25).
 */

#ifndef SLOTS_H
#define SLOTS_H

#include "buttons.h"

void slots_enter(void);
void slots_update(btn_event_t e);
void slots_exit(void);

#endif /* SLOTS_H */
