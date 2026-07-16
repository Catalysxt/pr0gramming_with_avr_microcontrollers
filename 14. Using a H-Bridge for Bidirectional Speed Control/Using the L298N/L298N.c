/* L298N dual H-bridge driver for ATmega328p.
 *
 * Sign-magnitude PWM: one bridge input is held static high, the other is
 * PWM-ed. Drive occurs on the PWM input's LOW phase, so OCR is loaded as
 * (255 - speed) to keep higher speed = faster.
 *
 * Pin/timer mapping is fixed by hardware: each PWM input must sit on its
 * timer's OCnx pin. Edit the macros only to match wiring of the SAME OC
 * pins; a PWM input on a non-OC pin will not work.
 *
 * Motor A -> Timer0 : IN1=OC0A (PD6), IN2=OC0B (PD5)
 * Motor B -> Timer1 : IN3=OC1A (PB1), IN4=OC1B (PB2)
 */

#include <avr/io.h>
#include "L298N.h"

/* ---- Hardware configuration ---- */
#define MOTOR_A_IN1_PORT  PORTD
#define MOTOR_A_IN1_DDR   DDRD
#define MOTOR_A_IN1       PD6        /* OC0A */
#define MOTOR_A_IN2_PORT  PORTD
#define MOTOR_A_IN2_DDR   DDRD
#define MOTOR_A_IN2       PD5        /* OC0B */
#define MOTOR_A_EN_PORT   PORTB
#define MOTOR_A_EN_DDR    DDRB
#define MOTOR_A_EN        PB5        /* ENA (also SCK - glitches under ISP) */

#define MOTOR_B_IN3_PORT  PORTB
#define MOTOR_B_IN3_DDR   DDRB
#define MOTOR_B_IN3       PB1        /* OC1A */
#define MOTOR_B_IN4_PORT  PORTB
#define MOTOR_B_IN4_DDR   DDRB
#define MOTOR_B_IN4       PB2        /* OC1B */
#define MOTOR_B_EN_PORT   PORTB
#define MOTOR_B_EN_DDR    DDRB

motor_state_t motor_a_state;
motor_state_t motor_b_state;

void motors_init(void) {
    /* bridge inputs as outputs */
    MOTOR_A_IN1_DDR |= (1 << MOTOR_A_IN1);
    MOTOR_A_IN2_DDR |= (1 << MOTOR_A_IN2);
    MOTOR_B_IN3_DDR |= (1 << MOTOR_B_IN3);
    MOTOR_B_IN4_DDR |= (1 << MOTOR_B_IN4);

    /* enables as outputs, enabled */
    MOTOR_A_EN_DDR |= (1 << MOTOR_A_EN);
    motor_a_enable();

    /* Timer0: fast PWM 8-bit, prescaler 64 (~976 Hz @ 16 MHz) */
    TCCR0A = (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS01) | (1 << CS00);

    /* Timer1: fast PWM 8-bit (mode 5), prescaler 64 */
    TCCR1A = (1 << WGM10);
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);

    motor_a_stop();
    motor_b_stop();
    motor_a_state.direction = DIR_FORWARD;
    motor_b_state.direction = DIR_FORWARD;
}

void motor_a_set(motor_direction_t dir, uint8_t speed) {
    uint8_t ocr = 255 - speed;             /* drive on PWM low phase */
    motor_a_state.direction = dir;
    motor_a_state.speed = speed;

    if (dir == DIR_FORWARD) {              /* IN1 high, PWM IN2 */
        TCCR0A &= ~((1 << COM0A1) | (1 << COM0A0));
        MOTOR_A_IN1_PORT |= (1 << MOTOR_A_IN1);
        TCCR0A |= (1 << COM0B1);
        OCR0B = ocr;
    } else {                               /* IN2 high, PWM IN1 */
        TCCR0A &= ~((1 << COM0B1) | (1 << COM0B0));
        MOTOR_A_IN2_PORT |= (1 << MOTOR_A_IN2);
        TCCR0A |= (1 << COM0A1);
        OCR0A = ocr;
    }
}

void motor_b_set(motor_direction_t dir, uint8_t speed) {
    uint8_t ocr = 255 - speed;
    motor_b_state.direction = dir;
    motor_b_state.speed = speed;

    if (dir == DIR_FORWARD) {              /* IN3 high, PWM IN4 */
        TCCR1A &= ~((1 << COM1A1) | (1 << COM1A0));
        MOTOR_B_IN3_PORT |= (1 << MOTOR_B_IN3);
        TCCR1A |= (1 << COM1B1);
        OCR1B = ocr;
    } else {                               /* IN4 high, PWM IN3 */
        TCCR1A &= ~((1 << COM1B1) | (1 << COM1B0));
        MOTOR_B_IN4_PORT |= (1 << MOTOR_B_IN4);
        TCCR1A |= (1 << COM1A1);
        OCR1A = ocr;
    }
}

void motor_a_stop(void) {                  /* both inputs high = brake */
    TCCR0A &= ~((1 << COM0A1) | (1 << COM0A0) | (1 << COM0B1) | (1 << COM0B0));
    MOTOR_A_IN1_PORT |= (1 << MOTOR_A_IN1);
    MOTOR_A_IN2_PORT |= (1 << MOTOR_A_IN2);
    motor_a_state.speed = 0;
}

void motor_b_stop(void) {
    TCCR1A &= ~((1 << COM1A1) | (1 << COM1A0) | (1 << COM1B1) | (1 << COM1B0));
    MOTOR_B_IN3_PORT |= (1 << MOTOR_B_IN3);
    MOTOR_B_IN4_PORT |= (1 << MOTOR_B_IN4);
    motor_b_state.speed = 0;
}

void motor_a_enable(void) { MOTOR_A_EN_PORT |=  (1 << MOTOR_A_EN); }
void motor_a_coast(void)  { MOTOR_A_EN_PORT &= ~(1 << MOTOR_A_EN); }
