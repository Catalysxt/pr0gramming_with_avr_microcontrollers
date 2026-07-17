/*
 * buttons.c
 *
 * Purpose : Edge/level detection for JUMP (PC0) and DUCK (PC1) buttons.
 * Hardware : PC0/PCINT8 (JUMP), PC1/PCINT9 (DUCK) — active-low, pull-up.
 * Datasheet: ATmega328P §13 (PCINT), §14.2 (pull-ups),
 *            Table 13-1 (PCICR/PCMSK1), §13.2.5 (PCINT1_vect)
 *
 * Buzzer SFX is fired directly from the ISR (per design spec) using
 * buzzer_tone_nb(), which only writes Timer2 registers — no blocking.
 * sys_millis() inside the ISR is safe: ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
 * saves SREG (I=0 inside ISR), so cli()/sei() are effectively no-ops.
 */

#include "buttons.h"
#include "hardware_connections.h"
#include "buzzer.h"
#include "sys_tick.h"
#include "game.h"           /* JUMP_PITCH, JUMP_MS, DUCK_PITCH, DUCK_MS */

#include <avr/io.h>
#include <avr/interrupt.h>

/* Debounce window — ignore transitions within 5 ms of the last one. */
#define DEBOUNCE_MS  5u

/* ISR-shared state. */
static volatile uint8_t  s_pinc_prev  = 0xFFu;   /* last sampled PINC value   */
static volatile bool     g_jump_edge  = false;    /* one-shot: jump was pressed */
static volatile bool     g_ducking    = false;    /* current duck level         */

/* Per-pin debounce timestamps (ms). */
static volatile uint32_t s_last_jump_ms = 0u;
static volatile uint32_t s_last_duck_ms = 0u;

void buttons_init(void)
{
    /* PC0, PC1 as inputs (clear DDR bits). */
    BTN_DDR &= ~((1u << JUMP_BIT) | (1u << DUCK_BIT));

    /* Enable internal pull-ups (set PORT bits while DDR=0).
     * Datasheet §14.2.1 — writing 1 to PORTxn when DDRxn=0 enables pull-up. */
    BTN_PORT |= (1u << JUMP_BIT) | (1u << DUCK_BIT);

    /* Capture initial pin state (both high = released, with pull-ups). */
    s_pinc_prev = BTN_PINREG;

    /* Enable pin-change interrupt group 1 (PORTC).
     * PCIE1 in PCICR arms the PCINT1_vect.  (datasheet §13.2.4) */
    PCICR |= (1u << PCIE1);

    /* Unmask individual pins within group 1.
     * PCINT8 = PC0 (bit 0 of PCMSK1), PCINT9 = PC1 (bit 1).
     * Datasheet §13.2.8 — PCMSK1 */
    PCMSK1 |= (1u << PCINT8) | (1u << PCINT9);
}

ISR(PCINT1_vect)
{
    uint8_t  now_pinc = BTN_PINREG;
    uint8_t  changed  = now_pinc ^ s_pinc_prev;
    uint32_t now_ms   = sys_millis();  /* safe inside ISR — see file header */

    /* ---- JUMP button (PC0, active-low) ---- */
    if (changed & (1u << JUMP_BIT)) {
        if ((now_ms - s_last_jump_ms) >= DEBOUNCE_MS) {
            s_last_jump_ms = now_ms;
            bool pressed = !(now_pinc & (1u << JUMP_BIT)); /* falling = press */
            if (pressed) {
                g_jump_edge = true;
                /* Fire SFX immediately so the press feels responsive.
                 * Beep fires even if the game state does not allow a jump;
                 * game_tick_update() ignores the edge in that case. */
                buzzer_tone_nb(JUMP_PITCH, JUMP_MS);
            }
        }
    }

    /* ---- DUCK button (PC1, active-low) ---- */
    if (changed & (1u << DUCK_BIT)) {
        if ((now_ms - s_last_duck_ms) >= DEBOUNCE_MS) {
            s_last_duck_ms = now_ms;
            bool pressed = !(now_pinc & (1u << DUCK_BIT));
            g_ducking = pressed;
            if (pressed) {
                buzzer_tone_nb(DUCK_PITCH, DUCK_MS);
            }
        }
    }

    s_pinc_prev = now_pinc;
}

bool buttons_consume_jump_edge(void)
{
    /* Atomically read and clear the one-shot flag. */
    bool edge;
    uint8_t sreg = SREG;
    __asm__ volatile ("cli" ::: "memory");
    edge        = g_jump_edge;
    g_jump_edge = false;
    SREG        = sreg;
    return edge;
}

bool buttons_ducking(void)
{
    /* Single-byte read — no ATOMIC_BLOCK needed on AVR. */
    return g_ducking;
}
