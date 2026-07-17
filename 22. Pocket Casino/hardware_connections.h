/*
 * hardware_connections.h
 *
 * Single source of truth for all physical pin assignments.
 * Every other module #includes this file — never hardcode pins elsewhere.
 *
 * Target: ATmega328P @ 16 MHz (external crystal)
 *
 * ATmega328P datasheet: Section 14 (I/O Ports), Table 14-1 pin functions
 */

#ifndef HARDWARE_CONNECTIONS_H
#define HARDWARE_CONNECTIONS_H

#include <avr/io.h>

/* ==========================================================================
 * LCD — Vishay LCD-016N002B, ST7066 (HD44780-compatible), 4-bit mode
 * Pins are hardcoded in lcd_driver.h; listed here for reference only.
 *
 * ATmega328P datasheet Section 14.4 (PORTB register descriptions)
 * ========================================================================== */
/* RS → PB0 | R/W → PB1 | E → PB2 | D4-D7 → PD4-PD7  (set by lcd_driver.h) */

/* ==========================================================================
 * Buzzer — passive piezo, driven by Timer2 CTC toggle on OC2A
 *
 * OC2A is the Timer2 Compare Match A output pin (ATmega328P datasheet
 * Section 17.5.1, Table 17-7). PB3 = OC2A.
 *
 * NOTE: OC1A (Timer1) is PB1, which the LCD driver already drives as R/W.
 * Timer1 is therefore unavailable for hardware waveform output; Timer2 is
 * used instead. Timer2 is 8-bit; prescaler is chosen dynamically per tone.
 * ========================================================================== */
#define BUZZER_DDR      DDRB
#define BUZZER_PORT     PORTB
#define BUZZER_PIN      PB3     /* OC2A — Timer2 Compare Match A output */

/* ==========================================================================
 * Buttons — active-low, internal pull-ups enabled
 *
 * Both buttons are on PORTC, handled by PCINT1_vect (Pin Change Interrupt 1).
 * ATmega328P datasheet Section 13.2.4 (PCINT[23:16]).
 * PC0 = PCINT8, PC1 = PCINT9.  Both belong to the PCINT1 group (PCICR.PCIE1).
 *
 * "INT8/INT9" in the project spec refer to these PCINT numbers, not external
 * interrupt vectors (which only exist for PD2/INT0 and PD3/INT1 on this MCU).
 * ========================================================================== */
#define BTN_DDR         DDRC
#define BTN_PORT        PORTC
#define BTN_PIN_REG     PINC

#define BTN_A_PIN       PC0     /* ACTION / SELECT / SPIN / ROLL — PCINT8  */
#define BTN_B_PIN       PC1     /* NEXT / BET / CHANGE          — PCINT9  */

/* PCINT1 group mask bits (ATmega328P datasheet Section 13.2.7, PCMSK1) */
#define BTN_A_PCINT_BIT PCINT8
#define BTN_B_PCINT_BIT PCINT9

#endif /* HARDWARE_CONNECTIONS_H */
