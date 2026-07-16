#include <avr/io.h>

#include "pinDefines.h"
#include "USART.h"
#include "24LC256.h"

int main(void)
{
    uint8_t address;
    uint8_t i;

    // Inits
    init_I2C();
    initUSART();

    while (1)
    {
        printString("\r\n==== EEPROM Memory Playground ====\r\n");
        printString("Address Value \r\n"); 

        // Print out first 10 bytes in memory
        for (i = 0; i < 10; i++)
        {
            printString(" ");
            printByte(i);
            printString(":   ");
            printByte(EEPROM_read_byte(i)); // EEPROM_read_byte accepts an address
            printString("\r\n");
        }

        // UI/Navigation
        printString(" [e] to erase all memory\r\n");
        printString(" [w] to write byte to memory\r\n");

        // Process user input
        switch (receiveByte())
        {
            case 'e':
                printString("Clearing EEPROM. Please wait.\r\n");
                EEPROM_clear_all();  
                break;
            
            case 'w':
                printString("Please select the memory address to write to:");
                address = getNumber();

                printString("\r\nEnter the number you'd like to store: ");
                i = getNumber();
                
                EEPROM_write_byte(address, i);
                printString("\r\n");
                break;
            default:
                printString("You say what?\r\n");
        }
    }
    return 0;
}