/*
 * sys_tick.h
 *
 * 1 ms system tick via Timer0 CTC. Every other ANVIL module times itself
 * off sys_millis() instead of blocking _delay_* calls, so this must be
 * initialised before anything else that needs timing.
 *
 * ATmega328P datasheet §15 (Timer0/Counter0)
 */

#ifndef ANVIL_SYS_TICK_H_
#define ANVIL_SYS_TICK_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Configure Timer0 for 1 kHz CTC and enable its compare-match interrupt.
 * Call once before sei().
 */
void sys_tick_init(void);

/*
 * Return the current millisecond count since sys_tick_init().
 * Safe to call from main context — uses ATOMIC_BLOCK internally.
 */
uint32_t sys_millis(void);

/*
 * Non-blocking interval check.
 * Returns true (and advances *anchor by period_ms) each time period_ms
 * milliseconds have elapsed since the previous trigger.
 * Initialise *anchor to sys_millis() before the first call.
 */
bool sys_elapsed(uint32_t *anchor, uint32_t period_ms);

#endif /* ANVIL_SYS_TICK_H_ */
