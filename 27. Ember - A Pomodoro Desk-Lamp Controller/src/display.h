/*
 * display.h
 *
 * Non-blocking driver for the Kingbright CC56-12SRWA 4-digit common-cathode
 * 7-segment display. A Timer2 CTC ISR multiplexes the four digits at ~250 Hz
 * (one digit lit per ~1 ms), reading a 4-byte framebuffer that the main
 * context updates via display_show_*. Nothing here blocks.
 *
 * Wiring & polarity live in hardware_connections.h (SEG[8], DIGIT[4],
 * EMBER_SEG_ACTIVE_LOW, EMBER_DIGIT_ACTIVE_LOW).
 *
 * ATmega328P datasheet §17 (Timer2/Counter2)
 */

#ifndef EMBER_DISPLAY_H_
#define EMBER_DISPLAY_H_

#include <stdint.h>
#include <stdbool.h>

/* Drive all segment/digit pins as outputs (blanked) and start the Timer2
 * multiplex ISR. Call once, before sei(). */
void display_init(void);

/* Show a countdown as MM:SS. min/sec are clamped to 99/59. The colon is the
 * decimal point between the MM and SS pair; use display_set_colon() to blink
 * it. Written from main context; the ISR picks it up on its next sweep. */
void display_show_mmss(uint8_t min, uint8_t sec);

/* Show an unsigned value right-aligned across the 4 digits (leading blanks).
 * Used by the SET_WORK / SET_BREAK screens to show the chosen minutes. */
void display_show_uint(uint16_t value);

/* Turn on/off the colon (the DP between the two digit pairs). */
void display_set_colon(bool on);

/* Blank every digit (segments off). The ISR keeps running but lights nothing
 * — used before entering sleep, and in IDLE. */
void display_blank(void);

/* Stop the Timer2 multiplex ISR and drive all lines to their off state.
 * Called by the sleep path so the display draws no current in power-down. */
void display_stop(void);

/* Restart the multiplex ISR after display_stop() (post-wake). */
void display_resume(void);

#endif /* EMBER_DISPLAY_H_ */
