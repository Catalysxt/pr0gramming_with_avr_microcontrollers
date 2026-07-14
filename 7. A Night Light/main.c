#include <avr/io.h>
#include <util/delay.h>

#include "pinDefines.h"
#include "USART.h"

// This function allows us to read any channel
uint16_t read_adc(uint8_t channel)
{
    ADMUX = (0xf0 & ADMUX) | channel; // Select desired channel
    ADCSRA |= (1 << ADSC);  // Begin conversion
    loop_until_bit_is_clear(ADCSRA, ADSC);
    return (ADC);
}

int main(void)
{
    // Configure ADC
    ADMUX |= (1 << REFS0);  // Configure voltage reference as AVcc
    ADCSRA |= (1 << ADPS1) | (1 << ADPS0);  // Divide clock by 8. 125kHz sampling frequency
    ADCSRA |= (1 << ADEN);  // Turn on the darn thing

    LED_DDR = 0xff;

    while (1)
    {
        // These 2 arguments are defined in PinDefines.h POT = PC3, LIGHT_SENSOR = PC0
        // We use a single function to read voltage of 2 separate pins. Nice
		
        uint16_t light_threshold = read_adc(POT); 
        uint16_t sensor_value = read_adc(LIGHT_SENSOR);

        // Trigger LEDs
        if (sensor_value < light_threshold) { LED_PORT = 0xff; }
        else { LED_PORT = 0x00; }
		
		// Debugging
	    printString("Threshold: ");
        printWord(light_threshold);
        printString("|  Sensor: ");
        printWord(sensor_value);
        printString("\r\n");
    }

    return 0;
}