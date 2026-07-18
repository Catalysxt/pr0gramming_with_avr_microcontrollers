#ifndef DIGITAL_COMPANION_ADC_H_
#define DIGITAL_COMPANION_ADC_H_

#include <stdint.h>
#include <stdbool.h>

/* Non-blocking ADC driver (AVcc reference, /128 prescaler).
 *
 * At F_CPU = 16 MHz the /128 prescaler gives a 125 kHz ADC clock -- inside the
 * datasheet's 50-200 kHz window for full 10-bit accuracy (each conversion takes
 * 13 ADC clocks ~= 104 us). Two uses in this project:
 *   1. adc_sample_noise() -- fold several conversions of the FLOATING ADC0 pin
 *      into a byte of entropy for the xorshift PRNG seed at boot.
 *   2. Tier-3 audio scaffold -- ADC1 free-running for a mic RMS envelope
 *      (guarded by ENABLE_AUDIO; DSP not implemented yet).
 *
 * Single-conversion use is non-blocking: adc_start() then poll adc_ready(). */

/* Enable the ADC (AVcc ref, /128 clock). Call once at boot. */
void adc_init(void);

/* Select the mux channel (0..7) for subsequent conversions. */
void adc_select_channel(uint8_t channel);

/* Kick off one conversion on the selected channel (returns immediately). */
void adc_start(void);

/* True once the in-flight conversion has finished. */
bool adc_ready(void);

/* 10-bit result of the last completed conversion. */
uint16_t adc_read(void);

/* Blocking-but-brief boot helper: sample the floating ADC0 pin several times
 * and XOR-fold the noisy low bits into one entropy byte. Boot-only (not the
 * main loop), so the short spin is acceptable. */
uint8_t adc_sample_noise(void);

#ifdef ENABLE_AUDIO
/* TODO(tier3): start ADC1 in free-running mode for the mic envelope. */
void adc_start_free_running(uint8_t channel);
#endif

#endif /* DIGITAL_COMPANION_ADC_H_ */
