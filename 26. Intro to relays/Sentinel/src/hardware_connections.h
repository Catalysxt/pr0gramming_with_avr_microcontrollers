#ifndef SENTINEL_HARDWARE_CONNECTIONS_H_
#define SENTINEL_HARDWARE_CONNECTIONS_H_

/*
 * Single source of truth for every pin_t this project uses. Modules receive
 * pin_t values from here as arguments — they never reference DDRx/PORTx/PINx
 * directly, so re-wiring a peripheral only touches this file.
 *
 * Initializer order matches pin_t: { &DDRx, &PORTx, &PINx, bit }.
 */

#include <avr/io.h>   /* DDRB/PORTB/PINB, DDRD/PORTD/PIND, PB0, PD2 */
#include "pin.h"      /* pin_t */

// ---- Relay drive (PB0 -> relay module IN -> 12 V LED load) ----
// Same pin as the relay/hello_world base, but here PB0 drives an active-HIGH
// relay *module* directly (the module carries its own coil driver + flyback).
// Logic HIGH energises the relay; no ULN2803 in the path (see docs/Theory.md).
static const pin_t RELAY = { &DDRB, &PORTB, &PINB, PB0 };

// ---- Mode button (tactile, active-low with internal pull-up) ----
// Idle = HIGH (pull-up). Pressed = LOW (button shorts the pin to GND).
static const pin_t BUTTON = { &DDRD, &PORTD, &PIND, PD2 };

#endif /* SENTINEL_HARDWARE_CONNECTIONS_H_ */
