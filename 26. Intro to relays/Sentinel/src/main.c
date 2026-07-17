/*
 * Sentinel (relay/sentinel) — FSM-driven relay controller
 *
 * Expands relay/hello_world (a blocking 1 Hz blink) into a 4-state finite
 * state machine driven by non-blocking Timer0 timing and a debounced button,
 * reporting its mode over UART. Load switched: a 12 V LED via an active-HIGH
 * relay module whose IN pin PB0 drives directly (module has its own coil driver).
 *
 * Superloop only — all timing is cooperative (sys_tick millis); the sole ISR
 * is the 1 ms Timer0 tick. No _delay_* anywhere in the run loop.
 */

#include "hardware_connections.h"  /* RELAY, BUTTON pin_t instances */
#include "sys_tick.h"
#include "button.h"
#include "relay_fsm.h"
#include "USART.h"                 /* initUSART */
#include "pin.h"                   /* pin_set_output */

#include <avr/interrupt.h>         /* sei() */

int main(void)
{
    sys_tick_init();          /* Timer0 1 ms timebase — configure before sei() */
    initUSART();              /* 9600 8N1 (BAUD default) for state reporting    */

    pin_set_output(RELAY);    /* PB0 -> relay module IN (active-HIGH)           */
    button_init(BUTTON);      /* PD2 input + internal pull-up                    */

    relay_fsm_init(RELAY);    /* restore last state (or safe AUTO) and print it  */

    sei();                    /* enable interrupts: start the millisecond tick   */

    for (;;) {
        button_service();                        /* sample + debounce (self-gated) */
        relay_fsm_tick(button_consume_event());  /* advance FSM, never blocks       */
    }
}
