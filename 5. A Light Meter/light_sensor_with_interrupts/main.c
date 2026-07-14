#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>  // Required for ISR and sei()

#include "pinDefines.h"
#include "USART.h"

// Volatile variables are necessary because they are modified inside an ISR
volatile uint16_t adc_value = 0;
volatile uint8_t new_data_ready = 0;

static inline void init_adc0(void)
{
    ADMUX |= (1 << REFS0);  // Configure voltage reference to AVcc
    // PC0 is selected by default since MUX3:0 are all 0

    // ADCSRA settings:
    // ADEN  = Enable ADC
    // ADIE  = Enable ADC Interrupt
    // ADATE = Enable ADC Auto Trigger (Free Running Mode by default when ADCSRB is 0)
    // ADPS1 | ADPS0 = Prescaler of 8
    ADCSRA |= (1 << ADEN)  | 
              (1 << ADIE)  | 
              (1 << ADATE) | 
              (1 << ADPS1) | (1 << ADPS0);

    // ADCSRB bits ADTS2:0 are 000 by default for Free Running Mode
    ADCSRB &= ~((1 << ADTS2) | (1 << ADTS1) | (1 << ADTS0));

    sei(); // Enable global interrupts

    ADCSRA |= (1 << ADSC);  // Start the very first conversion to kick off the free-running loop
}

// Interrupt Service Routine for ADC conversion complete
ISR(ADC_vect)
{
    adc_value = ADC;       // Read the 10-bit ADC value
    new_data_ready = 1;    // Flag to signal main loop that data is updated
}

int main(void)
{
    init_adc0();
    initUSART();
    LED_DDR = 0xff;

    // Event loop
    while (1)
    {
        // Only update the display and USART if a new conversion has finished
        if (new_data_ready)
        {
            new_data_ready = 0; // Reset flag

            uint8_t led_value = (adc_value >> 7); // 10-bit value down to 0-7 scale
            uint8_t adc_byte = (adc_value >> 2);  // Scale 10-bit down to 8-bit byte

            // Display the ADC value on LEDs
            LED_PORT = 0;
            for (int i = 0; i <= led_value; i++) 
            { 
                LED_PORT |= (1 << i); 
            }

            printByte(adc_byte);
            printString("\r\n");
            
            // Note: In a pure interrupt architecture, we avoid long delays inside while(1) 
            // if other real-time tasks need to run. However, it is kept here to match your 
            // original serial output cadence.
            _delay_ms(100);
        }
    }
}
