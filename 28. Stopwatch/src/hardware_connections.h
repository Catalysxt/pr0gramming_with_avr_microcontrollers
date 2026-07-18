#ifndef MAX7219_HARDWARE_CONNECTIONS_H_
#define MAX7219_HARDWARE_CONNECTIONS_H_

#include <avr/io.h>
#include "pin.h"

/* Single source of truth for this project's physical pin assignments.
   Header-only, per the project convention: drivers receive pin_t values and
   never reference PORT/DDR macros directly. */

/* ---- MAX7219 (hardware SPI) ----
 *
 * The ATmega328P hardware SPI unit owns three fixed pins; the MAX7219 needs
 * two of them plus a chip-select we toggle by hand:
 *
 *   MOSI = PB3  -> MAX7219 DIN   (serial data in)   [fixed by hardware SPI]
 *   SCK  = PB5  -> MAX7219 CLK   (serial clock)      [fixed by hardware SPI]
 *   SS   = PB2  -> MAX7219 LOAD  (CS / latch)        [we drive it as GPIO]
 *
 * MISO (PB4) is unused: the MAX7219 has no data-out line to read back.
 *
 * PB2 MUST remain an output while the SPI hardware is in master mode. If it
 * were ever left as an input and pulled low, the SPI unit auto-clears the MSTR
 * bit and the master goes silent (ATmega328P datasheet, "SS Pin Functionality
 * / Master Mode"). We use PB2 as our active-low CS, so it is always an output
 * -- one pin, both jobs. */
static const pin_t MAX7219_CS = { &DDRB, &PORTB, &PINB, PB2 };

/* ---- Stopwatch pushbuttons (active-low, internal pull-up) ----
 *
 *   PD6 -> START/PAUSE button   PD7 -> CLEAR button
 *
 * Each button shorts its pin to GND when pressed; we enable the ATmega's
 * internal pull-up so an idle (released) button reads HIGH and a press reads
 * LOW -- no external resistors. PD6/PD7 are free: the hardware SPI unit owns
 * PB2/PB3/PB5, and PD0/PD1 (USART) are unused now that serial is dropped. */
static const pin_t BTN_STARTPAUSE = { &DDRD, &PORTD, &PIND, PD6 };
static const pin_t BTN_CLEAR      = { &DDRD, &PORTD, &PIND, PD7 };

#endif /* MAX7219_HARDWARE_CONNECTIONS_H_ */
