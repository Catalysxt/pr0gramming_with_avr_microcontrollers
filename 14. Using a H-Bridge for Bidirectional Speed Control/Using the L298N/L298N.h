#ifndef L298_H
#define L298_H

#include <avr/io.h>

typedef enum {
    DIR_FORWARD,
    DIR_BACKWARD
} motor_direction_t;

typedef struct {
    uint8_t speed;              /* 0-255 magnitude */
    motor_direction_t direction;
} motor_state_t;

/* current state of each motor */
extern motor_state_t motor_a_state;
extern motor_state_t motor_b_state;

void motors_init(void);

void motor_a_set(motor_direction_t dir, uint8_t speed);
void motor_b_set(motor_direction_t dir, uint8_t speed);

void motor_a_stop(void);       /* active brake (both inputs high) */
void motor_b_stop(void);

void motor_a_enable(void);     /* EN high */
void motor_a_coast(void);      /* EN low -> free-running stop */

#endif /* L298_H */
