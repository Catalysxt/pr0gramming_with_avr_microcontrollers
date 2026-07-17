/*
 * sleep.c
 *
 * Purpose : 30-second idle power-down with PCINT wake on the keypad
 *           columns. No FSM state lives here — sleep is transparent to
 *           ascent.c, which is what makes it state-preserving.
 * Hardware: PCINT8-11 (PC0-PC3, the keypad columns, reusing
 *           hardware.h's KEY_COL_MASK), Timer0/Timer2/ADC power gating.
 * Datasheet: ATmega328P §12.2.3 (PCICR/PCIE1), §12.2.4 (PCMSK1),
 *            §9.5-9.6 (Sleep Modes / Power-down mode)
 *
 * Sleep-wake trick: before sleeping, every row pin is driven LOW
 * (keypad_rows_sleep()). While asleep, pressing ANY of the 16 keys shorts
 * its column to its (now-low) row, pulling that column low — which is
 * exactly the falling edge PCINT8-11 is configured to catch. The ISR body
 * is intentionally near-empty: sleep_cpu() returning IS the wake
 * mechanism, the ISR only needs to exist so the interrupt is serviced
 * (an enabled interrupt with no ISR would be a build error) and to give a
 * future feature a "did we just wake from a real keypress" flag to read.
 *
 * sleep_enable()/sei()/sleep_cpu() in that exact order is the textbook
 * AVR race-free sleep idiom: enabling global interrupts before sleep_cpu()
 * (rather than after) means a PCINT firing in the gap between sei() and
 * the actual SLEEP instruction still executes sleep_cpu() as its very
 * next instruction (guaranteed by the avr-libc sleep_mode()/sleep_cpu()
 * contract), so a wake source that fires early can never be missed.
 */

#include "sleep.h"
#include "hardware.h"
#include "keypad.h"
#include "buzzer.h"
#include "ui.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <stdint.h>

/* Set by the ISR on every PCINT1 event. Not consumed elsewhere in v1 —
 * sleep_cpu() returning is the whole wake mechanism — kept as a hook for
 * a future feature that needs to distinguish a real wake from a spurious
 * one, per the subsystem spec. */
static volatile uint8_t g_wake_flag = 0;

void sleep_init(void)
{
    /* PCINT8-11 == PC0-3 == the keypad columns; KEY_COL_MASK (0x0Fu)
     * lines up exactly since PCMSK1 bit N enables PCINT(8+N) == PCn.
     * Datasheet §12.2.4 (PCMSK1). */
    PCMSK1 |= KEY_COL_MASK;
    /* PCIE1: enable the PCINT[14:8] interrupt group. Datasheet §12.2.3. */
    PCICR  |= (1u << PCIE1);
}

ISR(PCINT1_vect)
{
    g_wake_flag = 1u;   /* body intentionally minimal, per the <20-line ISR rule */
}

void sleep_enter_power_down(void)
{
    ui_blank();
    buzzer_off();
    keypad_rows_sleep();   /* rows LOW: any keypress now pulls a column low */

    /* Gate unused peripheral clocks via PRR before sleeping — reduces
     * power-down current and matches the spec's explicit power budget.
     * <avr/power.h> */
    power_timer0_disable();
    power_timer2_disable();
    power_adc_disable();

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);   /* datasheet §9.5, deepest sleep mode */
    cli();
    sleep_enable();
    sei();
    sleep_cpu();       /* halts here; PCINT1_vect wakes the core, execution resumes below */
    sleep_disable();

    power_timer0_enable();
    power_timer2_enable();
    power_adc_enable();
    keypad_rows_wake();     /* restore ROW0-3 idle-HIGH for normal scanning */

    /* Deliberately NOT redrawing here — main.c calls ascent_redraw()
     * right after this returns, so whatever screen was showing before
     * sleep reappears. Keeps sleep.c decoupled from ascent.h. */
}
