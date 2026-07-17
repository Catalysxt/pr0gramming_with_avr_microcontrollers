/*
 * hardware_connections.h
 *
 * Single source of truth for every pin used by digital_dash.
 * Header-only — no .c companion.
 *
 * LCD pins (RS=PB0, RW=PB1, EN=PB2, D4-D7=PD4-PD7) are owned by
 * lcd_driver.h and listed here only for cross-reference.
 *
 * ATmega328P datasheet §1 (pinout), §14 (I/O ports)
 */

#ifndef DIGITAL_DASH_HARDWARE_CONNECTIONS_H_
#define DIGITAL_DASH_HARDWARE_CONNECTIONS_H_

#include <avr/io.h>

/* ---- Buttons (PCINT1 group — PORTC) ------------------------------------ */
/* Both buttons are active-low with internal pull-ups.                       */
/* PC0 = PCINT8, PC1 = PCINT9 — both serviced by PCINT1_vect.               */

#define BTN_DDR     DDRC
#define BTN_PORT    PORTC
#define BTN_PINREG  PINC
#define JUMP_BIT    PC0     /* PCINT8 — falling edge = jump press  */
#define DUCK_BIT    PC1     /* PCINT9 — held low = ducking         */

/* ---- Buzzer (Timer2 OC2A output — PORTB) -------------------------------- */
/* Passive piezo driven by Timer2 CTC toggle on OC2A.                        */
/* ATmega328P datasheet §17 (Timer2), §17.6.2 (OC2A = PB3).                 */

#define BUZZER_DDR  DDRB
#define BUZZER_BIT  PB3

/* ---- RNG seed ADC ------------------------------------------------------- */
/* PC2/ADC2 left physically unconnected — floating node provides thermal     */
/* noise read once at boot to seed the xorshift PRNG.                        */
/* PC0 and PC1 cannot serve as the seed source because their pull-ups        */
/* clamp the voltage, yielding a near-constant reading.                       */

#define RNG_ADC_CH  2u      /* ADMUX MUX[3:0] = 0b0010 → ADC2 (PC2)  */

/* ---- LCD (documentation only — managed entirely by lcd_driver.h) -------- */
/* RS  = PB0   RW  = PB1   EN  = PB2                                         */
/* D4  = PD4   D5  = PD5   D6  = PD6   D7  = PD7                            */

#endif /* DIGITAL_DASH_HARDWARE_CONNECTIONS_H_ */
