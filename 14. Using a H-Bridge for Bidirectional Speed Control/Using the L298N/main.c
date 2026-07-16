
// PWM Control of a DC Motor

// Libraries included as part of avr-gcc toolchain
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// Custom libraries
#include "L298N.h"
#include "USART.h"
 
typedef void (*motor_set_fn)(motor_direction_t, uint8_t);
 
static void hold(motor_set_fn set, motor_direction_t dir,
                 uint8_t speed, uint16_t ms) {
    set(dir, speed);
    while (ms--) _delay_ms(1);          /* variable delay via 1 ms steps */
}
 
static void sweep(motor_set_fn set, motor_direction_t dir) {
    for (uint8_t s = 0; s <= 240; s += 10) { set(dir, s); _delay_ms(80); }
    for (uint8_t s = 240; s > 0;  s -= 10) { set(dir, s); _delay_ms(80); }
}
 
static void showcase(const char *name, motor_set_fn set, void (*stop)(void)) {
    printString(name);
    printString(": forward\r\n");
    hold(set, DIR_FORWARD, 200, 1500);
    stop();
    _delay_ms(500);
 
    printString(name);
    printString(": backward\r\n");
    hold(set, DIR_BACKWARD, 200, 1500);
    stop();
    _delay_ms(500);
 
    printString(name);
    printString(": forward sweep\r\n");
    sweep(set, DIR_FORWARD);
    stop();
    _delay_ms(500);
 
    printString(name);
    printString(": backward sweep\r\n");
    sweep(set, DIR_BACKWARD);
    stop();
    _delay_ms(500);
}
 
int main(void) {
    initUSART();
    motors_init();
    printString("L298N sign-magnitude showcase\r\n");
 
    while (1) {
        showcase("Motor A", motor_a_set, motor_a_stop);
        showcase("Motor B", motor_b_set, motor_b_stop);
    }
    return 0;                            /* never reached */
}