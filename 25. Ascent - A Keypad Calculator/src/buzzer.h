/*
 * buzzer.h
 *
 * Non-blocking tone/melody generator using Timer2 CTC toggle-mode on OC2A
 * (PB3). Call buzzer_service() from the main superloop every iteration to
 * auto-stop expired tones and advance in-progress melodies.
 *
 * ATmega328P datasheet §17 (Timer2/Counter2)
 */

#ifndef ASCENT_BUZZER_H_
#define ASCENT_BUZZER_H_

#include <stdint.h>
#include <avr/pgmspace.h>

/* One note in a PROGMEM melody: play at freq_hz for duration_ms, then the
 * next note in the array (if any) begins automatically. */
typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} note_t;

/*
 * Sets BUZZER_DDR as output. Call once during init (DDR only — no timer
 * is armed until the first tone/melody request).
 */
void buzzer_init(void);

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

/* ---- SFX library (PROGMEM note_t arrays), one per key class ---- */
extern const note_t SFX_DIGIT[1]    PROGMEM; /* 2000 Hz / 20 ms blip                       */
extern const note_t SFX_OPERATOR[1] PROGMEM; /* 1500 Hz / 30 ms chirp                      */
extern const note_t SFX_EQUALS[3]   PROGMEM; /* 3-note rising chime: 1000,1500,2000 Hz/70ms */
extern const note_t SFX_CLEAR[2]    PROGMEM; /* 2-note descending sweep: 2000->800 Hz/50ms  */
extern const note_t SFX_ERROR[1]    PROGMEM; /* 200 Hz / 400 ms whomp                      */

#define SFX_DIGIT_LEN    1u
#define SFX_OPERATOR_LEN 1u
#define SFX_EQUALS_LEN   3u
#define SFX_CLEAR_LEN    2u
#define SFX_ERROR_LEN    1u

#endif /* ASCENT_BUZZER_H_ */
