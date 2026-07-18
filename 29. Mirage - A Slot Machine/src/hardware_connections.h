#ifndef SLOT_MACHINE_HARDWARE_CONNECTIONS_H_
#define SLOT_MACHINE_HARDWARE_CONNECTIONS_H_

#include <avr/io.h>
#include "pin.h"

/* Single source of truth for this project's physical pin assignments (header
 * only, per project convention: drivers take pin_t values and never reference
 * PORT/DDR macros directly). Reuses the stopwatch wiring and adds the buzzer and
 * dot-matrix module. Full pin table + wiring diff live in HARDWARE.md.
 *
 *   Fn                MCU pin  Peripheral / notes
 *   ----------------  -------  ---------------------------------------------
 *   SPI MOSI (DIN)    PB3      hardware SPI, shared down the whole chain
 *   SPI SCK  (CLK)    PB5      hardware SPI, shared clock
 *   SPI /LOAD (CS)    PB2      shared latch for score chip + matrix module
 *   Buzzer            PB1      OC1A, Timer1 CTC toggle (fixed HW pin)
 *   BTN_A (SPIN/SEL)  PD6      active-low + internal pull-up
 *   BTN_B (MENU/BACK) PD7      active-low + internal pull-up, long-press
 *   PRNG seed         PC0/ADC0 left floating; sampled for entropy at boot
 */

/* ---- MAX7219/MAX7221 chain -- shared SPI /LOAD (CS) ----
 *
 * One SPI daisy chain carries both display subsystems, sharing MOSI (PB3), SCK
 * (PB5) and this one active-low CS (PB2):
 *   device 0        = score MAX7221 (two 4-digit 7-seg displays)
 *   devices 1..4    = the 4 internal MAX7219s of the dot-matrix module
 * The refresh in main.c sends 5 cascaded frames per register. PB2 must stay an
 * output while the SPI unit is master (else MSTR auto-clears and it goes silent
 * -- ATmega328P datasheet, master-mode SS note); as our CS it always is. */
static const pin_t MAX7219_CS = { &DDRB, &PORTB, &PINB, PB2 };

/* ---- UI pushbuttons (active-low, internal pull-up) ----
 *
 * Reused from the stopwatch (PD6/PD7). Each shorts its pin to GND when pressed;
 * the firmware enables the internal pull-up, so idle = HIGH, pressed = LOW -- no
 * external resistors. Debounce + long-press are handled in buttons.c.
 *   BTN_A: SPIN / SELECT / confirm
 *   BTN_B: MENU / BACK; long-press (~1 s) enters SETTINGS */
static const pin_t BTN_A = { &DDRD, &PORTD, &PIND, PD6 };
static const pin_t BTN_B = { &DDRD, &PORTD, &PIND, PD7 };

/* ---- Buzzer (documentary) ----
 * The tone engine toggles OC1A in hardware, which is fixed to PB1 on the
 * ATmega328P, so buzzer.c owns that pin directly rather than via a pin_t. Listed
 * here only so this file stays the complete pin map.
 *
 * ---- PRNG entropy (documentary) ----
 * ADC0 / PC0 is left unconnected (floating) and sampled at boot for a noisy
 * seed; game.c drives the ADC directly. Do not wire anything to PC0. */

#endif /* SLOT_MACHINE_HARDWARE_CONNECTIONS_H_ */
