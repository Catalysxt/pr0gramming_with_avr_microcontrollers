/*
 * leds.c
 *
 * Purpose : Green/red status LED control — direct on/off, one-shot
 *           pulses, and a slow blink scheduler, all non-blocking.
 * Hardware: PB4 (green), PB5 (red), each push-pull through 330R to GND.
 * Datasheet: ATmega328P §14.2.1 (DDRx/PORTx as push-pull output config)
 */

#include "leds.h"
#include "hardware.h"
#include "sys_tick.h"

#include <avr/io.h>

static bool     s_red_blink_active = false;
static uint16_t s_red_blink_period = 0;
static uint32_t s_red_blink_anchor = 0;

static bool     s_green_pulse_active  = false;
static uint32_t s_green_pulse_stop_at = 0;

static bool     s_red_pulse_active  = false;
static uint32_t s_red_pulse_stop_at = 0;

void leds_init(void)
{
    LED_GREEN_DDR  |= (uint8_t)(1u << LED_GREEN_BIT);
    LED_RED_DDR    |= (uint8_t)(1u << LED_RED_BIT);
    LED_GREEN_PORT &= (uint8_t)~(1u << LED_GREEN_BIT);
    LED_RED_PORT   &= (uint8_t)~(1u << LED_RED_BIT);
}

void leds_green(bool on)
{
    s_green_pulse_active = false;   /* explicit control overrides a pulse */
    if (on) {
        LED_GREEN_PORT |= (uint8_t)(1u << LED_GREEN_BIT);
    } else {
        LED_GREEN_PORT &= (uint8_t)~(1u << LED_GREEN_BIT);
    }
}

void leds_red(bool on)
{
    s_red_blink_active = false;     /* explicit control overrides blink/pulse */
    s_red_pulse_active  = false;
    if (on) {
        LED_RED_PORT |= (uint8_t)(1u << LED_RED_BIT);
    } else {
        LED_RED_PORT &= (uint8_t)~(1u << LED_RED_BIT);
    }
}

void leds_red_blink(uint16_t period_ms)
{
    if (period_ms == 0u) {
        s_red_blink_active = false;
        LED_RED_PORT &= (uint8_t)~(1u << LED_RED_BIT);
        return;
    }
    s_red_pulse_active  = false;
    s_red_blink_active  = true;
    s_red_blink_period  = period_ms;
    s_red_blink_anchor  = sys_millis();
    LED_RED_PORT |= (uint8_t)(1u << LED_RED_BIT);  /* start lit */
}

void leds_pulse_green(uint16_t duration_ms)
{
    LED_GREEN_PORT |= (uint8_t)(1u << LED_GREEN_BIT);
    s_green_pulse_active  = true;
    s_green_pulse_stop_at = sys_millis() + duration_ms;
}

void leds_pulse_red(uint16_t duration_ms)
{
    s_red_blink_active = false;   /* pulse takes precedence over blink */
    LED_RED_PORT |= (uint8_t)(1u << LED_RED_BIT);
    s_red_pulse_active  = true;
    s_red_pulse_stop_at = sys_millis() + duration_ms;
}

void leds_service(void)
{
    if (s_green_pulse_active && (sys_millis() >= s_green_pulse_stop_at)) {
        LED_GREEN_PORT &= (uint8_t)~(1u << LED_GREEN_BIT);
        s_green_pulse_active = false;
    }

    if (s_red_pulse_active) {
        if (sys_millis() >= s_red_pulse_stop_at) {
            LED_RED_PORT &= (uint8_t)~(1u << LED_RED_BIT);
            s_red_pulse_active = false;
        }
        return;   /* pulse owns the red LED until it expires */
    }

    if (s_red_blink_active) {
        if (sys_elapsed(&s_red_blink_anchor, (uint32_t)(s_red_blink_period / 2u))) {
            LED_RED_PORT ^= (uint8_t)(1u << LED_RED_BIT);  /* toggle */
        }
    }
}
