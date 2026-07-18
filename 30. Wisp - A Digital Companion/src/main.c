/* Digital Companion -- ATmega328P firmware.
 *
 * An expressive 8x8 dot-matrix face (MAX7221 over SPI) whose emotion is CAUSED
 * by sensors, not scheduled: an HC-SR04 ranger and a pet button feed a numeric
 * (valence, arousal) mood model; an FSM reads that point and picks an
 * expression; the animator tweens there and adds idle micro-motion. This file
 * only wires peripherals and runs a non-blocking, tick-paced main loop.
 *
 * ---- Pin map (ATmega328P @ 16 MHz; see hardware_connections.h + HARDWARE.md) ----
 *   PB3  MOSI/DIN   -> MAX7221 data    (hardware SPI)
 *   PB5  SCK/CLK    -> MAX7221 clock   (hardware SPI)
 *   PB2  SS /LOAD   -> MAX7221 CS       (driven as GPIO)
 *   PB0  ICP1       -> HC-SR04 ECHO    (Timer1 input capture)
 *   PD4  GPIO       -> HC-SR04 TRIG    (10 us ping pulse)
 *   PB1  PCINT1     -> pet button       (active-low, pull-up; seeds PRNG)
 *   PC0  ADC0       -> floating, PRNG entropy source
 *   PC1  ADC1       -> mic (Tier-3 scaffold, ENABLE_AUDIO)
 *   PD3  GPIO       -> loop-scope test point
 *   PD1  TXD        -> debug UART (DEBUG_UART only)
 *
 * ---- Timers ----
 *   Timer0 : 1 ms system tick (timing.c) -- paces the whole loop.
 *   Timer1 : HC-SR04 echo input capture (hcsr04.c), /64 -> 4 us/tick.
 *   Timer2 : unused.
 */

#include <avr/io.h>
#include <avr/interrupt.h>          /* sei(), ISR() */
#include <stdint.h>
#include <stdbool.h>

#include "pin.h"
#include "max7219.h"
#include "hardware_connections.h"   /* MAX7221_CS, PET_BUTTON, HCSR04_TRIG, LOOP_SCOPE */

#include "timing.h"
#include "buttons.h"
#include "adc.h"
#include "hcsr04.h"
#ifdef DEBUG_UART
#include "USART.h"                  /* debug serial: cm log + FSM transitions */
#endif

#include "expression.h"
#include "prng.h"
#include "mood.h"
#include "fsm.h"
#include "sensors.h"
#include "face.h"
#include "animator.h"               /* ANIM_TICK_MS, REFRESH_MS */

/* One MAX7221 (a single-device chain) drives the 8x8 face. */
#define CHAIN_DEVICES     1u
#define DISPLAY_INTENSITY 6u        /* 0..15 PWM brightness */

/* Cadences (ms), all derived off the 1 ms tick. */
#define DECAY_MS 100u               /* mood decay + inactivity drift period */
#define PING_MS  100u               /* HC-SR04 re-trigger period (echo fits inside) */

static const max7219_chain_t s_chain = { MAX7221_CS, CHAIN_DEVICES };
static button_t s_pet;

/* PRNG seeding: PB1 pin-change latches Timer1's count on the first press so the
 * random sequence depends on real user timing, not just boot ADC noise. */
static volatile uint16_t s_seed_capture = 0;
static volatile bool     s_seed_ready   = false;

ISR(PCINT0_vect) {
    if (!s_seed_ready) {
        s_seed_capture = TCNT1;     /* free-running HC-SR04 timer = fine entropy */
        s_seed_ready   = true;
    }
}

static void companion_init(void) {
    /* ADC up first so we can draw boot entropy from the floating ADC0 pin, then
     * seed the PRNG before the animator (its idle schedule uses it). */
    adc_init();
    prng_seed(adc_sample_noise());

    face_init(s_chain, DISPLAY_INTENSITY);   /* SPI + MAX7221 + neutral frame */

#ifdef DEBUG_UART
    initUSART();                             /* PD1/TXD @ BAUD; wakes fsm.c logging too */
#endif

    button_init(&s_pet, PET_BUTTON);
    hcsr04_init(HCSR04_TRIG);
    pin_set_output(LOOP_SCOPE);

    /* Pin-change interrupt on PB1 (PCINT1) for the seed capture. */
    PCICR  |= (1 << PCIE0);
    PCMSK0 |= (1 << PCINT1);

    mood_init();
    fsm_init();
    sensors_init(0);

    timing_init();
    sei();                          /* start the 1 ms tick + capture/PC ISRs */
}

int main(void) {
    companion_init();

    uint32_t last_1ms     = 0;
    uint32_t last_decay   = 0;
    uint32_t last_ping    = 0;
    uint32_t last_anim    = 0;
    uint32_t last_refresh = 0;
    bool     seed_mixed   = false;

    for (;;) {
        pin_high(LOOP_SCOPE);       /* scope: loop start (see README timing proof) */
        uint32_t now = timing_ms();

        /* 1 ms cadence: debounce the pet button; fold in the seed on first press. */
        if (now != last_1ms) {
            last_1ms = now;
            if (button_poll(&s_pet) == BUTTON_SHORT) {
                sensors_pet(now);
            }
            if (s_seed_ready && !seed_mixed) {
                prng_add_entropy(s_seed_capture);
                seed_mixed = true;
            }
        }

        /* Ranging: fire a ping periodically; consume echoes whenever they land. */
        if ((now - last_ping) >= PING_MS) {
            last_ping = now;
            hcsr04_trigger(HCSR04_TRIG);
        }
        if (hcsr04_ready()) {
            uint16_t cm = hcsr04_distance_cm();
            sensors_proximity_update(cm, now);
#ifdef DEBUG_UART
            printString("cm=");
            if (cm == HCSR04_OUT_OF_RANGE) {
                printString("---");
            } else {
                printWord(cm);   /* shared USART: fixed 5-digit decimal */
            }
            transmitByte('\r');
            transmitByte('\n');
#endif
        }

        /* 100 ms: linear mood decay + inactivity drift (SAD/SLEEPY over time). */
        if ((now - last_decay) >= DECAY_MS) {
            last_decay = now;
            mood_decay();
            sensors_idle_tick(now);
        }

        /* Animation tick (25 Hz): FSM picks the target expression; animator
         * advances the tween + idle motion. */
        if ((now - last_anim) >= ANIM_TICK_MS) {
            last_anim = now;
            face_set_expression(fsm_step(mood_get()));
            face_tick();
        }

        /* Display refresh (75 Hz): stream the composed frame to the MAX7221. */
        if ((now - last_refresh) >= REFRESH_MS) {
            last_refresh = now;
            face_flush();
        }

        pin_low(LOOP_SCOPE);        /* scope: loop end */
    }

    return 0;
}
