/*
 * buzzer.c
 *
 * Purpose : Non-blocking piezo tone/melody generator via Timer2 CTC,
 *           toggling OC2A on compare match.
 * Hardware: PB3 / OC2A (passive piezo to GND, per hardware.h).
 * Datasheet: ATmega328P §17.11 (Timer2 registers), §17.6.2 (OC2A pin),
 *            Table 17-8 (CTC: WGM22=0,WGM21=1,WGM20=0), Table 17-9 (CS2x)
 *
 * Tone frequency derivation:
 *   f_out = F_CPU / (2 * prescaler * (OCR2A + 1))
 *   -> OCR2A = F_CPU / (2 * prescaler * f_out) - 1
 *
 * Prescaler is the smallest of {8,32,64,128,256,1024} that keeps OCR2A
 * within [0,255] for the requested frequency (100-3000 Hz per spec).
 */

#include "buzzer.h"
#include "hardware.h"
#include "sys_tick.h"

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stddef.h>

/* Timestamp (ms) at which the current tone should be silenced/advanced. */
static volatile uint32_t s_stop_at = 0;
static volatile uint8_t  s_active  = 0;

/* In-progress melody state. s_note_count == 0 means "no melody active" —
 * a plain buzzer_tone_nb() tone in progress, or nothing playing. */
static const note_t *s_notes      = NULL;
static uint8_t        s_note_count = 0;
static uint8_t        s_note_idx   = 0;

/* Prescaler encoding for TCCR2B CS2[2:0] bits (datasheet Table 17-9). */
typedef struct {
    uint16_t divisor;
    uint8_t  cs_bits;   /* value to OR into TCCR2B for CS22:CS20 */
} prescaler_entry_t;

static const prescaler_entry_t k_prescalers[] = {
    {    8u, 0x02u },
    {   32u, 0x03u },
    {   64u, 0x04u },
    {  128u, 0x05u },
    {  256u, 0x06u },
    { 1024u, 0x07u },
};
#define NUM_PRESCALERS  (sizeof(k_prescalers) / sizeof(k_prescalers[0]))

void buzzer_off(void)
{
    TCCR2B = 0u;                          /* stop Timer2             */
    TCCR2A = 0u;                          /* disconnect OC2A         */
    BUZZER_DDR &= ~(1u << BUZZER_BIT);   /* tristate pin — no bias  */
    s_active     = 0u;
    s_note_count = 0u;                    /* abandon any melody      */
}

/* Configures Timer2 for freq_hz and arms s_stop_at for duration_ms. Does
 * NOT touch s_note_count — callers decide whether this cancels a melody. */
static void start_tone_hw(uint16_t freq_hz, uint16_t duration_ms)
{
    if (freq_hz == 0u) {
        buzzer_off();
        return;
    }

    BUZZER_DDR |= (1u << BUZZER_BIT);   /* PB3 output so OC2A can drive it */

    uint8_t ocr = 0;
    uint8_t cs  = k_prescalers[NUM_PRESCALERS - 1u].cs_bits; /* fallback: largest */
    for (uint8_t i = 0; i < NUM_PRESCALERS; i++) {
        uint32_t denom = 2UL * k_prescalers[i].divisor * freq_hz;
        uint32_t val   = F_CPU / denom;
        if (val >= 1UL && (val - 1UL) <= 255UL) {
            ocr = (uint8_t)(val - 1UL);
            cs  = k_prescalers[i].cs_bits;
            break;
        }
    }

    TCCR2B = 0u;   /* stop timer before reconfiguring — avoids glitches */

    /* CTC mode: WGM22=0, WGM21=1, WGM20=0 (Table 17-8, mode 2).
     * Toggle OC2A on compare match: COM2A1=0, COM2A0=1 (Table 17-4). */
    TCCR2A = (1u << COM2A0) | (1u << WGM21);
    OCR2A  = ocr;
    TCCR2B = cs;   /* write CS bits last — this starts the timer */

    s_stop_at = sys_millis() + duration_ms;
    s_active  = 1u;
}

void buzzer_tone_nb(uint16_t freq_hz, uint16_t duration_ms)
{
    s_note_count = 0u;   /* a direct tone request abandons any melody */
    start_tone_hw(freq_hz, duration_ms);
}

/* Reads note idx out of PROGMEM and starts it, without touching
 * s_note_count/s_note_idx (the caller manages those). */
static void play_note_at(uint8_t idx)
{
    uint16_t freq = pgm_read_word(&s_notes[idx].freq_hz);
    uint16_t dur  = pgm_read_word(&s_notes[idx].duration_ms);
    start_tone_hw(freq, dur);
}

void buzzer_play(const note_t *notes, uint8_t len)
{
    if ((notes == NULL) || (len == 0u)) {
        buzzer_off();
        return;
    }
    s_notes      = notes;
    s_note_count = len;
    s_note_idx   = 0u;
    play_note_at(0u);
}

void buzzer_service(void)
{
    if (!s_active || (sys_millis() < s_stop_at)) {
        return;
    }

    if ((s_note_count > 0u) && ((uint8_t)(s_note_idx + 1u) < s_note_count)) {
        s_note_idx++;
        play_note_at(s_note_idx);
    } else {
        buzzer_off();
    }
}

/* ---- SFX library ---- */
const note_t SFX_KEY_TAP[1] PROGMEM = {
    { 2000u, 20u },
};

const note_t SFX_SUCCESS[3] PROGMEM = {
    { 1000u, 80u },
    { 1500u, 80u },
    { 2000u, 80u },
};

const note_t SFX_FAIL[1] PROGMEM = {
    { 200u, 400u },
};

const note_t SFX_LOCKOUT_TICK[1] PROGMEM = {
    { 1500u, 30u },
};

const note_t SFX_CHANGE_OK[5] PROGMEM = {
    { 1000u, 60u },
    { 1250u, 60u },
    { 1500u, 60u },
    { 1750u, 60u },
    { 2000u, 90u },
};
