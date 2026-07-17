/*
 * 4x4 matrix keypad scanner for ATmega328P.
 *
 * Rows on PORTB low nibble: PB0=row0, PB1=row1, PB2=row2, PB3=row3 (outputs).
 * Columns on PORTC low nibble: PC0-PC3 (inputs with pull-ups).
 * All 4 rows — including row 3 (*,0,#,D) — are now reachable.
 * The decoded ASCII character is written to PORTD (PD0..PD7).
 *
 * Scan: wait for all keys released, debounce, then ground each row in
 * turn to identify the active (row, column) pair.
 */

#include <avr/io.h>     /* register / bit definitions */
#include <util/delay.h> /* _delay_ms(), _delay_us() */

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

/* Reverse the bit order of a byte.
 * D1 is wired to PD0 and read as the MSB, so the natural PORTD write is
 * mirrored.  Three swap passes (nibbles → pairs → adjacent) correct this. */
static unsigned char reverse_byte(unsigned char b)
{
    b = (unsigned char)(((b & 0xF0u) >> 4) | ((b & 0x0Fu) << 4));
    b = (unsigned char)(((b & 0xCCu) >> 2) | ((b & 0x33u) << 2));
    b = (unsigned char)(((b & 0xAAu) >> 1) | ((b & 0x55u) << 1));
    return b;
}

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

    /* PORTD drives 8 LEDs that display the ASCII value of the pressed key */
    DDRD  = 0xFF;

    /* PB0-PB3: row outputs, initially high (all rows inactive) */
    ROW_DDR |= ROW_MASK;
    ROW_PRT |= ROW_MASK;

    /* PC0-PC3: column inputs with pull-ups enabled.
     * DDRC resets to 0 (all inputs) but written explicitly for clarity. */
    COL_DDR &= ~COL_MASK;
    COL_PRT |=  COL_MASK;

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

        /* Decode the column bit pattern, reverse the bit order to match the
         * D1=MSB physical LED layout, and drive the result onto PORTD. */
        if      (colloc == 0x0E) PORTD = reverse_byte(keypad[rowloc][0]);
        else if (colloc == 0x0D) PORTD = reverse_byte(keypad[rowloc][1]);
        else if (colloc == 0x0B) PORTD = reverse_byte(keypad[rowloc][2]);
        else if (colloc == 0x07) PORTD = reverse_byte(keypad[rowloc][3]);
    }
    return 0;
}
