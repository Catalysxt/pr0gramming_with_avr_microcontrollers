/*
 * buzzer.h
 *
 * Non-blocking tone/melody generator using Timer1 CTC toggle-mode on OC1A
 * (PB1). Call buzzer_service() from the main superloop every iteration to
 * auto-stop expired tones and advance in-progress melodies.
 *
 * Timer1 (not Timer2) so Timer2 is free to multiplex the display and Timer0
 * to run sys_tick. Being 16-bit, Timer1 spans the whole 100-3000 Hz range at
 * a single /8 prescaler.
 *
 * ATmega328P datasheet §15 (Timer1/Counter1)
 */

#ifndef EMBER_BUZZER_H_
#define EMBER_BUZZER_H_

#include <stdint.h>
#include <stdbool.h>
#include <avr/pgmspace.h>
#include "pin.h"

/* One note in a PROGMEM melody: play at freq_hz for duration_ms, then the
 * next note in the array (if any) begins automatically. */
typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} note_t;

/* Bind the OC1A pin (BUZZER). Call once during init. */
void buzzer_init(pin_t buzzer);

/* Start a single tone at freq_hz for duration_ms. Returns immediately — the
 * tone runs in hardware via Timer1. Abandons any in-progress melody.
 * freq_hz == 0 silences the buzzer immediately. */
void buzzer_tone_nb(uint16_t freq_hz, uint16_t duration_ms);

/* Start playing a PROGMEM note_t array of length len. buzzer_service()
 * advances through the notes automatically; a later buzzer_tone_nb() or
 * buzzer_play() abandons whatever melody is in progress. */
void buzzer_play(const note_t *notes, uint8_t len);

/* Poll every main-loop iteration: silences an expired single tone, or
 * advances an in-progress melody to its next note. */
void buzzer_service(void);

/* Immediately silence the buzzer, tristate the pin, abandon any melody. */
void buzzer_off(void);

/* True while a tone or melody is sounding. main.c checks this so it never
 * powers down (which would freeze Timer1) in the middle of a chime. */
bool buzzer_is_active(void);

/* ---- SFX library (PROGMEM note_t arrays) ---- */
extern const note_t SFX_CONFIRM[2] PROGMEM;  /* rising 2-note: setting locked in   */
extern const note_t SFX_BREAK[3]   PROGMEM;  /* bright chime: work done, lamp ON    */
extern const note_t SFX_DONE[3]    PROGMEM;  /* falling 3-note: break over, back idle */

#define SFX_CONFIRM_LEN  2u
#define SFX_BREAK_LEN    3u
#define SFX_DONE_LEN     3u

#endif /* EMBER_BUZZER_H_ */
