
// H-Bridge Demo

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "pinDefines.h"

// Hardware connections
// Bridge A:
	// LED0 = PB0
	// One FET = PD6
// Bridge B:
	// Other FET = PD5
	// LED1 = PB1

// Used to control the motor
static inline void set_bridge_state(uint8_t bridge_a, uint8_t bridge_b)
{
	
	if (bridge_a) // Trigger the chosen half of the h-bridge
	{
		PORTD |= (1 << PD6);
		LED_PORT |= (1 << LED0);
	}
	
	else 
	{
		PORTD &= ~(1 << PD6);
		LED_PORT &= ~(1 << LED0);
	}
	
	if (bridge_b) // Trigger the chosen half of the h-bridge
	{
		PORTD |= (1 << PD5);
		LED_PORT |= (1 << LED1);
	}
	
	else 
	{
		PORTD &= ~(1 << PD5);
		LED_PORT &= ~(1 << LED1);
	}	
	
}

int main(void)
{
	// Inits
	DDRD |= (1 << PD6);
	DDRD |= (1 << PD5);
	
	LED_DDR |= (1 << LED0);
	LED_DDR |= (1 << LED1);
	
	while (1) 
	{
		// Forward
		set_bridge_state(1,0); 
		_delay_ms(2000);
		
		// Halt
		set_bridge_state(0,0); 
		_delay_ms(2000);
		
		// Backward/Reverse
		set_bridge_state(0,1); 
		_delay_ms(2000);
		
		// Halt
		set_bridge_state(1,1); 
		_delay_ms(2000);
		
		// Let's rapidly brake the motor
		set_bridge_state(1,0);
		_delay_ms(2000);
		
		set_bridge_state(0, 1);
		_delay_ms(75);              // Adjust this to suit your setup
		
		set_bridge_state(0, 0);
		_delay_ms(2000);
		
	}
	return 0;
}