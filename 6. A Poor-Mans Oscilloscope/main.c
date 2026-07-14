#include <avr/io.h>
#include <util/delay.h>

#include "pinDefines.h"
#include "USART.h"

#define SAMPLE_DELAY 100 // ms. Controls update speed of the slow scope

static inline void init_free_running_adc(void)
{
    ADMUX |= (1 << REFS0);  // Set voltage reference to AVcc
    ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);  // Divide clock by 128 for 16MHz / 128 = 125kHz sampling frequency
    ADMUX |= (1 << ADLAR);  // If set, it occupies ADCH with 8 bits, ADCL with 2 bits.
        // ADLAR = ADC Left Adjustment Result Bit
    ADCSRA |= (1 << ADEN);  // Enable the ADC boi
    ADCSRA |= (1 << ADATE); // Enable auto-triggering. ADC trigger a new conversion once existing one is finished.
        // We have to manually trigger first one though. 
        // ADATE = ADC Auto Trigger Enable Bit
    // Usually you need to configure the trigger source. However, conveniently a zero on trigger source bits
    // configures the source to free-running mode. Other options include trigger on timer compare match or analog comparator
    ADCSRA |= (1 << ADSC); // Begin the conversion
}

int main(void)
{
    initUSART();
    init_free_running_adc();

    while (1)
    {
		transmitByte(ADCH);
		transmitByte('\r');
		transmitByte('\n');
        _delay_ms(SAMPLE_DELAY);
    }
    return 0;
}