/*
 * Ember — Pomodoro Desk-Lamp Controller (relay/pom_timer)
 *
 * A standalone AVR box: dial work/break minutes on a KY-040 encoder, watch a
 * 4-digit 7-segment display count down MM:SS, and when the work block hits
 * 0:00 a relay clicks a real desk lamp ON — an unmissable "take a break" cue.
 * Between sessions the MCU sleeps in power-down at µA, waking on a twist.
 *
 * Architecture: cooperative superloop. The only ISRs are Timer0 (1 ms tick),
 * Timer2 (display multiplex), and PCINT2 (encoder). No _delay_* in the run
 * loop; every timed action rides sys_millis().
 *
 * Timer map:  Timer0 = sys_tick   Timer1 = buzzer   Timer2 = display refresh
 */

#include "hardware_connections.h"   /* RELAY, BUZZER, ENC_*, SEG/DIGIT */
#include "sys_tick.h"
#include "display.h"
#include "buzzer.h"
#include "encoder.h"
#include "button.h"
#include "storage.h"
#include "ember_fsm.h"
#include "sleep.h"
#include "pin.h"

#include <avr/interrupt.h>          /* sei() */

/* ==== DEBUG display test selector ====
 *   0 = normal operation
 *   1 = common-CATHODE static test ("8." on DIG1, steady DC — verify NPN rework)
 *   2 = common-CATHODE probe test  (segments HIGH, digits off; probe by hand)
 * REMEMBER to set back to 0 before shipping. */
#define EMBER_DEBUG_TEST 0

int main(void)
{
#if EMBER_DEBUG_TEST == 1
    /* ---- Common-CATHODE static check (run AFTER installing NPN low-side
     *      digit switches: base<-PBx, collector->common, emitter->GND) ----
     * Holds DIG1 fully lit ("8.") steady, no multiplex, so every voltage is
     * steady DC. Expect: each SEG pin ~4.5-4.8 V (sourcing); DIG1 common near
     * 0 V (NPN saturated to GND); DIG1 shows "8." bright. Lights -> segment
     * polarity + DIG1 NPN switch both good, move to test 0. Dark -> NPN not
     * conducting (check base resistor / orientation) or wrong digit pin. */
    for (uint8_t i = 0; i < 8u; i++) { pin_set_output(SEG[i]); pin_high(SEG[i]); }  /* all segments LIT (active-high) */
    for (uint8_t d = 0; d < 4u; d++) { pin_set_output(DIGIT[d]); pin_low(DIGIT[d]); } /* all digits OFF (NPN base low) */
    pin_high(DIGIT[0]);            /* DIG1 NPN ON (active-high) -> common sunk to GND */
    for (;;) { }

#elif EMBER_DEBUG_TEST == 2
    /* ---- Common-CATHODE hypothesis, manual-probe test ----
     * WHY manual: your digit drivers are PNP HIGH-SIDE switches (source only).
     * A common-cathode part needs its common pulled to GND, which the PNPs
     * cannot do -> firmware alone can't light it. So we drive all 8 segment
     * pins HIGH (current-limited by the existing per-segment series resistors)
     * and leave the digit/common pins hi-Z (PNPs OFF). YOU complete the path.
     *
     * PROCEDURE (power on with this flashed):
     *   1. Take a jumper from a GND pin.
     *   2. Briefly touch it to each display COMMON pin in turn
     *      (CA56-12SRWA digit commons: pin 12=Dig1, 9=Dig2, 8=Dig3, 6=Dig4).
     *   3. A digit lighting up as "8." (all segments) => COMMON-CATHODE.
     *      Nothing lights on any common => NOT common-cathode (it's common-
     *      anode; your fault is a broken path, not the part type -> run test 1).
     *
     * SAFETY: each segment is limited by its own series resistor (~150-330R),
     * so ~15-30 mA per segment sourced by 8 separate MCU pins (safe per-pin);
     * the ~120-240 mA total sinks through YOUR external GND jumper, not the MCU.
     * Keep each touch brief. Do NOT jumper the common to +5V in this mode. */
    for (uint8_t d = 0; d < 4u; d++) { pin_set_output(DIGIT[d]); pin_high(DIGIT[d]); } /* PNP bases HIGH: Vbe=0 -> transistors firmly OFF, commons float */
    for (uint8_t i = 0; i < 8u; i++) { pin_set_output(SEG[i]); pin_high(SEG[i]); } /* all segment lines driven HIGH */
    for (;;) { }
#endif

#if EMBER_DEBUG_TEST == 0

    sys_tick_init();                /* Timer0 1 ms timebase — before sei()      */
    display_init();                 /* segment/digit pins + Timer2 multiplex     */
    buzzer_init(BUZZER);            /* Timer1 CTC on OC1A (PB1)                   */
    encoder_init(ENC_CLK, ENC_DT);  /* PCINT2 quadrature decode + pull-ups        */
    button_init(ENC_SW);            /* encoder push-button, active-low            */
    ember_fsm_init(RELAY);          /* storage_init + load presets, enter IDLE    */

    sei();                          /* interrupts live: tick, display, encoder    */

    for (;;) {
        button_service();           /* sample + debounce SW (self-gated)          */
        buzzer_service();           /* advance/expire tones (self-gated)          */

        ember_fsm_tick(encoder_consume_delta(), button_consume_event());

        /* Nothing to do in IDLE until the user acts — drop to power-down and
         * wait for a pin-change wake. Never sleep mid-chime (PWR_DOWN would
         * freeze Timer1 and cut the tone short). */
        if (ember_fsm_is_idle() && !buzzer_is_active()) {
            sleep_enter_power_down();
        }
    }
#endif /* EMBER_DEBUG_TEST == 0 */
}
