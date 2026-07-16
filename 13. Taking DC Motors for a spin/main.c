
// PWM Control of a DC Motor

// Libraries included as part of avr-gcc toolchain
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// Custom libraries
#include "pinDefines.h"
#include "USART.h"

// Control how rapidly/slowly speed adjusts
#define SPEED_DELAY 20                             /* milliseconds */

static inline void init_timer_0(void)
{
	// TCCR0A = Timer/Counter Control Register A
	// This register controls the Output Compare Pin (OC0n) behavior 
	// WGM = Waveform Generation Mode
	
	TCCR0A |= (1 << WGM00);	// Fast PWM mode (page 108 of datasheet)
	TCCR0A |= (1 << WGM01);    // Also, fast PWM mode
	TCCR0A |= (1 << COM0B1);   // COM0B1 = Compare Match Output B Mode
	
	// TCCR0B = Timer/Counter Control Register B
	// CS02 = Clock Select bit
	
	TCCR0B |= (1 << CS02); // Scale down F_CPU by 256
	 
}


int main(void) {

  uint8_t update_speed = 0;

  // -------- Inits --------- //
  init_timer_0();
  OCR0B = 0; // PD5 has an alternative function of OC0B
  // It's the Timer/Counter 0 Output Compare Match B Output
  // TCNT0 is compared to value in OCR0B. If there is a match, timer resets from bottom.
  // Waveform is transmitted to OC0B.
  
  ANTENNA_DDR |= (1 << ANTENNA);	// We actually have the FET connected to PD5, not ANTENNA lol
  
  // Indicator LEDs indicating if we're increasing or decreasing speed
  LED_DDR |= (1 << LED0);
  LED_DDR |= (1 << LED1);

  initUSART();
  printString("DC Motor Showcase!\r\n");

  // ------ Event loop ------ //
  while (1) {

    update_speed = getNumber(); // Fetch a 0-255 number via UART
		
	// Depending on requested speed, we ramp up or down the speed

    if (OCR0B < update_speed)
	{
		printString("Increasing speed!\r\n");
		LED_PORT |= (1 << LED0); 
		
		// If we haven't reached the speed, keep incrementing till we do
		while (OCR0B < update_speed)
		{
			OCR0B++;
			_delay_ms(SPEED_DELAY);
      }
    }
	
    else
	{
		printString("Decreasing speed!\r\n");
		LED_PORT |= (1 << LED1);
		while (OCR0B > update_speed)
		{
			OCR0B--; // Decrement to match new requested speed
			_delay_ms(SPEED_DELAY);
		}
    }
    LED_PORT = 0;                                           /* all off */

  }                                                  /* End event loop */
  return 0;                            /* This line is never reached */
}