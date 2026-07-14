#include <avr/io.h>
#include <util/delay.h>

int main(void) {
    DDRB = 0xFF; // all pins output

    while (1) {
        for (uint8_t i = 0; i < 8; i++) {
            PORTB = (1 << i);
            _delay_ms(400);
        }
    }
}
