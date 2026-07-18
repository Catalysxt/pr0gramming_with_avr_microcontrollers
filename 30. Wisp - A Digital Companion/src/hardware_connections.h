#ifndef DIGITAL_COMPANION_HARDWARE_CONNECTIONS_H_
#define DIGITAL_COMPANION_HARDWARE_CONNECTIONS_H_

#include <avr/io.h>
#include "pin.h"

/* Single source of truth for this project's physical pin assignments (header
 * only, per project convention: drivers take pin_t values and never reference
 * PORT/DDR macros directly). Full pin table + wiring diff vs. the stopwatch
 * live in HARDWARE.md.
 *
 *   Fn                MCU pin   Peripheral / notes
 *   ----------------  --------  --------------------------------------------
 *   SPI MOSI (DIN)    PB3       hardware SPI to MAX7221 DIN
 *   SPI SCK  (CLK)    PB5       hardware SPI clock
 *   SPI /LOAD (CS)    PB2       MAX7221 latch (own CS line, active-low)
 *   HC-SR04 ECHO      PB0/ICP1  Timer1 input capture (echo pulse width)
 *   HC-SR04 TRIG      PD4       10 us trigger pulse (plain GPIO)
 *   Pet button        PB1/PCINT1 active-low + internal pull-up
 *   PRNG seed         PC0/ADC0  left floating; sampled for entropy at boot
 *   Mic (Tier 3)      PC1/ADC1  electret amp; scaffold only (ENABLE_AUDIO)
 *   Loop scope        PD3       toggled top/bottom of main loop (scope proof)
 *   Debug TXD         PD1       USART TX, only when DEBUG_UART is defined
 */

/* ---- MAX7221 8x8 dot-matrix -- own SPI /LOAD (CS) ----
 *
 * A single MAX7221 drives the 8x8 face over the shared hardware-SPI unit
 * (MOSI=PB3, SCK=PB5). It gets its own active-low CS on PB2 so future daisy-
 * chained devices can share MOSI/SCK. PB2 must stay an OUTPUT while the SPI
 * unit is master (else a low on it auto-clears MSTR and the master goes silent
 * -- ATmega328P datasheet, master-mode SS note); as our CS it always is. */
static const pin_t MAX7221_CS = { &DDRB, &PORTB, &PINB, PB2 };

/* ---- Pet button (active-low, internal pull-up) ----
 *
 * Shorts PB1 to GND when pressed; the firmware enables the internal pull-up so
 * idle = HIGH, pressed = LOW -- no external resistor. Debounce + long-press are
 * handled in buttons.c. PB1 is also PCINT1: the pin-change ISR latches TCNT1 on
 * the first press to fold user timing into the PRNG seed (see prng.c). */
static const pin_t PET_BUTTON = { &DDRB, &PORTB, &PINB, PB1 };

/* ---- HC-SR04 ultrasonic ranger ----
 *
 * TRIG is a plain GPIO output pulsed HIGH for 10 us to start a ping. ECHO is
 * wired to PB0/ICP1 so Timer1's input-capture unit timestamps both edges of the
 * return pulse in hardware -- no pulseIn-style busy loop. ECHO idles LOW and is
 * a 5 V logic output; safe straight into PB0 at Vcc = 5 V (see HARDWARE.md for
 * the 3.3 V divider caveat). */
static const pin_t HCSR04_TRIG = { &DDRD, &PORTD, &PIND, PD4 };
/* ECHO/PB0 is owned by the Timer1 ICP1 hardware in hcsr04.c, not via pin_t. */

/* ---- Loop-scope test point ----
 *
 * Toggled at the top and bottom of the main loop so an oscilloscope on PD3 can
 * confirm the loop's period is a stable 1/TICK_HZ with no long stuck-low
 * segments when expressions change (README "Scope + timing verification"). */
static const pin_t LOOP_SCOPE = { &DDRD, &PORTD, &PIND, PD3 };

/* ---- Documentary-only pins (driven directly by their peripheral) ----
 * PC0/ADC0 : floating PRNG entropy source -- do not wire anything to it.
 * PC1/ADC1 : electret mic for the Tier-3 audio scaffold (ENABLE_AUDIO).
 * PD1      : USART TXD, only initialised behind DEBUG_UART.
 * PB0/ICP1 : HC-SR04 ECHO, owned by Timer1 input capture in hcsr04.c. */

#endif /* DIGITAL_COMPANION_HARDWARE_CONNECTIONS_H_ */
