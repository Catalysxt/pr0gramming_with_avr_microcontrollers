/*
 * HD44780 LCD driver in 4-bit mode for ATmega328P.
 *
 * Data lines D4-D7 are on the high nibble of PORTD; control lines
 * RS, RW, EN are on PORTB bits 0, 1, 2. Each byte is sent as two
 * EN-latched writes: the high nibble first, then the low nibble shifted
 * up into D4-D7. The init sequence (0x33, 0x32, 0x28) forces the
 * controller into 4-bit, 2-line, 5x7 mode. lcd_gotoxy() uses 1-based
 * (x, y) coordinates mapped to DDRAM bases 0x80, 0xC0, 0x94, 0xD4.
 * The program prints a two-line message.
 */

#include <avr/io.h>             // standard AVR header
#include <util/delay.h>        // delay header

// LCD data port (high nibble = D4-D7)
#define LCD_DPRT PORTD          
#define LCD_DDDR DDRD
#define LCD_DPIN PIND

// LCD command port
#define LCD_CPRT PORTB         
#define LCD_CDDR DDRB
#define LCD_CPIN PINB
#define LCD_RS 0 // PB0
#define LCD_RW 1 // PB1
#define LCD_EN 2 // PB2

// _delay_us() is a macro, not a function
#define delay_us(d) _delay_us(d)

void lcdCommand(unsigned char cmnd)
{
    LCD_DPRT = cmnd & 0xF0;         // high nibble to D4-D7
    LCD_CPRT &= ~(1 << LCD_RS);     // RS = 0 for command
    LCD_CPRT &= ~(1 << LCD_RW);     // RW = 0 for write
    LCD_CPRT |=  (1 << LCD_EN);     // EN = 1
    delay_us(1);
    LCD_CPRT &= ~(1 << LCD_EN);     // latch high nibble
    delay_us(100);
    LCD_DPRT = cmnd << 4;           // low nibble to D4-D7
    LCD_CPRT |=  (1 << LCD_EN);     // EN = 1
    delay_us(1);
    LCD_CPRT &= ~(1 << LCD_EN);     // latch low nibble
    delay_us(100);
}

void lcdData(unsigned char data)
{
    LCD_DPRT = data & 0xF0;         // high nibble to D4-D7
    LCD_CPRT |=  (1 << LCD_RS);     // RS = 1 for data
    LCD_CPRT &= ~(1 << LCD_RW);     // RW = 0 for write
    LCD_CPRT |=  (1 << LCD_EN);     // EN = 1
    delay_us(1);
    LCD_CPRT &= ~(1 << LCD_EN);     // latch high nibble
    LCD_DPRT = data << 4;           // low nibble to D4-D7
    LCD_CPRT |=  (1 << LCD_EN);     // EN = 1
    delay_us(1);
    LCD_CPRT &= ~(1 << LCD_EN);     // latch low nibble
    delay_us(100);
}

void lcd_init(void)
{
    LCD_DDDR = 0xFF;
    LCD_CDDR = 0xFF;
    LCD_CPRT &= ~(1 << LCD_EN);     // EN = 0
    lcdCommand(0x33);              // init for 4-bit
    lcdCommand(0x32);              // init for 4-bit
    lcdCommand(0x28);              // 2 lines, 5x7 matrix
    lcdCommand(0x0E);              // display on, cursor on
    lcdCommand(0x01);              // clear LCD
    delay_us(2000);
    lcdCommand(0x06);             // shift cursor right
}

void lcd_gotoxy(unsigned char x, unsigned char y)
{
    unsigned char firstCharAdr[] = {0x80, 0xC0, 0x94, 0xD4};
    lcdCommand(firstCharAdr[y - 1] + x - 1);
    delay_us(100);
}

void lcd_print(char *str)
{
    unsigned char i = 0;
    while (str[i] != 0)
    {
        lcdData(str[i]);
        i++;
    }
}

int main(void)
{
    lcd_init();
    lcd_gotoxy(1, 1);
    lcd_print("Chicken pizza is");
    lcd_gotoxy(1, 2);
    lcd_print("amazing!");
    while (1);                      // stay here forever
    return 0;
}