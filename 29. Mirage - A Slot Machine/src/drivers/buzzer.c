#include <avr/io.h>
#include <avr/pgmspace.h>   /* notes live in flash; pgm_read_word() */
#include "buzzer.h"

/* OC1A is fixed to PB1 on the ATmega328P -- that is the only pin Timer1 can
 * toggle in hardware, which is why the buzzer must live here. */
#define BUZZER_DDR   DDRB
#define BUZZER_PORT  PORTB
#define BUZZER_BIT   PB1

/* Prescaler /8 keeps OCR1A within the 16-bit range across the audible band:
 * OCR1A = F_CPU/(2*8*f) - 1  ->  ~9999 at 100 Hz, ~249 at 4 kHz. */
#define BUZZER_PRESCALE 8u

/* Engine state. Only the main-loop context touches these (no ISR), so no
 * volatile is needed. */
static const note_t *s_seq   = 0;
static uint8_t       s_len   = 0;
static uint8_t       s_index = 0;
static uint32_t      s_note_deadline = 0;   /* ms timestamp the current note ends */
static bool          s_playing = false;
static bool          s_fresh   = false;     /* first tick of a new sequence */

/* Program Timer1 to emit `freq_hz` on OC1A, or go silent for a rest/zero. */
static void tone_set(uint16_t freq_hz) {
    if (freq_hz == 0) {
        /* Rest: detach OC1A so the pin stops toggling, stop the clock, hold low. */
        TCCR1A = 0;
        TCCR1B = 0;
        BUZZER_PORT &= (uint8_t)~(1 << BUZZER_BIT);
        return;
    }

    uint16_t top = (uint16_t)((F_CPU / (2UL * BUZZER_PRESCALE * freq_hz)) - 1UL);

    /* CTC with OCR1A as TOP (WGM12), toggle OC1A on compare match (COM1A0).
     * CS11 selects clk/8. Toggling once per match makes the output frequency
     * half the match rate -- already folded into `top` above. */
    TCCR1A = (1 << COM1A0);
    TCCR1B = (1 << WGM12) | (1 << CS11);
    OCR1A  = top;
    TCNT1  = 0;                 /* restart the period cleanly on a pitch change */
}

void buzzer_init(void) {
    BUZZER_DDR |= (1 << BUZZER_BIT);
    tone_set(0);                /* start silent */
}

void buzzer_stop(void) {
    s_playing = false;
    s_seq     = 0;
    s_len     = 0;
    tone_set(0);
}

void buzzer_play(const note_t *seq, uint8_t len) {
    if (seq == 0 || len == 0) {
        buzzer_stop();
        return;
    }
    s_seq     = seq;
    s_len     = len;
    s_index   = 0;
    s_playing = true;
    s_fresh   = true;           /* buzzer_tick() will load note 0 and time it */
}

/* Load note s_index onto the timer and set its deadline relative to now. */
static void start_current_note(uint32_t now_ms) {
    uint16_t freq = pgm_read_word(&s_seq[s_index].freq_hz);
    uint16_t dur  = pgm_read_word(&s_seq[s_index].dur_ms);
    tone_set(freq);
    s_note_deadline = now_ms + dur;
}

void buzzer_tick(uint32_t now_ms) {
    if (!s_playing) {
        return;
    }

    if (s_fresh) {              /* first tick after buzzer_play(): begin note 0 */
        s_fresh = false;
        start_current_note(now_ms);
        return;
    }

    /* Signed compare so it stays correct across the millisecond counter's wrap. */
    if ((int32_t)(now_ms - s_note_deadline) < 0) {
        return;                 /* current note still sounding */
    }

    if (++s_index >= s_len) {   /* sequence finished */
        buzzer_stop();
        return;
    }
    start_current_note(now_ms);
}

bool buzzer_busy(void) {
    return s_playing;
}
