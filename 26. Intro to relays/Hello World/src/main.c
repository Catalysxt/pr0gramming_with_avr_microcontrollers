#include <avr/io.h>
#include <util/delay.h>

// Toggle relay every second

int main(void) {
    DDRB |= (1 << PB0);          // PB0 output

    while (1) {
        PORTB |= (1 << PB0);     // PB0 = 1
        _delay_ms(1000);
        PORTB &= ~(1 << PB0);    // PB0 = 0
        _delay_ms(1000);
    }
}