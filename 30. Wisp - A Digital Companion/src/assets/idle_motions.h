#ifndef DIGITAL_COMPANION_IDLE_MOTIONS_H_
#define DIGITAL_COMPANION_IDLE_MOTIONS_H_

#include <stdint.h>
#include <avr/pgmspace.h>

/* Idle micro-motion parameters, composited on top of the base expression by the
 * animator every frame. These are the "signs of life" that keep a static face
 * from looking dead: an occasional blink, small eye darts (saccades), and a slow
 * breathing sway of the mouth. Kept as data here so the motion feel can be tuned
 * without touching animator.c logic.
 *
 * Face layout convention (matching the assets_src art): eyes occupy rows 1..3,
 * mouth rows 5..7, row 0 = top, bit 7 = leftmost column. */

/* ---- Eye / mouth regions ---- */
#define EYE_ROW_TOP    1u
#define EYE_ROW_BOT    3u
/* Column mask of both eyes (cols 1,2 and 5,6) -- used to draw the closed-eye
 * line during a blink and to bound the horizontal saccade shift. */
#define EYE_MASK       0x66u
#define MOUTH_ROW_TOP  5u
#define MOUTH_ROW_BOT  7u

/* ---- Blink: full eye-close for ~120 ms, Poisson-ish arrivals (mean ~4 s) ----
 * The animator draws the next blink at now + BLINK_MEAN_MS +/- jitter from the
 * PRNG, so blinks never land on a fixed beat. */
#define BLINK_MS        120u
#define BLINK_MEAN_MS   4000u
#define BLINK_JITTER_MS 2500u   /* +/- range added to the mean via the PRNG */

/* ---- Saccade: shift the eyes +/-1 column for ~200 ms, every 2-6 s ---- */
#define SACCADE_MS       200u
#define SACCADE_MIN_MS  2000u
#define SACCADE_SPAN_MS 4000u   /* interval = MIN + rand()%SPAN -> 2..6 s */

/* ---- Breathing: slow +/-1 row sway of the mouth over ~3 s (calm states only).
 * The animator walks BREATHING_CURVE one entry every BREATHING_STEP_TICKS and
 * shifts the mouth rows by the signed offset it reads. 10 entries * 7 ticks /
 * 25 Hz ~= 2.8 s per cycle. */
#define BREATHING_LEN         10u
#define BREATHING_STEP_TICKS   7u
static const int8_t BREATHING_CURVE[BREATHING_LEN] PROGMEM = {
    0, 0, 1, 1, 1, 0, 0, -1, -1, -1
};

#endif /* DIGITAL_COMPANION_IDLE_MOTIONS_H_ */
