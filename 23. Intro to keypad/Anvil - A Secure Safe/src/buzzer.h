/*
 * buzzer.h
 *
 * Non-blocking tone/melody generator using Timer2 CTC toggle-mode on OC2A
 * (PB3). Call buzzer_service() from the main superloop every iteration to
 * auto-stop expired tones and advance in-progress melodies.
 *
 * ATmega328P datasheet §17 (Timer2/Counter2)
 */

#ifndef ANVIL_BUZZER_H_
#define ANVIL_BUZZER_H_

#include <stdint.h>
#include <avr/pgmspace.h>

/* One note in a PROGMEM melody: play at freq_hz for duration_ms, then the
 * next note in the array (if any) begins automatically. */
typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} note_t;

/*
 * Start a single tone at freq_hz for duration_ms. Returns immediately —
 * the tone runs in hardware via Timer2. Abandons any in-progress
 * buzzer_play() melody. freq_hz == 0 silences the buzzer immediately.
 */
void buzzer_tone_nb(uint16_t freq_hz, uint16_t duration_ms);

/*
 * Start playing a PROGMEM note_t array of length len. buzzer_service()
 * advances through the notes automatically; a call to buzzer_tone_nb() or
 * another buzzer_play() abandons whatever melody is in progress.
 */
void buzzer_play(const note_t *notes, uint8_t len);

/*
 * Poll every main-loop iteration. Silences an expired single tone, or
 * advances an in-progress melody to its next note.
 */
void buzzer_service(void);

/*
 * Immediately silence the buzzer, tristate the pin, and abandon any
 * in-progress melody.
 */
void buzzer_off(void);

/* ---- SFX library (PROGMEM note_t arrays) ---- */
extern const note_t SFX_KEY_TAP[1]      PROGMEM; /* 2kHz/20ms blip on every accepted key      */
extern const note_t SFX_SUCCESS[3]      PROGMEM; /* 3-note rising chime: correct PIN           */
extern const note_t SFX_FAIL[1]         PROGMEM; /* 200Hz/400ms whomp: wrong PIN / entry error */
extern const note_t SFX_LOCKOUT_TICK[1] PROGMEM; /* 1500Hz/30ms: one per countdown second      */
extern const note_t SFX_CHANGE_OK[5]    PROGMEM; /* 5-note arpeggio: PIN change committed      */

#define SFX_KEY_TAP_LEN      1u
#define SFX_SUCCESS_LEN      3u
#define SFX_FAIL_LEN         1u
#define SFX_LOCKOUT_TICK_LEN 1u
#define SFX_CHANGE_OK_LEN    5u

#endif /* ANVIL_BUZZER_H_ */
