
// Stepper motor using Trapezoid_move();

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/power.h>		// For clock_prescale_set();


#include "USART.h"

// +/- 1 = Half-Stepping, +/- 2 = Full-Stepping

// For half-stepping we want greater granularity, thus the "step-size" of it
// is smaller than full-stepping, 1 < 2

#define FORWARD      1
#define BACKWARD    -1      // Negative means reversing the windings. Thus, the motor reverses direction
#define TURN        400     // Steps per revolution. Varies per motor

// Movement parameters
#define MAX_DELAY       250     // This is the initial delay for the first step. Also used in RAMP_STEP formula
#define MIN_DELAY       112     // Delay between steps. This gives time for shaft to go to new position
#define ACCELERATION    12      // Lower = smoother acceleration

#define RAMP_STEPS (MAX_DELAY - MIN_DELAY) / ACCELERATION   // used in trapezoid_move();

// GLOBALS BABY!
// Store values we need to drive the coil currents in an array

// One coil is connected to PB0, PB1
// The other coil is connected to PB2, PB3

// Energizing both coils = Full-Stepping
// When only one coil is energized, this is half-stepping
// There are 8 combinations we can energize the windings. This is the coil energization sequence
// To determine which coil to magnetize and which coil end, we'll simply loop through this array

const uint8_t motor_phases[] = 
{
    (1 << PB0) | (1 << PB2),    // Full
    (1 << PB0),                 // Half
    (1 << PB0) | (1 << PB3),    // etc.
    (1 << PB3),
    (1 << PB1) | (1 << PB3),
    (1 << PB1),
    (1 << PB1) | (1 << PB2),
    (1 << PB2)
};

volatile uint8_t step_phase = 0;        // Stores location of current step
volatile int8_t direction = FORWARD;    // 
    // NOT, It's not an enumeration. It's a preprocessor macro. It's just strange seeing a string being assigned to an int
volatile uint16_t step_counter = 0;     // Keeps track of total steps taken

void init_timer(void)
{
    TCCR0A |= (1 << WGM01);     // Clear Timer on Compare Match (CTC) Mode
        // WGM01 = Waveform Generation Mode 
            // This bit configures waveform generation mode. Either count to TOP, or count to output compare value
            // By setting WGM01, Timer 0 will count to OCR0A then repeat
        // TCCROA = Timer Counter/Control Register A
    
    TCCR0B |= (1 << CS00) | (1 << CS02);    // F_CPU scaled down by 1024x
        // CS0x = Clock Select bit
    OCR0A = MAX_DELAY;  // Set initial speed as max speed
    sei(); // Enable global interrupts;
    // NOTE: We don't enable interrupt flag of the timer. We'll do this in take_steps(). I believe the reason
    // is we want to repeatedly enable/disable interrupts
}

// This ISR is triggered when an output match is match
ISR(TIMER0_COMPA_vect)
{
    step_phase += direction;            // Perform a step in whatever direction (either forward/backward)
    step_phase &= 0b00000111;           // Constrict to 0 - 7 so we can index our array, motor_phases. Also, it prevents overflows. 
    PORTB = motor_phases[step_phase];  // Energize H-Bridges with coil config
    step_counter++;                     // Record-keeping
}

void take_steps(uint16_t how_many_steps, uint8_t delay)
{
    UDR0 = delay;   // Transmit delay over UART for the lolz
    OCR0A = delay;  // Set speed of steps
    step_counter = 0;

    TIMSK0 |= (1 << OCIE0A); // Enable interrupts for output compare match A
        // TIMSK0 = Timer Interrupt Mask Register
        // OCIE0A = Output Compare Match Interrupt Enable bit 

    // Keep firing ISR's until this condition is met. Thus, we continue stepping. 
    while (!(step_counter == how_many_steps)) {}; 

    TIMSK0 &= ~(1 << OCIE0A); // Disable interrupts, thereby shutting down the motor
}

// Accepts negative steps for backward rotation
void trapezoid_move(int16_t how_many_steps)
{
	uint8_t delay = MAX_DELAY;
	uint16_t steps_taken = 0;

	if (how_many_steps > 0) { direction = FORWARD; }
	else
	{
		direction = BACKWARD;
		how_many_steps = -how_many_steps; // Invert the steps
	}
	
	if (how_many_steps > (RAMP_STEPS * 2) )
	{
		// Acceleration stage
		while (steps_taken < RAMP_STEPS) 
		{
			take_steps(1, delay);
			delay -= ACCELERATION; // The first step has the largest dela,y then each successive step has a smaller delay, thereby ramping up the motherfu- I mean the stepper
			steps_taken++;
		}
		
		// Crusing stage
		delay = MIN_DELAY;
		take_steps( (how_many_steps - 2 * RAMP_STEPS), delay);
		steps_taken += (how_many_steps - 2 * RAMP_STEPS);
		
		// De-acceleration stage
		while (steps_taken < how_many_steps) 
		{
			take_steps(1, delay);
			delay += ACCELERATION; 	// Gradually increasing delay, thereby ramping down
			steps_taken++;
		}
	}
	
	else 
	{
		while (steps_taken <= how_many_steps / 2) 
		{
			take_steps(1, delay);
			delay -= ACCELERATION;
			steps_taken++;
		}
		
		delay += ACCELERATION;
		
		while (steps_taken < how_many_steps) 
		{
			take_steps(1, delay);
			delay += ACCELERATION;
			steps_taken++;
		}
	}
	
}

int main(void)
{
    // Inits
	clock_prescale_set(clock_div_1);	// CPU clock 8 MHz
    initUSART();
    _delay_ms(1000);
    init_timer();
    
    // Set control signals as OUTPUT
    // I'll replace this with variables in the extension
    DDRB = (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3);

    // Event loop
    while (1)
    {
		printString("Performing 2 forward revolutions\r\n");
		trapezoid_move(2 * TURN);
		_delay_ms(2000);
		
		printString("Performing a 180 backward rotation\r\n");
		trapezoid_move(-TURN / 2);
		_delay_ms(2000);
		
		printString("Performing a quarter forward rotation\r\n");
		trapezoid_move(TURN / 4);
		_delay_ms(3500);

		
		printString("Performing an eighth turn in reverse\r\n");
		trapezoid_move(-TURN / 8);                             
		_delay_ms(3500);

		// Repeat showcase in the opposite direction
		printString("Performing a backward quarter rotation\r\n");
		trapezoid_move(-TURN / 4);  
		_delay_ms(2000);
        
		printString("Perform an eigth forward rotation\r\n");
		trapezoid_move(TURN / 8);
		_delay_ms(2000);
		
		printString("Perform a forward rotation for 180 degrees\r\n");
		trapezoid_move(TURN / 2);                
		_delay_ms(2000);

    }
	return 0;
}

