
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/power.h>

#include "pinDefines.h"
#include "USART.h"
#include "talkingVoltmeter.h"

void start_sample_timer(void)
{
    sample_number = 0;
    TCCR2B |= (1 << CS21);  // Divide by 8 prescalar
    // This results in 8kHz on 8MHz system
}

void stop_sample_timer(void)
{
    TCCR2B = 0; // Disable the clock
    OCR0A = 128;    // Stop halfway
    last_out = 0;   // Begin at 0 next time
}

void speak(void)
{
    start_sample_timer();
    loop_until_bit_is_clear(TCCR2B, CS21);  // Wait until done
}

void update_PWM_audio(void)
{
    OCR0A = out + 128;  // Re-center to be less than 8-bit timer range
    last_out = out; // Current value becomes old value
    sample_number++;
}

// Save the bits in pairs beginning from MSB
// I can store the mask as a variable
// At the end the elements of differentials contains a 2-bit number ranging from zero - three.
void unpack_byte(uint8_t data_byte)
{
    differentials[0] = (data_byte >> 6) & 0b00000011;
    differentials[1] = (data_byte >> 4) & 0b00000011;
    differentials[2] = (data_byte >> 2) & 0b00000011;
    differentials[3] = (data_byte & 0b00000011);
}

// Timer 2 controls sampling speed. ISR reads new data and loads OCR0A
// This is where we actually ACCESS the audio data
ISR(TIMER2_COMPA_vect)
{
    // Retain last 2 bits
    uint8_t cycle = sample_number & 0b00000011; // Can either be 0, 1, 2, 3 depending on active sample
    uint16_t table_entry;
    uint8_t packed_data;

    if (cycle == 0)
    {
        table_entry = sample_number >> 2;   // Divide by 4 to determine which byte of the audio array to unpack next.
        if (table_entry < this_table_length) // If we're within the speech array
        {
            packed_data = pgm_read_byte(&this_table_ptr[table_entry]); // read out the byte
            unpack_byte(packed_data);   // Decode the data
        }
        else 
        {
            stop_sample_timer();    // We're at the end of the table. Finish it
        }
    }
    // Index the array using another array
    // Decode the difference since we're using differential PCM. We store the differences between 2 samples
    // Current value = last + difference
    out = last_out + dpcm_weights[differentials[cycle]] - (last_out >> 4);
    update_PWM_audio(); // Send out to OCR0A. 
}

// Alternative to USART:printString();
void print_string_progmem(const char* string_p)
{
    char one_letter;
    while ( (one_letter = pgm_read_byte(string_p)) )
    {
        transmitByte(one_letter);
        string_p++;
    }
}

int main(void)
{
    // This is the VCC voltage * 10. E.g. 5.08V = 5.8V = 51
    uint8_t vcc = 51;   // 10x VCC, in volts

    clock_prescale_set(clock_div_1);    // 8MHz

    // Inits
    init_timer_0();
    init_timer_2();
    sei();  // For timer 2 ISR
    init_adc();
    initUSART();

    print_string_progmem( PSTR("\r\n--=( Talking Voltmeter )=--\r\n") );

    // Sample message 
    select_table(INTRO);
    speak();

    while (1)
    {
        ADCSRA |= (1 << ADSC);
        loop_until_bit_is_clear(ADCSRA, ADSC);
        
        // ADC Value = (Vin * 1024) / Vcc
        // Vin = (ADC Value * Vcc) / 1024
        // Vin(x10)    = (ADC Value * 51 ) / 1024
        uint16_t voltage = ADC * vcc + vcc/2;   // vcc/2 required to make rounding work
        voltage = voltage >> 10;    // This is equivalen to dividing by 10-bits.
        
        uint8_t volts = voltage / 10;
        uint8_t tenths = voltage % 10;
        
        // Display via UART
        transmitByte('0' + volts);
        select_table(volts);
        speak();
        
        transmitByte('.');
        select_table(POINT);
        speak();
        
        transmitByte('0' + tenths);
        select_table(tenths);
        speak();
        
        print_string_progmem( PSTR("  volts\r\n") );
        select_table(VOLTS);
        speak();
        
        _delay_ms(SPEECH_DELAY);
    }
    return 0;
}