/*
 * 4x4 matrix keypad scanner — UART output for ATmega328P.
 *
 * Rows on PORTB low nibble: PB0=row0, PB1=row1, PB2=row2, PB3=row3 (outputs).
 * Columns on PORTC low nibble: PC0-PC3 (inputs with pull-ups).
 * On each keypress the decoded character is sent over USART0 (9600 8N1).
 *
 * PORTD is intentionally left alone: USART0 TX is on PD1 and USART0 RX is on
 * PD0 — driving PORTD for LEDs while UART is active would corrupt serial data.
 */

#include <avr/io.h>     /* register / bit definitions */
#include <util/delay.h> /* _delay_ms(), _delay_us() */
#include "USART.h"      /* initUSART(), transmitByte(), printString() */

/* Row outputs — PB0-PB3.
 * PB6/7 are XTAL1/XTAL2 (16 MHz crystal) — never overwrite the full byte;
 * always use read-modify-write to leave PB4-PB7 untouched. */
#define ROW_DDR   DDRB
#define ROW_PRT   PORTB
#define ROW_MASK  0x0F   /* PB0-PB3 */

/* Column inputs — PC0-PC3 with internal pull-ups */
#define COL_DDR      DDRC
#define COL_PRT      PORTC
#define COL_PIN      PINC
#define COL_MASK     0x0F   /* PC0-PC3 */

/* After driving a new row low, wait this long before reading the columns.
 * The 50 kΩ internal pull-up must recharge any line that was held low by a
 * previously-active row.  With ~100 pF of breadboard capacitance the RC time
 * constant is ~5 µs; 10 µs covers >2τ, ensuring a clean logic-high read. */
#define ROW_SETTLE_US  10u

/* ASCII lookup: keypad[row][col] */
static const unsigned char keypad[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

int main(void)
{
    unsigned char colloc, rowloc = 0;
    unsigned char key;

    /* Configure USART0 for 9600 8N1.  BAUD and F_CPU are supplied by the
     * Makefile; initUSART() computes UBRR = F_CPU/16/BAUD-1 internally. */
    initUSART();

    /* PB0-PB3: row outputs, initially high (all rows inactive) */
    ROW_DDR |= ROW_MASK;
    ROW_PRT |= ROW_MASK;

    /* PC0-PC3: column inputs with pull-ups enabled.
     * DDRC resets to 0 (all inputs) but written explicitly for clarity. */
    COL_DDR &= ~COL_MASK;
    COL_PRT |=  COL_MASK;

    /* Confirm MCU is live and ready before the scan loop starts */
    printString("Keypad ready\r\n");

    while (1)
    {
        /* Drive all rows low so any keypress pulls a column pin down,
         * then spin until all four column lines read high (key released). */
        do {
            ROW_PRT &= ~ROW_MASK;
            colloc = COL_PIN & COL_MASK;
        } while (colloc != COL_MASK);

        /* Rows are still all low from the release check above.
         * Two-sample debounce: key must read pressed both before and after 20 ms. */
        do {
            do {
                _delay_ms(20);
                colloc = COL_PIN & COL_MASK;
            } while (colloc == COL_MASK);    /* spin until a column goes low */
            _delay_ms(20);
            colloc = COL_PIN & COL_MASK;
        } while (colloc == COL_MASK);        /* reject glitch if gone after 20 ms */

        /* Ground one row at a time (others high) to locate the pressed row.
         * (ROW_PRT | ROW_MASK) raises PB0-PB3, then & ~bit clears only that row pin.
         * PB4-PB7 (including XTAL pins) are preserved throughout.
         * ROW_SETTLE_US delay lets the 50 kΩ pull-up recharge any column that was
         * held low by the previous all-rows-active phase before we sample it. */
        while (1)
        {
            ROW_PRT = (ROW_PRT | ROW_MASK) & ~0x01; /* PB0 low — row 0 */
            _delay_us(ROW_SETTLE_US);
            colloc  = COL_PIN & COL_MASK;
            if (colloc != COL_MASK) { rowloc = 0; break; }

            ROW_PRT = (ROW_PRT | ROW_MASK) & ~0x02; /* PB1 low — row 1 */
            _delay_us(ROW_SETTLE_US);
            colloc  = COL_PIN & COL_MASK;
            if (colloc != COL_MASK) { rowloc = 1; break; }

            ROW_PRT = (ROW_PRT | ROW_MASK) & ~0x04; /* PB2 low — row 2 */
            _delay_us(ROW_SETTLE_US);
            colloc  = COL_PIN & COL_MASK;
            if (colloc != COL_MASK) { rowloc = 2; break; }

            ROW_PRT = (ROW_PRT | ROW_MASK) & ~0x08; /* PB3 low — row 3 */
            _delay_us(ROW_SETTLE_US);
            colloc  = COL_PIN & COL_MASK;
            if (colloc != COL_MASK) { rowloc = 3; }
            break;
        }

        /* Decode the column bit pattern into an ASCII character.
         * Pull-up + open-drain: the active column pin reads 0, the rest read 1. */
        key = 0;
        if      (colloc == 0x0E) key = keypad[rowloc][0]; /* col 0: bit 0 low */
        else if (colloc == 0x0D) key = keypad[rowloc][1]; /* col 1: bit 1 low */
        else if (colloc == 0x0B) key = keypad[rowloc][2]; /* col 2: bit 2 low */
        else if (colloc == 0x07) key = keypad[rowloc][3]; /* col 3: bit 3 low */

        if (key)
        {
            printString("You pressed key: ");
            transmitByte(key);
            transmitByte('\r');
            transmitByte('\n');
        }
    }
    return 0;
}
