
#include <avr/io.h>
#include <util/delay.h>
#include <avr/power.h>

#include "pinDefines.h"
#include "USART.h"
#include "i2c.h"

// LM75 I2C address: 7-bit base = 0b1001000 (0x48), A2=A1=A0=GND → 0x48.
// 8-bit write address = 0x48 << 1       = 0x90
// 8-bit read  address = (0x48 << 1) | 1 = 0x91
// LM75 datasheet (TI SNOSC16D) p. 10 — slave address table
#define LM75_ADDRESS_W  0x90
#define LM75_ADDRESS_R  0x91

// LM75 internal register pointer values — datasheet p. 15, Table 3
#define LM75_TEMP_REGISTER      0x00
#define LM75_CONFIG_REGISTER    0x01
#define LM75_THYST_REGISTER     0x02
#define LM75_TOS_REGISTER       0x03

static void print_uint8(uint8_t val) {
    if (val >= 100) { transmitByte('0' + val / 100); }
    if (val >= 10)  { transmitByte('0' + (val / 10) % 10); }
    transmitByte('0' + val % 10);
}

int main(void)
{
    uint8_t temp_high_byte = 0;
    uint8_t temp_low_byte = 0;

    // INITS
    clock_prescale_set(clock_div_1);    // Prescaler = 1 (no division). With LFUSE=0xF7 (external 16 MHz crystal), CPU runs at 16 MHz.
                                        // avr/power.h — clock_prescale_set()

    initUSART();
    printString("\r\n==== i2c Thermometer ====\r\n");
    init_I2C();

    while (1)
    {
        i2c_start();                    // 1. Generate start condition
        i2c_send(LM75_ADDRESS_W);       // 2. Send address
        i2c_send(LM75_TEMP_REGISTER);   // 3. 

        // Restart by sending START again
        i2c_start();    

        // Request a read (rather than write)
        i2c_send(LM75_ADDRESS_R);

        // Receive 2 bytes of temperature
        temp_high_byte = i2c_read_ack();    // According to datasheet, this is MSB
        temp_low_byte = i2c_read_no_ack();  // We have the data we want, thus send a NACK to signal end of communication
            // According to datasheet, LSB is = 0.5 degrees Celsuis. This explains the if/else below. 
				i2c_stop();

        // Print the temp over UART
        print_uint8(temp_high_byte);
        if (temp_low_byte & _BV(7)) // LM75 datasheet p.6 — bit 7 of low byte = 0.5 °C
        {
            printString(".5 degrees\r\n");
        }
        else
        {
            printString(".0 degrees\r\n");
        }

        _delay_ms(3000);
    }
    return 0;
}