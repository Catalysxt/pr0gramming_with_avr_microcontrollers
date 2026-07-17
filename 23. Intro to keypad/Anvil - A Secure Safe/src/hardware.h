/*
 * hardware.h
 *
 * Purpose: single source of truth for every ATmega328P pin used by ANVIL.
 *          No logic lives here — #defines only. If a pin assignment needs
 *          to change, edit this file, never a driver.
 * Hardware touched: buzzer (PB3/OC2A), green/red LEDs (PB4/PB5), 4x4
 *          matrix keypad columns (PC0-PC3) and rows (PC4, PC5, PD2, PD3).
 * Datasheet: ATmega328P datasheet §14 (I/O Ports) for DDRx/PORTx/PINx
 *            register semantics.
 *
 * LCD pins (PB0=RS, PB1=R/W, PB2=E, PD4-PD7=data high nibble) are NOT
 * redefined here — ../../lcd/lcd_driver/lcd_driver.h already owns them
 * (LCD_RS_PORT/DDR/PIN, LCD_RW_*, LCD_E_*, LCD_DATA_PORTREG/DDR/PINREG).
 * Duplicating them here would create two conflicting sources of truth.
 */

#ifndef ANVIL_HARDWARE_H_
#define ANVIL_HARDWARE_H_

#include <avr/io.h>

/* ---- Buzzer (passive piezo, Timer2 CTC, toggle OC2A) ---- */
#define BUZZER_DDR      DDRB
#define BUZZER_PORT     PORTB
#define BUZZER_BIT      PB3    /* OC2A output compare pin, datasheet §17 */

/* ---- LEDs (push-pull, through 330R to GND) ---- */
#define LED_GREEN_DDR   DDRB
#define LED_GREEN_PORT  PORTB
#define LED_GREEN_BIT   PB4

#define LED_RED_DDR     DDRB
#define LED_RED_PORT    PORTB
#define LED_RED_BIT     PB5

/* ---- Keypad (4x4 matrix, active-low) ----
 * Columns: PC0-PC3, inputs with internal pull-ups. Released reads 1,
 * pressed reads 0 (the driven-low row pulls the column down through the
 * closed switch contact).
 *
 * Rows: driven high by default, one at a time driven low to scan. Rows are
 * split across two ports — ROW0/ROW1 live on PORTC alongside the columns,
 * ROW2/ROW3 live on PORTD. keypad.c must scan via a per-row lookup, never
 * assume a single contiguous register/nibble.
 */
#define KEY_COL_DDR     DDRC
#define KEY_COL_PORT    PORTC
#define KEY_COL_PIN     PINC
#define KEY_COL_MASK    0x0Fu   /* PC0..PC3 */
#define KEY_COL_COUNT   4u

#define KEY_ROW0_DDR    DDRC
#define KEY_ROW0_PORT   PORTC
#define KEY_ROW0_BIT    PC4

#define KEY_ROW1_DDR    DDRC
#define KEY_ROW1_PORT   PORTC
#define KEY_ROW1_BIT    PC5

#define KEY_ROW2_DDR    DDRD
#define KEY_ROW2_PORT   PORTD
#define KEY_ROW2_BIT    PD2

#define KEY_ROW3_DDR    DDRD
#define KEY_ROW3_PORT   PORTD
#define KEY_ROW3_BIT    PD3

#define KEY_ROW_COUNT   4u

/* PC6 (RESET) and PC7 (does not exist on DIP-28) must never be referenced.
 * PD0/PD1 (RXD/TXD) are reserved for stretch UART work — leave untouched. */

#endif /* ANVIL_HARDWARE_H_ */
