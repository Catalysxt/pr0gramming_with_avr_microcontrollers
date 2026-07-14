
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include "pinDefines.h"
#include "USART.h"

#define SENSE_TIME  50
#define THRESHOLD   12000

// Since it's used in the ISR and main() it must be declared volatile
volatile uint16_t charge_cycle_count;

void init_pin_change_interrupt(void)
{
  PCICR |= (1 << PCIE1);  // Enables interrupts for PCINT[14:8]. An interrupt will trigger ISR (PCINT1). PCINT = Pin Change Interrupt Request 1
    // PCICR = Pin Change Interrupt Control Register
    // PCIE1 = Pin Change Interrupt Enable 1
  PCMSK1 |= (1 << PC1); // Enable interrupts on pin PC1. This is associated with PCINT9
    // PCMSK =  Pin Change Mask Register
}

ISR(PCINT1_vect)
{
  charge_cycle_count++;

  // Charge the cap
  CAP_SENSOR_DDR |= (1 << CAP_SENSOR);
  _delay_us(1);

  // Discharge it
  CAP_SENSOR_DDR &= ~(1 << CAP_SENSOR);
  PCIFR |= (1 << PCIF1);  // Clear it. If we don't clear it, program execution will jump back to the PCINT ISR. 
    // PCIFR = Pin Change Interrupt Flag Register
    // PCIF1 = Pin Change Interrupt Flag 1
    // When an interrupt on PCINT[14:8] is requested, PCIF1 is SET
}

// I wonder how many times the cap charges and discharges. Well we keep track of it in charge_cycle_count

int main(void)
{
  clock_prescale_set(clock_div_1);  // Full speed! 8MHz
  initUSART();
  printString("==[ Cap Sensor ]==\r\n\r\n");

  LED_DDR = 0xff;
  MCUCR |= (1 << PUD);  // Disable pullups BOI!
    // We don't want pullups enables when we're sensing the voltage when the ISR configures PC1 as input. 
    // MCUCR = MCU Control Register
    // PUD = Pull-Up Disable Bit

  CAP_SENSOR_PORT |= (1 << CAP_SENSOR);

  init_pin_change_interrupt();

  while (1)
  { 
    charge_cycle_count = 0; // Reset counter
    CAP_SENSOR_DDR |= (1 << CAP_SENSOR);  // Begin with cap charged
    
    sei();
    _delay_ms(SENSE_TIME);
    cli();

    if (charge_cycle_count < THRESHOLD)
    {
      LED_PORT = 0xff;
    }
    else 
    {
      LED_PORT = 0;
    }

    printWord(charge_cycle_count);
    printString("\r\n");
  }


}

