/*
 * scrolling_uart_text — receive a string over UART and scroll it across LCD
 * row 0.  A potentiometer on PC0 (ADC0) sets the scroll speed in real time.
 *
 * Hardware connections:
 *   LCD control : PB0 = RS   PB1 = RW   PB2 = E
 *   LCD data bus: PD4-PD7 = DB4-DB7  (4-bit mode)
 *   UART        : PD0 = RXD  PD1 = TXD  (9600 8N1)
 *   Speed pot   : PC0 (ADC0) — wiper between GND and AVCC
 *                 fully CCW → slowest (600 ms/step)
 *                 fully CW  → fastest  (50 ms/step)
 *
 * The LCD data bus uses only PD4:PD7, so PD0/PD1 (UART) are never touched
 * by the LCD driver and there is no pin sharing between LCD and UART.
 *
 * Usage: open a serial terminal at 9600 8N1.  Type a message and press
 * Enter (CR).  The text scrolls across the LCD; turn the pot to change
 * speed.  The ready prompt returns when scrolling is finished.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include "../lcd_driver/lcd_driver.h"
#include "USART.h"

/* ── Scroll-speed range ──────────────────────────────────────────────────── */
#define SCROLL_MS_MIN   50U     /* fastest step: pot fully CW  (ms) */
#define SCROLL_MS_MAX  600U     /* slowest step: pot fully CCW (ms) */

/* ── Display geometry ────────────────────────────────────────────────────── */
#define LCD_COLS   16U
#define MAX_MSG    64U

/* ── ADC ─────────────────────────────────────────────────────────────────── */

static void adc_init(void)
{
    /* AVCC reference (REFS0=1), channel 0 selected by default (MUX=0b0000) */
    ADMUX  = (1U << REFS0);

    /* Enable ADC; prescaler = /128 → 16 MHz / 128 = 125 kHz (within spec) */
    ADCSRA = (1U << ADEN)
           | (1U << ADPS2) | (1U << ADPS1) | (1U << ADPS0);

    /* Disable the digital input buffer on PC0 to reduce noise on ADC0 */
    DIDR0 |= (1U << ADC0D);
}

static uint16_t adc_read(uint8_t ch)
{
    /* Select channel; keep reference bits in ADMUX[7:4] */
    ADMUX = (ADMUX & 0xF0U) | (ch & 0x0FU);

    ADCSRA |= (1U << ADSC);                   /* start single conversion */
    loop_until_bit_is_clear(ADCSRA, ADSC);    /* wait for ADSC to clear  */
    return ADC;                                /* read 10-bit result      */
}

/*
 * Map ADC [0..1023] → delay [SCROLL_MS_MAX..SCROLL_MS_MIN].
 * Higher pot voltage (CW) = shorter delay = faster scroll.
 */
static uint16_t pot_delay_ms(void)
{
    uint16_t adc   = adc_read(0U);
    uint16_t range = (uint16_t)(SCROLL_MS_MAX - SCROLL_MS_MIN);
    /* invert: adc=0 → max delay, adc=1023 → min delay */
    return (uint16_t)(SCROLL_MS_MAX - ((uint32_t)adc * range >> 10U));
}

/* ── Delay helper ────────────────────────────────────────────────────────── */

/* _delay_ms() requires a compile-time constant; drive it one tick at a time. */
static void delay_ms_var(uint16_t ms)
{
    uint16_t i;
    for (i = 0U; i < ms; i++)
        _delay_ms(1.0);
}

/* ── Scrolling ───────────────────────────────────────────────────────────── */

/*
 * Scroll msg right-to-left across LCD row 0.
 *
 * At step s, display column c shows character at position (c + s) in the
 * padded string  [LCD_COLS-1 spaces] + msg + [trailing spaces]:
 *
 *   src = c + s
 *   src < LCD_COLS-1               → ' '  (leading pad)
 *   src - (LCD_COLS-1) < msg_len   → msg[src - (LCD_COLS-1)]
 *   otherwise                      → ' '  (trailing pad)
 *
 * Total steps = msg_len + LCD_COLS (includes one fully-blank exit frame).
 * The ADC is sampled once per step so the pot takes effect immediately.
 */
static void scroll_display(const char *msg)
{
    uint8_t len   = (uint8_t)strlen(msg);
    uint8_t total = len + (uint8_t)LCD_COLS;
    uint8_t step, col;

    for (step = 0U; step < total; step++)
    {
        uint16_t delay = pot_delay_ms();   /* sample pot before each step */

        Lcd_SetCursor(0U, 0U);
        for (col = 0U; col < (uint8_t)LCD_COLS; col++)
        {
            uint8_t src    = col + step;
            uint8_t offset = (uint8_t)(LCD_COLS - 1U);
            char    ch;

            if (src < offset)
                ch = ' ';
            else if ((src - offset) < len)
                ch = msg[src - offset];
            else
                ch = ' ';

            Lcd_WriteChar((uint8_t)ch);
        }

        delay_ms_var(delay);
    }
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void)
{
    char msg[MAX_MSG + 1U];

    Lcd_Init();
    initUSART();
    adc_init();

    printString("Enter your text: ");

    for (;;)
    {
        readString(msg, (uint8_t)(MAX_MSG + 1U));
        printString("\r\n");    /* move to a new line immediately after Enter */

        if (msg[0] == '\0')
        {
            printString("Enter your text: ");
            continue;
        }

        Lcd_Clear();
        Lcd_SetCursor(0U, 1U);
        Lcd_WriteString(" << scrolling >>");

        scroll_display(msg);

        Lcd_Clear();
        printString("Enter your text: ");
    }

    return 0;
}
