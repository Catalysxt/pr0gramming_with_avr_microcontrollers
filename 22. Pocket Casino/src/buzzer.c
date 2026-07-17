/*
 * buzzer.c
 *
 * Passive piezo driver — Timer2 CTC mode, hardware toggle on OC2A/PB3.
 *
 * Frequency generation:
 *   Toggle OC2A every OCR2A+1 Timer2 counts → square wave period =
 *   2 * (OCR2A+1) * prescaler / F_CPU.
 *
 *   Rearranged:  OCR2A = F_CPU / (2 * prescaler * freq_hz) - 1
 *
 *   Timer2 has an 8-bit counter so OCR2A max is 255.  The prescaler is
 *   chosen so that OCR2A fits within [0, 255] for the requested frequency.
 *
 *   Prescaler table at F_CPU = 16 MHz (ATmega328P datasheet Table 17-8):
 *     CS22:20 = 001 → /1    min freq ≈ 31 250 Hz  (too high for audio)
 *     CS22:20 = 010 → /8    min freq ≈  3 815 Hz
 *     CS22:20 = 011 → /32   min freq ≈    977 Hz
 *     CS22:20 = 100 → /64   min freq ≈    488 Hz
 *     CS22:20 = 101 → /128  min freq ≈    244 Hz
 *     CS22:20 = 110 → /256  min freq ≈    122 Hz
 *     CS22:20 = 111 → /1024 min freq ≈     30 Hz
 *
 * Auto-stop: buzzer_update() compares g_ms_tick against a deadline set by
 *   buzzer_tone(); when elapsed, Timer2 is disabled and OC2A goes low.
 *
 * Melody playback: buzzer_update() steps through a PROGMEM note_t array,
 *   calling buzzer_tone() for each note.
 *
 * ATmega328P datasheet references:
 *   Section 17.7    — Timer/Counter2 Registers
 *   Section 17.8.1  — TCCR2A: WGM21:20 = 10 → CTC; COM2A1:0 = 01 → toggle
 *   Section 17.8.2  — TCCR2B: CS22:20 = prescaler select
 *   Section 17.8.3  — TCNT2: counter register
 *   Section 17.8.4  — OCR2A: compare value
 *   Table 17-6      — COM2A mode: 01 = toggle OC2A on Compare Match
 *   Table 17-8      — Clock Select (CS22:20)
 */

#include "buzzer.h"
#include "hardware_connections.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stddef.h>

/* Shared 1 ms tick counter from main.c */
extern volatile uint32_t g_ms_tick;

/* Active tone state */
static volatile uint32_t s_stop_time  = 0U;   /* g_ms_tick value when tone should end */
static volatile uint8_t  s_tone_active = 0U;

/* Melody playback state */
static const note_t *s_melody       = NULL;
static uint8_t       s_melody_len   = 0U;
static uint8_t       s_melody_index = 0U;

/* ==========================================================================
 * Private: select Timer2 prescaler and return CS22:20 bits for TCCR2B.
 * Also writes OCR2A for the given frequency.
 * Returns 0 if freq_hz is 0 (silence).
 * ========================================================================== */
static uint8_t select_prescaler_and_set_ocr(uint16_t freq_hz)
{
    uint32_t ocr;
    uint16_t prescaler;
    uint8_t  cs_bits;

    if (freq_hz == 0U)
    {
        return 0U;
    }

    /* Pick the smallest prescaler that keeps OCR2A in [0, 255].
     * ATmega328P datasheet Table 17-8. */
    if (freq_hz >= 3815U)      { prescaler = 8U;    cs_bits = (1U << CS21); }
    else if (freq_hz >= 977U)  { prescaler = 32U;   cs_bits = (1U << CS21) | (1U << CS20); }
    else if (freq_hz >= 488U)  { prescaler = 64U;   cs_bits = (1U << CS22); }
    else if (freq_hz >= 244U)  { prescaler = 128U;  cs_bits = (1U << CS22) | (1U << CS20); }
    else if (freq_hz >= 122U)  { prescaler = 256U;  cs_bits = (1U << CS22) | (1U << CS21); }
    else                       { prescaler = 1024U; cs_bits = (1U << CS22) | (1U << CS21) | (1U << CS20); }

    /* OCR2A = F_CPU / (2 * prescaler * freq_hz) - 1
     * Computed in 32-bit to avoid overflow before the division. */
    ocr = (uint32_t)F_CPU / (2UL * (uint32_t)prescaler * (uint32_t)freq_hz);
    if (ocr > 0U)
    {
        ocr -= 1U;
    }
    if (ocr > 255U)
    {
        ocr = 255U;     /* clamp — shouldn't happen with correct prescaler */
    }

    OCR2A = (uint8_t)ocr;
    return cs_bits;
}

void buzzer_init(void)
{
    /* PB3 (OC2A) as output (datasheet Section 14.4.1, DDRB bit 3). */
    BUZZER_DDR |= (1U << BUZZER_PIN);

    /* Timer2 CTC mode: WGM22:21:20 = 0:1:0 → CTC on OCR2A.
     * COM2A1:0 = 0:1 → toggle OC2A on Compare Match A.
     * Datasheet Section 17.8.1 (TCCR2A), Table 17-3 (WGM), Table 17-6 (COM2A). */
    TCCR2A = (1U << COM2A0) | (1U << WGM21);

    /* Timer2 stopped: CS22:20 = 000 (no clock source).
     * Datasheet Section 17.8.2 (TCCR2B), Table 17-8. */
    TCCR2B = 0x00U;

    TCNT2  = 0U;
    OCR2A  = 0U;

    s_tone_active  = 0U;
    s_melody       = NULL;
}

void buzzer_stop(void)
{
    /* Stop clock: CS22:20 = 000 (datasheet Table 17-8). */
    TCCR2B = 0x00U;
    TCNT2  = 0U;

    /* Drive OC2A low. Toggle mode requires stopping and clearing the pin.
     * Easiest: briefly set COM2A1:0 = 00 (normal port operation) and
     * manually clear PB3, then restore toggle mode. */
    TCCR2A &= ~((1U << COM2A1) | (1U << COM2A0));
    BUZZER_PORT &= ~(1U << BUZZER_PIN);
    TCCR2A |= (1U << COM2A0);   /* restore toggle for next tone */

    s_tone_active = 0U;
}

void buzzer_tone(uint16_t hz, uint16_t ms)
{
    uint8_t cs;

    buzzer_stop();

    if (hz == 0U || ms == 0U)
    {
        return;
    }

    cs = select_prescaler_and_set_ocr(hz);

    TCNT2  = 0U;
    TCCR2B = cs;    /* start Timer2 with chosen prescaler */

    s_stop_time   = g_ms_tick + (uint32_t)ms;
    s_tone_active = 1U;
}

void buzzer_play_melody(const note_t *notes, uint8_t len)
{
    if (notes == NULL || len == 0U)
    {
        return;
    }
    s_melody       = notes;
    s_melody_len   = len;
    s_melody_index = 0U;

    /* Kick off the first note immediately. */
    uint16_t hz = pgm_read_word(&notes[0].hz);
    uint16_t ms = pgm_read_word(&notes[0].ms);
    buzzer_tone(hz, ms);
}

void buzzer_update(void)
{
    if (!s_tone_active)
    {
        return;
    }

    if (g_ms_tick >= s_stop_time)
    {
        buzzer_stop();

        /* Advance melody if one is playing. */
        if (s_melody != NULL)
        {
            s_melody_index++;
            if (s_melody_index < s_melody_len)
            {
                uint16_t hz = pgm_read_word(&s_melody[s_melody_index].hz);
                uint16_t ms = pgm_read_word(&s_melody[s_melody_index].ms);
                buzzer_tone(hz, ms);
            }
            else
            {
                s_melody = NULL;   /* melody complete */
            }
        }
    }
}

/* ==========================================================================
 * SFX definitions in PROGMEM
 * Frequencies are chosen to be pleasant on a passive piezo (simple tones).
 * ========================================================================== */

/* Single short click used for menu navigation */
const note_t SFX_NAV_TICK[] PROGMEM =
{
    { 1200U, 30U }
};
const uint8_t SFX_NAV_TICK_LEN = 1U;

/* Two-note ascending confirmation */
const note_t SFX_CONFIRM[] PROGMEM =
{
    { 800U,  60U },
    { 1200U, 80U }
};
const uint8_t SFX_CONFIRM_LEN = 2U;

/* Ascending win fanfare */
const note_t SFX_WIN[] PROGMEM =
{
    { 600U,  80U },
    { 800U,  80U },
    { 1000U, 80U },
    { 1400U, 150U }
};
const uint8_t SFX_WIN_LEN = 4U;

/* Descending lose sound */
const note_t SFX_LOSE[] PROGMEM =
{
    { 500U,  100U },
    { 350U,  100U },
    { 200U,  200U }
};
const uint8_t SFX_LOSE_LEN = 3U;

/* Jackpot celebration — fast ascending run */
const note_t SFX_JACKPOT[] PROGMEM =
{
    { 600U,  60U },
    { 800U,  60U },
    { 1000U, 60U },
    { 1200U, 60U },
    { 1600U, 60U },
    { 1200U, 60U },
    { 1600U, 200U }
};
const uint8_t SFX_JACKPOT_LEN = 7U;

/* Short mid-pitch tick for reel and animation steps */
const note_t SFX_REEL_TICK[] PROGMEM =
{
    { 900U, 25U }
};
const uint8_t SFX_REEL_TICK_LEN = 1U;
