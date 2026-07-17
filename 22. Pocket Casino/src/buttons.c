/*
 * buttons.c
 *
 * Interrupt-driven button driver for two active-low pushbuttons.
 *
 * Hardware: BTN_A on PC0 (PCINT8), BTN_B on PC1 (PCINT9).
 * Both buttons share PCINT1_vect; pin state is read inside the ISR
 * to determine which button changed and in which direction.
 *
 * Debounce: the ISR records the change time from g_ms_tick. A transition
 * is ignored if it occurs within DEBOUNCE_MS of the previous one on the
 * same button. After DEBOUNCE_MS the press is confirmed.
 *
 * Long-press: if a button remains held for LONG_PRESS_MS without releasing,
 * a BTN_x_LONG event is generated (once) on that press.
 *
 * Event queue: ring buffer of depth 4. Oldest event is dropped on overflow
 * (single-player device, can't generate events faster than it can handle them).
 *
 * ATmega328P datasheet:
 *   Section 13.2   — Pin Change Interrupts overview
 *   Section 13.2.4 — PCICR (Pin Change Interrupt Control Register)
 *   Section 13.2.5 — PCIFR (Pin Change Interrupt Flag Register)
 *   Section 13.2.7 — PCMSK1 (mask for PCINT[14:8])
 *   Section 14.4   — PORTC / DDRC / PINC register descriptions
 */

#include "buttons.h"
#include "hardware_connections.h"
#include "rng.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stddef.h>

/* Shared 1 ms tick counter, incremented by Timer0 COMPA ISR in main.c */
extern volatile uint32_t g_ms_tick;

#define DEBOUNCE_MS     20U     /* ignore transitions within 20 ms of last */
#define LONG_PRESS_MS   800U    /* press held >= 800 ms → long event       */

#define QUEUE_SIZE  4U

/* Per-button tracking state (only accessed from ISR + poll, with cli guard). */
typedef struct
{
    uint8_t  pressed;       /* 1 while button is currently down   */
    uint8_t  long_fired;    /* 1 once the long-press event is sent */
    uint32_t press_time;    /* g_ms_tick snapshot when press began */
    uint32_t last_change;   /* g_ms_tick of last valid transition  */
} btn_state_t;

static volatile btn_state_t s_btn[2]; /* index 0=BTN_A, 1=BTN_B */

/* Event ring buffer */
static volatile btn_event_t s_queue[QUEUE_SIZE];
static volatile uint8_t     s_head = 0U;
static volatile uint8_t     s_tail = 0U;

static void queue_push(btn_event_t e)
{
    uint8_t next = (uint8_t)((s_tail + 1U) % QUEUE_SIZE);
    if (next != s_head)     /* drop on full — never blocks ISR */
    {
        s_queue[s_tail] = e;
        s_tail = next;
    }
}

void buttons_init(void)
{
    /* PC0, PC1: inputs (clear DDR bits).
     * ATmega328P datasheet Section 14.4.9 (DDRC) */
    BTN_DDR &= ~((1U << BTN_A_PIN) | (1U << BTN_B_PIN));

    /* Enable internal pull-ups by writing 1 to PORTC pins while in input mode.
     * ATmega328P datasheet Section 14.2.1 */
    BTN_PORT |= (1U << BTN_A_PIN) | (1U << BTN_B_PIN);

    /* Enable Pin Change Interrupt 1 group (PCINT[14:8] = PORTC).
     * ATmega328P datasheet Section 13.2.4 — PCICR.PCIE1 */
    PCICR |= (1U << PCIE1);

    /* Unmask only PC0 (PCINT8) and PC1 (PCINT9).
     * ATmega328P datasheet Section 13.2.7 — PCMSK1 */
    PCMSK1 |= (1U << BTN_A_PCINT_BIT) | (1U << BTN_B_PCINT_BIT);
}

/* PCINT1_vect fires on ANY change to any unmasked PCINT1 pin.
 * Read PINC to determine the new state of both buttons. */
ISR(PCINT1_vect)
{
    uint32_t now     = g_ms_tick;
    uint8_t  pinc    = BTN_PIN_REG;
    uint8_t  a_down  = ((pinc & (1U << BTN_A_PIN)) == 0U) ? 1U : 0U;
    uint8_t  b_down  = ((pinc & (1U << BTN_B_PIN)) == 0U) ? 1U : 0U;
    uint8_t  states[2];
    uint8_t  i;

    states[0] = a_down;
    states[1] = b_down;

    /* Mix button press timestamp into RNG (human-timing entropy). */
    rng_reseed_mix((uint16_t)now);

    for (i = 0U; i < 2U; i++)
    {
        if (states[i] != s_btn[i].pressed)
        {
            /* Debounce: ignore bounces within DEBOUNCE_MS */
            if ((now - s_btn[i].last_change) < DEBOUNCE_MS)
            {
                continue;
            }

            s_btn[i].last_change = now;

            if (states[i])
            {
                /* Falling edge (button pressed) */
                s_btn[i].pressed    = 1U;
                s_btn[i].press_time = now;
                s_btn[i].long_fired = 0U;
            }
            else
            {
                /* Rising edge (button released) */
                s_btn[i].pressed = 0U;
                if (!s_btn[i].long_fired)
                {
                    /* Short press: emit press event */
                    queue_push((i == 0U) ? BTN_A_PRESS : BTN_B_PRESS);
                }
            }
        }
    }
}

btn_event_t buttons_poll(void)
{
    btn_event_t e = BTN_NONE;
    uint32_t    now;
    uint8_t     i;

    /* Check for long-press threshold on currently-held buttons. */
    cli();
    now = g_ms_tick;
    for (i = 0U; i < 2U; i++)
    {
        if (s_btn[i].pressed && !s_btn[i].long_fired)
        {
            if ((now - s_btn[i].press_time) >= LONG_PRESS_MS)
            {
                s_btn[i].long_fired = 1U;
                queue_push((i == 0U) ? BTN_A_LONG : BTN_B_LONG);
            }
        }
    }
    sei();

    /* Drain one event from the queue. */
    cli();
    if (s_head != s_tail)
    {
        e = s_queue[s_head];
        s_head = (uint8_t)((s_head + 1U) % QUEUE_SIZE);
    }
    sei();

    return e;
}
