#ifndef SLOT_MACHINE_BUZZER_H_
#define SLOT_MACHINE_BUZZER_H_

#include <stdint.h>
#include <stdbool.h>

/* Non-blocking square-wave tone / melody engine on Timer1 (CTC, toggling OC1A =
 * PB1). The timer hardware generates the pitch on its own -- CTC toggles the pin
 * every compare match, so once OCR1A is set the CPU spends zero cycles per wave.
 * We only need software to step from note to note when each duration elapses,
 * which buzzer_tick() does off the 1 ms system clock. Nothing here blocks and no
 * ISR is used (the tone is pure hardware), so audio never disturbs the main loop
 * or the display refresh.
 *
 * Melodies are arrays of note_t in PROGMEM. A note with freq_hz == 0 is a rest
 * (silence for its duration). Starting a new sequence cancels the current one,
 * so the engine is fully interruptible (e.g. a win jingle preempts spin clicks). */

typedef struct {
    uint16_t freq_hz;   /* tone pitch; 0 = rest (silence)     */
    uint16_t dur_ms;    /* how long to hold this note         */
} note_t;

/* Configure OC1A (PB1) as output and leave the timer idle/silent. */
void buzzer_init(void);

/* Play `len` notes from `seq` (which lives in PROGMEM). Cancels any current
 * sequence immediately. len == 0 is equivalent to buzzer_stop(). */
void buzzer_play(const note_t *seq, uint8_t len);

/* Advance the engine: call every loop with the current millisecond count. When
 * the active note's duration has elapsed it loads the next note (or stops at the
 * end of the sequence). */
void buzzer_tick(uint32_t now_ms);

/* Silence immediately and forget any queued sequence. */
void buzzer_stop(void);

/* True while a sequence is still playing -- lets a state block for a jingle's
 * length without a separate timer (e.g. STATE_JACKPOT). */
bool buzzer_busy(void);

#endif /* SLOT_MACHINE_BUZZER_H_ */
