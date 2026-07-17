/*
 * buzzer.h
 *
 * Non-blocking passive piezo driver using Timer2 CTC on OC2A/PB3.
 *
 * Key design notes:
 *   - Timer2 hardware-toggles OC2A/PB3 on each Compare Match A.
 *   - Auto-stop is handled in buzzer_update(), called every superloop.
 *   - Melody playback is also non-blocking (stepped in buzzer_update()).
 *   - All SFX arrays live in PROGMEM to save SRAM.
 *
 * ATmega328P datasheet:
 *   Section 17   — Timer/Counter2 (8-bit)
 *   Section 17.5 — Output Compare Unit
 *   Section 17.8 — TCCR2A/TCCR2B control registers
 *   Table 17-6   — Compare Output Mode (COM2A1:0 = 01 → toggle OC2A)
 *   Table 17-8   — Clock Select (CS22:20)
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include <avr/pgmspace.h>

/* One musical note: frequency in Hz, duration in ms. */
typedef struct
{
    uint16_t hz;
    uint16_t ms;
} note_t;

/* Initialise Timer2 and set PB3 (OC2A) as output. Buzzer starts silent. */
void buzzer_init(void);

/* Start a tone at hz for ms milliseconds (non-blocking).
 * hz=0 silences the buzzer immediately. */
void buzzer_tone(uint16_t hz, uint16_t ms);

/* Silence the buzzer immediately. */
void buzzer_stop(void);

/* Start playing a melody stored in PROGMEM (non-blocking).
 * notes: pointer to a PROGMEM note_t array; len: number of notes. */
void buzzer_play_melody(const note_t *notes, uint8_t len);

/* Must be called every superloop iteration to advance melody playback
 * and stop tones whose duration has elapsed. */
void buzzer_update(void);

/* ==========================================================================
 * Built-in SFX melodies (PROGMEM, use with buzzer_play_melody)
 * ========================================================================== */
extern const note_t SFX_NAV_TICK[]     PROGMEM;
extern const note_t SFX_CONFIRM[]      PROGMEM;
extern const note_t SFX_WIN[]          PROGMEM;
extern const note_t SFX_LOSE[]         PROGMEM;
extern const note_t SFX_JACKPOT[]      PROGMEM;
extern const note_t SFX_REEL_TICK[]    PROGMEM;

extern const uint8_t SFX_NAV_TICK_LEN;
extern const uint8_t SFX_CONFIRM_LEN;
extern const uint8_t SFX_WIN_LEN;
extern const uint8_t SFX_LOSE_LEN;
extern const uint8_t SFX_JACKPOT_LEN;
extern const uint8_t SFX_REEL_TICK_LEN;

#endif /* BUZZER_H */
