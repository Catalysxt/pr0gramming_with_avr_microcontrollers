/*
 * sleep.c
 *
 * Purpose : µA-range idle. In IDLE the box has nothing to do until the user
 *           touches the encoder, so it enters the deepest sleep and waits for
 *           a pin-change wake. No 32.768 kHz crystal / Timer2-async needed:
 *           a Pomodoro only keeps time *after* you start it, so we never have
 *           to count while asleep.
 * Hardware: encoder CLK/DT/SW on PCINT2 (enabled in encoder_init); Timer0/1/2
 *           + ADC clocks gated via PRR.
 * Datasheet: ATmega328P §9.5 (Power-down), §9.11 (PRR), §12.2 (PCINT).
 *
 * The sleep_enable()/sei()/sleep_cpu() ordering is the textbook race-free
 * AVR idiom: enabling global interrupts immediately before SLEEP guarantees
 * that a wake event firing in the gap still runs sleep_cpu() as its very next
 * instruction, so an early wake can never be missed.
 */

#include "sleep.h"
#include "display.h"
#include "buzzer.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>

void sleep_enter_power_down(void)
{
    display_stop();    /* blank digits (no stuck segment sinking current) + kill Timer2 IRQ */
    buzzer_off();      /* make sure nothing is mid-tone                                     */

    /* Gate peripheral clocks so power-down current is truly minimal. In
     * PWR_DOWN the oscillator stops anyway, but PRR also cuts the residual
     * draw of each block. Timer0's sys_tick simply freezes and resumes on
     * wake — fine, because IDLE isn't counting time. <avr/power.h> */
    power_timer0_disable();
    power_timer1_disable();
    power_timer2_disable();
    power_adc_disable();

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);   /* §9.5 deepest sleep */
    cli();
    sleep_enable();
    sei();
    sleep_cpu();       /* halts here; PCINT2 (encoder) wakes the core, resumes below */
    sleep_disable();

    power_timer0_enable();
    power_timer1_enable();
    power_timer2_enable();
    power_adc_enable();

    display_resume();  /* restart the multiplex; framebuffer is still blank (IDLE) */
}
