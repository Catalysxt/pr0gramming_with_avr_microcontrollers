#include <avr/io.h>
#include <util/delay.h>

#include "pinDefines.h"
#include "USART.h"

static inline void init_adc0(void)
{
    ADMUX |= (1 << REFS0);  // Configure voltage reference to AVcc
        // ADMUX = ADC Multiplexer Selection Register
        // REFS0 = Reference Selection Bit 0
    ADCSRA |= (1 << ADPS1) | (1 << ADPS0);  // Divide F_CPU by 8
        // ADCSRA = ADC Control and Status Register A
        // ADPSx = ADC Prescaler Select bits
    ADCSRA |= (1 << ADEN);  // Turn that big boy on
        // ADEN = ADC Enable bit
}

int main(void)
{
    init_adc0();
    initUSART();
    LED_DDR = 0xff;

    // Event loop
    while (1)
    {
        ADCSRA |= (1 << ADSC);  // Manually trigger ADC conversion. This sets ADC to Single Conversion mode
            // ADSC = ADC Start Conversion
        loop_until_bit_is_clear(ADCSRA, ADSC);  // Patiently wait for ADC conversion to finish
        uint16_t adc_value = ADC;

        uint8_t led_value = (adc_value >> 7); // ADC is a 10-bit value. We only want 8-bits for 8 LEDs
        uint8_t adc_byte = (adc_value >> 2); // Trim 10-bit value into a byte. I.e. drop the 2 MSB's.
        
        // Display the ADC value on LEDs
        LED_PORT = 0;
        for (int i  = 0; i <= led_value; i++) { LED_PORT |= (1 << i); }
        printByte(adc_byte);
        printString("\r\n");
        
        _delay_ms(150);
    }
}
