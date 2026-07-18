#include <avr/io.h>
#include "adc.h"

/* ADMUX: REFS0 = AVcc reference with an external cap on AREF.
 * ADCSRA: ADEN enable; ADPS2:0 = 111 -> /128 prescaler (125 kHz @ 16 MHz). */
#define ADC_NOISE_SAMPLES 16u   /* conversions folded into the entropy byte */
#define ADC0_CHANNEL      0u

void adc_init(void) {
    ADMUX  = (1 << REFS0);                                   /* AVcc ref, ch 0 */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

void adc_select_channel(uint8_t channel) {
    /* Preserve REFS bits, swap in the low mux nibble. */
    ADMUX = (uint8_t)((ADMUX & 0xF0u) | (channel & 0x0Fu));
}

void adc_start(void) {
    ADCSRA |= (1 << ADSC);          /* start conversion */
}

bool adc_ready(void) {
    /* ADSC clears itself when the conversion completes. */
    return (ADCSRA & (1 << ADSC)) == 0;
}

uint16_t adc_read(void) {
    return ADC;                     /* ADCL then ADCH read order handled by macro */
}

uint8_t adc_sample_noise(void) {
    uint8_t entropy = 0;
    adc_select_channel(ADC0_CHANNEL);
    for (uint8_t i = 0; i < ADC_NOISE_SAMPLES; i++) {
        adc_start();
        while (!adc_ready()) {
            /* boot-only spin: ~104 us per conversion */
        }
        /* The two LSBs of a floating-input conversion are the noisiest; rotate
         * them through the byte so 16 samples fill all 8 bit positions. */
        entropy = (uint8_t)((entropy << 1) ^ (adc_read() & 0x03u));
    }
    return entropy;
}

#ifdef ENABLE_AUDIO
void adc_start_free_running(uint8_t channel) {
    /* TODO(tier3): ADATE + ADSC with a free-running trigger source, ADIE for a
     * completion ISR that feeds a rolling RMS envelope. Not implemented. */
    adc_select_channel(channel);
    (void)channel;
}
#endif
