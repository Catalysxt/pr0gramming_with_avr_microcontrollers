#ifndef EMBER_HARDWARE_CONNECTIONS_H_
#define EMBER_HARDWARE_CONNECTIONS_H_

/*
 * Single source of truth for every pin_t in Ember. Modules receive pin_t
 * values from here as arguments — they never reference DDRx/PORTx/PINx
 * directly, so re-wiring a peripheral only touches this file.
 *
 * Initializer order matches pin_t: { &DDRx, &PORTx, &PINx, bit }.
 *
 * Pin budget (16 MHz crystal makes PB6/PB7 = XTAL unavailable; PD0/PD1
 * reserved for the UART dashboard stretch): 17 GPIO of 18 usable.
 *
 *   Relay          PB0
 *   Buzzer (OC1A)  PB1
 *   Digit 1..4     PB2 PB3 PB4 PB5   (NPN low-side, active-HIGH select)
 *   Seg a..f       PC0 PC1 PC2 PC3 PC4 PC5
 *   Seg g, DP      PD5 PD6
 *   Encoder CLK/DT/SW  PD2 PD3 PD4   (PCINT2 group -> power-down wake)
 */

#include <avr/io.h>   /* DDRx/PORTx/PINx, PBn/PCn/PDn */
#include "pin.h"      /* pin_t */

// ---- Relay drive (PB0 -> relay module IN -> AC desk lamp, load side) ----
// Active-HIGH module (carries its own coil driver + flyback). Logic HIGH
// energises the relay. Lamp ON == "take a break". MAINS: load side wires
// active -> NO only; never GND -> COM (see docs/Theory.md).
static const pin_t RELAY = { &DDRB, &PORTB, &PINB, PB0 };

// ---- Passive piezo buzzer (Timer1 CTC toggle on OC1A) ----
// MUST be PB1: OC1A is the only pin Timer1 can toggle in hardware.
static const pin_t BUZZER = { &DDRB, &PORTB, &PINB, PB1 };

// ---- 4-digit 7-seg display: Kingbright CC56-12SRWA (COMMON CATHODE) ----
// (Bench-verified common-cathode: shorting a digit common to GND lit all its
//  segments. The "CA" family prefix means common-anode; this is the "CC" part.)
// To light a segment: digit common LOW (sunk to GND), segment anode HIGH.
// Digit selects drive NPN LOW-SIDE switches -> active HIGH turns a digit ON.
#define EMBER_DIGIT_ACTIVE_LOW  0
// Segment anodes are sourced by the MCU through series resistors -> HIGH = lit.
#define EMBER_SEG_ACTIVE_LOW    0

// Segment order is the font-table bit order: bit0=a .. bit6=g, bit7=DP.
static const pin_t SEG[8] = {
    { &DDRC, &PORTC, &PINC, PC0 },  // a
    { &DDRC, &PORTC, &PINC, PC1 },  // b
    { &DDRC, &PORTC, &PINC, PC2 },  // c
    { &DDRC, &PORTC, &PINC, PC3 },  // d
    { &DDRC, &PORTC, &PINC, PC4 },  // e
    { &DDRC, &PORTC, &PINC, PC5 },  // f
    { &DDRD, &PORTD, &PIND, PD5 },  // g
    { &DDRD, &PORTD, &PIND, PD6 },  // DP
};

// Digit anodes 1..4 (leftmost = DIGIT[0]).
static const pin_t DIGIT[4] = {
    { &DDRB, &PORTB, &PINB, PB2 },  // Dig1 (datasheet pin 12)
    { &DDRB, &PORTB, &PINB, PB3 },  // Dig2 (datasheet pin 9)
    { &DDRB, &PORTB, &PINB, PB4 },  // Dig3 (datasheet pin 8)
    { &DDRB, &PORTB, &PINB, PB5 },  // Dig4 (datasheet pin 6)
};

// ---- KY-040 rotary encoder (active-low quadrature + push-button) ----
// CLK=A, DT=B on PD2/PD3; SW (push) on PD4. All three share PCINT2 so a
// twist or press wakes the MCU from SLEEP_MODE_PWR_DOWN. Internal pull-ups
// used; the KY-040 board also carries 10k pull-ups on CLK/DT.
static const pin_t ENC_CLK = { &DDRD, &PORTD, &PIND, PD2 };  // PCINT18
static const pin_t ENC_DT  = { &DDRD, &PORTD, &PIND, PD3 };  // PCINT19
static const pin_t ENC_SW  = { &DDRD, &PORTD, &PIND, PD4 };  // PCINT20

// Mask of the encoder bits within PORTD / PCMSK2 (they align: PCINT(16+n)=PDn).
#define EMBER_ENC_PCINT_MASK  ((1u << PCINT18) | (1u << PCINT19) | (1u << PCINT20))

#endif /* EMBER_HARDWARE_CONNECTIONS_H_ */
