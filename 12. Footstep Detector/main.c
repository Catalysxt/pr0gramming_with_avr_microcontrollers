/*
 *      Sensitive footstep-detector and EWMA demo
 */

// ------- Preamble -------- //
#include <avr/io.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include "pinDefines.h"
#include "USART.h"

#define ON_TIME            2000                        /* milliseconds */
#define CYCLE_DELAY          10                        /* milliseconds */

#define INITIAL_PADDING  10                /* higher is less sensitive */

#define SWITCH              PB7     /* Attach LED or switch relay here */

#define USE_POT               1  /* define to 1 if using potentiometer */
#if USE_POT
#define POT               PC5                  /* optional padding pot */
#endif

// -------- Functions --------- //
void initADC(void) {
  ADMUX |= (1 << REFS0);                  /* reference voltage to AVCC */
  ADCSRA |= (1 << ADPS1) | (1 << ADPS2);    /* ADC clock prescaler /64 */
  ADCSRA |= (1 << ADEN);                                 /* enable ADC */
}

// This function accepts an pin configured for ADC
uint16_t readADC(uint8_t channel) {
  ADMUX = (0b11110000 & ADMUX) | channel;
  ADCSRA |= (1 << ADSC);
  loop_until_bit_is_clear(ADCSRA, ADSC);
  return (ADC);
}

int main(void) {
  // -------- Inits --------- //
  uint16_t lightsOutTimer = 0;                 /* timer for the switch */
  uint16_t adcValue;
  uint16_t middleValue = 511;
  uint16_t highValue = 520;
  uint16_t lowValue = 500;
  uint16_t dead_zone = 0;
  uint8_t padding = INITIAL_PADDING;

  LED_DDR = ((1 << LED0) | (1 << LED1) | (1 << SWITCH));
  initADC();
  initUSART();

  // ------ Event loop ------ //
  while (1) {
    adcValue = readADC(PIEZO); // Continously read the ADC

                     /* moving average -- tracks sensor's bias voltage */
    middleValue = adcValue + middleValue - ((middleValue - 8) >> 4);
          /* moving averages for positive and negative parts of signal */
		  
    if (adcValue > (middleValue >> 4))
	{
      highValue = adcValue + highValue - ((highValue - 8) >> 4);
    }
	
    if (adcValue < (middleValue >> 4))
	{
      lowValue = adcValue + lowValue - ((lowValue - 8) >> 4);
    }
	
            /* "padding" provides a minimum value for the noise volume */
    dead_zone = highValue - lowValue + padding;

            /* Now check to see if ADC value above or below thresholds */
                /* Comparison with >> 4 b/c EWMA is on different scale */
	uint16_t negative_threshold = 0;
	negative_threshold = ((middleValue - dead_zone) >> 4);
	
	uint16_t positive_threshold = 0;
	positive_threshold = ((middleValue + dead_zone) >> 4);
	
    if (adcValue < negative_threshold)
	{
      LED_PORT = (1 << LED0) | (1 << SWITCH);       /* one LED, switch */
      lightsOutTimer = ON_TIME / CYCLE_DELAY;           /* reset timer */
    }
	
    else if (adcValue > positive_threshold)
	{
      LED_PORT = (1 << LED1) | (1 << SWITCH);     /* other LED, switch */
      lightsOutTimer = ON_TIME / CYCLE_DELAY;           /* reset timer */
    }
	
    else
	{                          
      if (lightsOutTimer > 0)
	  {                  /* time left on timer */
        lightsOutTimer--;
      }
	  
      else 
	  {                                              /* time's up */
        LED_PORT &= ~(1 << SWITCH);                 /* turn switch off */
		LED_PORT &= ~(1 << LED0);
        LED_PORT &= ~(1 << LED1);
      }
    }
	
#if USE_POT                          /* optional padding potentiometer */
    padding = readADC(POT) >> 4;         /* scale down to useful range */
#endif

                                            /* Serial output and delay */
           /* ADC is 10-bits, recenter around 127 for display purposes */
		   
    printString("ADC: ");
	printWord(adcValue);
	
	printString(" | neg threshold: ");
	printWord(negative_threshold); 
	
	printString(" | pos threshold: ");
	printWord(positive_threshold); 

	//printString(" | dead_zone: ");
	//printWord(dead_zone);
	
	printString("\r\n");
	
    // transmitByte((lowValue >> 4) - 512 + 127);
    // transmitByte((highValue >> 4) - 512 + 127);
    _delay_ms(CYCLE_DELAY);
  }     
                                            
  return 0;                            
}
