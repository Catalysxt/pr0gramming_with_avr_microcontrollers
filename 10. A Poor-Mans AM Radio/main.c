
// Generates a simple tune and broadcasts it to AM radio band 1MHz

#include <avr/io.h> /* Defines pins, ports, etc */
#include <util/delay.h> /* Functions to waste time */
#include <avr/power.h>
#include <avr/interrupt.h>

#include "pinDefines.h"
#include "scale16.h"

#define COUNTER_VALUE   3 // Determines carrier frequency. See equation below
// We can tune the AM transmitter to 6 bands

// f_carrier = F_CPU / (2 * N * (1 + OCRnX) )
// We divide by 2 since we're toggling on/off each loop
// A full cycle/period of the carrier takes 2 loops

// 8Mhz / (2 * 1 * (1+2)) = 1333 kHz
// 8Mhz / (2 * 1 * (1+3)) = 1000 kHz
// 8Mhz / (2 * 1 * (1+4)) = 800 kHz
// 8Mhz / (2 * 1 * (1+5)) = 670 kHz
// 8Mhz / (2 * 1 * (1+6)) = 570 kHz
// 8Mhz / (2 * 1 * (1+7)) = 500 kHz

// Timer 0 generates the carrier signal at 1MHz. 
static inline void init_timer_0(void)
{
    TCCR0A |= (1 << WGM01); // Set to CTC Compare Match Mode. Timer will count to OCRA then reset then continue
    TCCR0A |= (1 << COM0B0);    // Toggle OCOB on a compare match event. I.e. connect an output compare match event to an I/O
        // COM0B0 = Compare Match Output B mode bit
    TCCR0B |= (1 << CS00);  // No prescaling. Thus, f_{OCnx} = 1MHz
		// f_{OCnx} = 1 / [2 * N * (1+OCRnx)]
    OCR0A = COUNTER_VALUE;  // f_carrier
}

// Generates the signal superimposed on carrier
static inline void init_timer_1(void)
{
    TCCR1B |= (1 << WGM12); // Configure to Mode 4. CTC Mode, triggering a compare match on OCR1A
        // WGM = Waveform Generate Mode Bit
    TCCR1B |= (1 << CS11);  // Prescale by /8. Thus, 1MHz
    TIMSK1 |= (1 << OCIE1A);    // Enable interrupts on output compare match
}

ISR(TIMER1_COMPA_vect)
{
    ANTENNA_DDR ^= (1 << ANTENNA);   // Toggles the pin (PD5)
}

static inline void transmit_beep(uint16_t pitch, uint16_t duration)
{
    OCR1A = pitch;  // Change signal by writing to Output Compare Register
    sei();  // Enable interrupts
    do
    {
        _delay_ms(1);
        duration--;
    } while (duration > 0);
    cli();  // Disable interrupts to halt toggling
    ANTENNA_DDR |= (1 << ANTENNA);
}

int main(void)
{
    clock_prescale_set(clock_div_1);    // 8MHz clock
    init_timer_0();
    init_timer_1();

    // Transmit the tune
    while (1)
    {
        transmit_beep(E3, 200);
        _delay_ms(100);

        transmit_beep(E3, 200);
        _delay_ms(200);

        transmit_beep(E3, 200);
        _delay_ms(200);
        
        transmit_beep(C3, 200);
        transmit_beep(E3, 200);
        _delay_ms(200);

        transmit_beep(G3, 400);
        _delay_ms(500);

        transmit_beep(G2, 400);

        _delay_ms(2599);

    }
    return 0;
}
