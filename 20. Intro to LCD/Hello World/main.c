/*
 * HD44780 LCD driver in 8-bit mode for ATmega328P.
 *
 * Data bus D0-D7 is on PORTD; control lines RS, RW, EN are on PORTB
 * bits 0, 1, 2. Each transfer places a byte on the data port, sets RS
 * (0 = command, 1 = data) and RW = 0, then latches it with an EN
 * high-to-low pulse.
 *
 * lcd_gotoxy() uses 1-based (x, y) coordinates, mapping rows to the
 * DDRAM base addresses 0x80, 0xC0, 0x94, 0xD4. The program initialises
 * the display and prints a two-line message.
 */

#include <avr/io.h>             // standard AVR header
#include <util/delay.h>        // delay header

// LCD data port
// D0-D7 is connected on Port D
#define LCD_DPRT PORTD          
#define LCD_DDDR DDRD
#define LCD_DPIN PIND

// LCD command port
#define LCD_CPRT PORTC          
#define LCD_CDDR DDRC
#define LCD_CPIN PINC

#define LCD_RS 0 // PC0
#define LCD_RW 1 // PC1
#define LCD_EN 2 // PC2

// _delay_us() is a macro, not a function
#define delay_us(d) _delay_us(d)

void lcdCommand(unsigned char cmnd)
{
    LCD_DPRT = cmnd;                // send cmnd to data port
    LCD_CPRT &= ~(1 << LCD_RS);     // RS = 0 for command
    LCD_CPRT &= ~(1 << LCD_RW);     // RW = 0 for write
    LCD_CPRT |=  (1 << LCD_EN);     // EN = 1
    delay_us(1);
    LCD_CPRT &= ~(1 << LCD_EN);     // H-to-L pulse
    delay_us(100);
}

void lcdData(unsigned char data)
{
    LCD_DPRT = data;               // send data to data port
    LCD_CPRT |=  (1 << LCD_RS);     // RS = 1 for data
    LCD_CPRT &= ~(1 << LCD_RW);     // RW = 0 for write
    LCD_CPRT |=  (1 << LCD_EN);     // EN = 1
    delay_us(1);
    LCD_CPRT &= ~(1 << LCD_EN);     // H-to-L pulse
    delay_us(100);
}

void lcd_init(void)
{
    LCD_DDDR = 0xFF;
    LCD_CDDR = 0xFF;
    LCD_CPRT &= ~(1 << LCD_EN);     // EN = 0
    delay_us(2000);                // wait for power on
    lcdCommand(0x38);              // 2 lines, 5x7 matrix
    lcdCommand(0x0E);              // display on, cursor on
    lcdCommand(0x01);              // clear LCD
    delay_us(2000);
    lcdCommand(0x06);             // shift cursor right
}

void lcd_gotoxy(unsigned char x, unsigned char y)
{
    char firstCharAdr[] = {0x80, 0xC0, 0x94, 0xD4};
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
    lcd_print("The world is but");
    lcd_gotoxy(1, 2);
    lcd_print("one country");
    while (1);                      // stay here forever
    return 0;
}