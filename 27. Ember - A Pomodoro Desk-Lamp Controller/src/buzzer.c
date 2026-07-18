/*
 * buzzer.c
 *
 * Purpose : Non-blocking piezo tone/melody generator via Timer1 CTC,
 *           toggling OC1A on compare match.
 * Hardware: PB1 / OC1A (passive piezo to GND), pin_t from hardware_connections.h.
 * Datasheet: ATmega328P §15.11 (Timer1 registers), §15.9.1 (OC1A pin),
 *            Table 15-5 (CTC mode 4: WGM13:0=0100), Table 15-3 (COM1A: toggle),
 *            Table 15-6 (CS1x prescaler).
 *
 * Tone frequency derivation (toggle mode halves the compare rate):
 *   f_out = F_CPU / (2 * prescaler * (OCR1A + 1))
 *   -> OCR1A = F_CPU / (2 * prescaler * f_out) - 1
 *
 * Fixed prescaler /8. Timer1 is 16-bit, so OCR1A stays in range across the
 * whole 100-3000 Hz SFX band:
 *   100 Hz  -> OCR1A = 9999   (< 65535, ok)
 *   3000 Hz -> OCR1A = 332
 */

#include "buzzer.h"
#include "sys_tick.h"
#include "pin.h"

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stddef.h>

#define BUZZER_PRESCALER      8UL
#define BUZZER_CS_BITS        (1u << CS11)   /* /8 (Table 15-6) */

/* Timestamp (ms) at which the current tone should be silenced/advanced. */
static volatile uint32_t s_stop_at = 0;
static volatile uint8_t  s_active  = 0;

/* In-progress melody state. s_note_count == 0 means "no melody active". */
static const note_t *s_notes      = NULL;
static uint8_t        s_note_count = 0;
static uint8_t        s_note_idx   = 0;

static pin_t s_pin;

void buzzer_init(pin_t buzzer)
{
    s_pin = buzzer;
    buzzer_off();
}

void buzzer_off(void)
{
    TCCR1B = 0u;                 /* stop Timer1                     */
    TCCR1A = 0u;                 /* disconnect OC1A                 */
    pin_low(s_pin);
    pin_set_input(s_pin, false); /* tristate — no DC bias on piezo  */
    s_active     = 0u;
    s_note_count = 0u;
}

/* Configures Timer1 for freq_hz and arms s_stop_at for duration_ms. Does NOT
 * touch s_note_count — callers decide whether this cancels a melody. */
static void start_tone_hw(uint16_t freq_hz, uint16_t duration_ms)
{
    if (freq_hz == 0u) {
        buzzer_off();
        return;
    }

    pin_set_output(s_pin);   /* PB1 output so OC1A can drive it */

    uint32_t top = (F_CPU / (2UL * BUZZER_PRESCALER * (uint32_t)freq_hz));
    if (top > 0UL) { top -= 1UL; }
    if (top > 0xFFFFUL) { top = 0xFFFFUL; }

    TCCR1B = 0u;   /* stop before reconfiguring — avoids glitches */

    /* CTC mode 4 (TOP = OCR1A): WGM12=1, WGM13/11/10=0.
     * Toggle OC1A on compare match: COM1A0=1, COM1A1=0 (Table 15-3). */
    TCCR1A = (1u << COM1A0);
    OCR1A  = (uint16_t)top;
    TCCR1B = (1u << WGM12) | BUZZER_CS_BITS;   /* write CS last -> starts timer */

    s_stop_at = sys_millis() + duration_ms;
    s_active  = 1u;
}

void buzzer_tone_nb(uint16_t freq_hz, uint16_t duration_ms)
{
    s_note_count = 0u;   /* a direct tone request abandons any melody */
    start_tone_hw(freq_hz, duration_ms);
}

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

bool buzzer_is_active(void)
{
    return s_active != 0u;
}

/* ---- SFX library ---- */
const note_t SFX_CONFIRM[2] PROGMEM = {
    { 1200u, 40u },
    { 1800u, 60u },
};

const note_t SFX_BREAK[3] PROGMEM = {
    { 1600u, 120u },
    { 2000u, 120u },
    { 2400u, 200u },
};

const note_t SFX_DONE[3] PROGMEM = {
    { 1800u, 100u },
    { 1400u, 100u },
    { 1000u, 180u },
};
