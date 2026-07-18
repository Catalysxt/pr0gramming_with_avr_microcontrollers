/* Slot machine -- ATmega328P firmware.
 *
 * Three reels on a 4-panel 8x8 dot-matrix, a two 4-digit 7-segment score readout,
 * a buzzer, and two buttons. All display chips hang off ONE SPI daisy chain that
 * shares MOSI/SCK and a single /LOAD (CS); the firmware streams N cascaded frames
 * per refresh. The application logic is an explicit FSM in app/game.c; this file
 * only wires peripherals together and runs a non-blocking main loop.
 *
 * ---- Pin map (ATmega328P @ 16 MHz, see hardware_connections.h + HARDWARE.md) ----
 *   PB3  MOSI/DIN   -> chain data   (hardware SPI)
 *   PB5  SCK/CLK    -> chain clock  (hardware SPI)
 *   PB2  SS /LOAD   -> chain CS      (shared latch, driven as GPIO)
 *   PB1  OC1A       -> buzzer       (Timer1 CTC toggle)
 *   PD6  BTN_A      -> SPIN / SELECT (active-low, pull-up)
 *   PD7  BTN_B      -> MENU / BACK; long-press = SETTINGS
 *   PC0  ADC0       -> floating, PRNG seed source
 *
 * ---- Chain layout (device 0 = nearest the MCU's DIN) ----
 *   dev 0     : score MAX7221  (8 x 7-seg digits)
 *   dev 1..4  : dot-matrix module's 4 internal MAX7219s (panels 0..3)
 *
 * ---- Timers ----
 *   Timer0 : 1 ms system tick (timing.c) -- paces buttons, animation, audio.
 *   Timer1 : buzzer square-wave tone (buzzer.c, CTC toggle on OC1A).
 *   Timer2 : unused.
 */

#include <avr/io.h>
#include <avr/interrupt.h>          /* sei() */
#include <stdint.h>

#include "pin.h"
#include "max7219.h"
#include "hardware_connections.h"   /* MAX7219_CS, BTN_A, BTN_B */
#include "timing.h"
#include "buttons.h"
#include "buzzer.h"
#include "seg7.h"
#include "dotmatrix.h"
#include "game.h"

/* One SPI chain: the score chip (device 0) followed by the 4 dot-matrix panels. */
#define CHAIN_DEVICES   (1u + DOT_PANELS)   /* 1 score + 4 matrix = 5 */
#define DISPLAY_INTENSITY   5u              /* 0..15 PWM brightness */
#define DISPLAY_PERIOD_MS   15u             /* ~66 Hz refresh -- flicker-free */

static const max7219_chain_t s_chain = { MAX7219_CS, CHAIN_DEVICES };

static button_t s_btn_a;
static button_t s_btn_b;

/* Stream the display buffers to the chain. Both the 7-seg digits and the matrix
 * rows live in the chips' 8 DIGIT registers, so one loop over those registers
 * refreshes everything: for register `row`, device 0 gets the score chip's digit
 * byte and devices 1..4 get panel 0..3's row byte, all latched by one CS pulse. */
static void display_refresh(void) {
    const uint8_t *score = seg7_rows();
    for (uint8_t row = 0; row < 8u; row++) {
        uint8_t data[CHAIN_DEVICES];
        data[0] = score[row];
        for (uint8_t p = 0; p < DOT_PANELS; p++) {
            data[1u + p] = dotmatrix_rows(p)[row];
        }
        max7219_chain_write(s_chain, (uint8_t)(MAX7219_REG_DIGIT0 + row), data);
    }
}

int main(void) {
    max7219_chain_init(s_chain);
    max7219_chain_set_intensity(s_chain, DISPLAY_INTENSITY);
    buzzer_init();

    button_init(&s_btn_a, BTN_A);
    button_init(&s_btn_b, BTN_B);

    game_init();                     /* seeds PRNG from ADC noise, paints attract */

    timing_init();
    sei();                           /* start the 1 ms tick */

    uint32_t last_sample_ms  = 0;
    uint32_t last_refresh_ms = 0;

    for (;;) {
        uint32_t now = timing_ms();

        /* Fixed 1 ms cadence: debounce timing and all game timers depend on it. */
        if (now != last_sample_ms) {
            last_sample_ms = now;
            button_event_t ev_a = button_poll(&s_btn_a);
            button_event_t ev_b = button_poll(&s_btn_b);
            game_update(now, ev_a, ev_b);
            buzzer_tick(now);
        }

        /* Refresh the display at a steady rate rather than every loop, to keep
         * SPI traffic bounded and the brightness even. */
        if ((now - last_refresh_ms) >= DISPLAY_PERIOD_MS) {
            last_refresh_ms = now;
            display_refresh();
        }
    }

    return 0;
}
