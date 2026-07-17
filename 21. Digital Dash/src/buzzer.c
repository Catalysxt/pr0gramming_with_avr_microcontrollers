/*
 * buzzer.c
 *
 * Purpose : Non-blocking piezo tone via Timer2 CTC, toggle OC2A.
 * Hardware : PB3 / OC2A (passive piezo to GND).
 * Datasheet: ATmega328P §17.11 (Timer2 registers), §17.6.2 (OC2A pin),
 *            Table 17-8 (CTC WGM22=0,WGM21=1,WGM20=0), Table 17-9 (CS2x)
 *
 * Tone frequency derivation:
 *   f_out = F_CPU / (2 * prescaler * (OCR2A + 1))
 *   → OCR2A = F_CPU / (2 * prescaler * f_out) - 1
 *
 * Prescaler is chosen as the smallest value that keeps OCR2A in [0,255].
 */

#include "buzzer.h"
#include "hardware_connections.h"
#include "sys_tick.h"

#include <avr/io.h>
#include <stddef.h>

/* Timestamp (ms) at which the current tone should be silenced. */
static volatile uint32_t s_stop_at = 0;
static volatile uint8_t  s_active  = 0;

/* Prescaler encoding for TCCR2B CS2[2:0] bits (datasheet Table 17-9). */
typedef struct {
    uint16_t divisor;
    uint8_t  cs_bits;   /* value to OR into TCCR2B for CS22:CS20 */
} prescaler_entry_t;

static const prescaler_entry_t k_prescalers[] = {
    {    1u, 0x01u },
    {    8u, 0x02u },
    {   32u, 0x03u },
    {   64u, 0x04u },
    {  128u, 0x05u },
    {  256u, 0x06u },
    { 1024u, 0x07u },
};
#define NUM_PRESCALERS  (sizeof(k_prescalers) / sizeof(k_prescalers[0]))

void buzzer_tone_nb(uint16_t freq_hz, uint16_t duration_ms)
{
    if (freq_hz == 0u) {
        buzzer_off();
        return;
    }

    /* Make PB3 an output so OC2A can drive the piezo. */
    BUZZER_DDR |= (1u << BUZZER_BIT);

    /* Find the smallest prescaler that keeps OCR2A within [0,255].
     * OCR2A = F_CPU / (2 * prescaler * freq) - 1  */
    uint8_t ocr   = 0;
    uint8_t cs    = k_prescalers[NUM_PRESCALERS - 1u].cs_bits; /* fallback */
    for (uint8_t i = 0; i < NUM_PRESCALERS; i++) {
        uint32_t denom = 2UL * k_prescalers[i].divisor * freq_hz;
        if (denom == 0u) continue;
        uint32_t val   = (F_CPU / denom);
        if (val >= 1UL && (val - 1UL) <= 255UL) {
            ocr = (uint8_t)(val - 1UL);
            cs  = k_prescalers[i].cs_bits;
            break;
        }
    }

    /* Stop Timer2 before reconfiguring to avoid glitches. */
    TCCR2B = 0u;

    /*
     * CTC mode: WGM22=0, WGM21=1, WGM20=0  (datasheet Table 17-8, mode 2)
     * Toggle OC2A on compare match: COM2A1=0, COM2A0=1  (Table 17-4)
     */
    TCCR2A = (1u << COM2A0) | (1u << WGM21);
    OCR2A  = ocr;
    TCCR2B = cs;   /* write CS bits last — this starts the timer */

    s_stop_at = sys_millis() + duration_ms;
    s_active  = 1u;
}

void buzzer_service(void)
{
    if (s_active && (sys_millis() >= s_stop_at)) {
        buzzer_off();
    }
}

void buzzer_off(void)
{
    TCCR2B = 0u;                            /* stop Timer2              */
    TCCR2A = 0u;                            /* disconnect OC2A          */
    BUZZER_DDR &= ~(1u << BUZZER_BIT);     /* tristate PB3 — no bias   */
    s_active = 0u;
}
