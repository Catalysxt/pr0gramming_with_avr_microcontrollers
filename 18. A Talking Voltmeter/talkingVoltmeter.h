
#include "allDigits.h"
#include <avr/pgmspace.h>

// Redefine sample-table names in "allDigits.h"
#define ONE_TABLE DPCM_one_8000
#define TWO_TABLE DPCM_two_8000
#define THREE_TABLE DPCM_three_8000
#define FOUR_TABLE DPCM_four_8000
#define FIVE_TABLE DPCM_five_8000
#define SIX_TABLE DPCM_six_8000
#define SEVEN_TABLE DPCM_seven_8000
#define EIGHT_TABLE DPCM_eight_8000
#define NINE_TABLE DPCM_nine_8000
#define ZERO_TABLE DPCM_zero_8000
#define POINT_TABLE DPCM_point_8000
#define VOLTS_TABLE DPCM_volts_8000
#define INTRO_TABLE DPCM_talkingvoltmeter_8000

#define SPEECH_DELAY    2000 // ms

// Globals used by the ISR. Used to determine what sample to play
volatile uint8_t *this_table_ptr;    // Points at the current speech table
volatile uint16_t *this_table_length;   // Length of speech table

volatile uint16_t sample_number;    // sample index

// Output values. Keep track of previous and current PWM values
volatile int8_t out = 0;
volatile int8_t last_out = 0;

// Used when the decoder reconstructs the audio 
volatile uint8_t differentials[4] = {0, 0, 0, 0};
const int8_t dpcm_weights[4] = { -12, -3, 3, 12 };

// These arrays let us choose a table (and its length) numerically
const uint16_t table_lengths[] = 
{
    sizeof(ZERO_TABLE), sizeof(ONE_TABLE), sizeof(TWO_TABLE),
    sizeof(THREE_TABLE), sizeof(FOUR_TABLE), sizeof(FIVE_TABLE),
    sizeof(SIX_TABLE), sizeof(SEVEN_TABLE), sizeof(EIGHT_TABLE),
    sizeof(NINE_TABLE), sizeof(POINT_TABLE), sizeof(VOLTS_TABLE),
    sizeof(INTRO_TABLE)
};

// Create an indexing table of the starting addresses for each spoken digit. Store it in PROGMEM
// We use this to index the speech data
// This is an array of pointers that stores the starting address of the arrays of spoken data
const uint8_t *const table_pointers[] PROGMEM =
{
    ZERO_TABLE, ONE_TABLE, TWO_TABLE, THREE_TABLE, FOUR_TABLE,
    FIVE_TABLE, SIX_TABLE, SEVEN_TABLE, EIGHT_TABLE, NINE_TABLE,
    POINT_TABLE, VOLTS_TABLE, INTRO_TABLE
};

// We index the 2 arrays using the argument
void select_table(uint8_t which_table)
{
    uint16_t pointer_address;
    this_table_length = table_lengths[which_table]; // How large is the array?
    pointer_address = (uint16_t) & table_pointers[which_table]; // Where is the table located?
        // Since table_pointers is a pointer we need to get the address so we can pass it into pgm_read_word

        // Using the address, read the speech data from PROGMEM 
    this_table_ptr = (uint8_t *) pgm_read_word(pointer_address);
}

#define POINT 10
#define VOLTS 11
#define INTRO 12

void init_timer_0(void)
{
    TCCR0A |= (1 << WGM00) | (1 << WGM01);  // Configure for Fast-PWM Mode. This is mode 7. WGM02 from TCRR0B is also used
    TCCR0A |= (1 << COM0A0) | (1 << COM0A1);    // Send PWM signal to OC0A/PD6
        // COM0Ax = Compare Match Output Mode
    TCCR0B = (1 << CS00);   // No prescaling
        // CS0x = Clock Select bits
    OCR0A = 128;    // Initialize to halfway
    SPEAKER_DDR |= (1 << SPEAKER); // Set speaker as output
}

// Feeds timer 0. Runs at 8kHz. Timer 0 is x4 faster
void init_timer_2(void)
{
    TCCR2A |= (1 << WGM21);
    TIMSK2 |= (1 << OCIE2A);    // Enable compare interrupts for the ISR
        // TIMSK = Timer Interrupt Mask Register
        // OCIExA = Output Compare Interrupt Enable Bit
    OCR2A = 128;    // Controls sample playback frequency
}

void init_adc(void)
{
    ADMUX |= (0b00001111 & PC5);    // Set mux to ADC5
    DIDR0 |= _BV(ADC5D);    // Disable circuitry on PC5 for power reduction
        // DIDRx = Digital Input Disable Register
    ADMUX |= (1 << REFS0);
    ADCSRA |= (1 << ADPS1) | (1 << ADPS2) | (1 << ADPS0);  // Prescale by /128x
	    // I'm using external 16MHz crystal oscillator
    ADCSRA |= (1 << ADEN);  // Enable ADC
}
