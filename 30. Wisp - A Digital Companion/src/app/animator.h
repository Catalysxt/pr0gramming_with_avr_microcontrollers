#ifndef DIGITAL_COMPANION_ANIMATOR_H_
#define DIGITAL_COMPANION_ANIMATOR_H_

#include <stdint.h>
#include "expression.h"
#include "max7219.h"

/* Animation engine: tween + idle micro-motion compositor.
 *
 * Owns the 8-byte frame buffer; nothing else writes pixels. It (1) tweens
 * between expressions with a per-row XOR-morph instead of snapping, and (2)
 * composites idle micro-motion -- blink, saccade, breathing -- on top of the
 * base expression every frame so a resting face still looks alive.
 *
 * --- Two decoupled rates (justified) ---
 * REFRESH_HZ = 75 (display flush). Above the ~60 Hz flicker-fusion threshold so
 *   the multiplexed matrix looks rock steady. One flush is 8 x 16-bit SPI writes
 *   ~= 130 us at the 1 MHz SPI clock plus CS overhead -- a tiny slice of the
 *   13.3 ms period, so refresh never starves the rest of the loop.
 * ANIM_HZ = 25 (tween/idle/fsm tick). Cinema-smooth for motion (film is 24 fps)
 *   yet coarse enough that the per-tick mood/fsm/compose work is negligible on a
 *   16 MHz AVR. Decoupling the two means brightness stays even (fast refresh)
 *   while animation math runs only as often as the eye needs it (slow tick).
 *
 * --- Tween: per-row XOR-morph with dwell (vs. per-pixel PWM crossfade) ---
 * For each row, diff = from ^ to; over TWEEN_TICKS sub-ticks we progressively
 * flip diff's bits MSB-first (a stable, deterministic order), so pixels that
 * change "peel" across the row in ~200 ms. We chose this over a per-pixel PWM
 * brightness crossfade because the MAX7221 has ONE global intensity register --
 * it cannot dim individual dots, so a true per-pixel fade would need software
 * bit-angle modulation across the whole panel every refresh (heavy, and it
 * fights the chip's own multiplexing). XOR-morph is purely combinational on the
 * row bytes, costs a handful of instructions, and reads as a deliberate,
 * characterful "wipe" rather than a mushy dissolve. MSB-first (over e.g. a Bayer
 * dither order) keeps it visually coherent -- changes sweep consistently from
 * the left edge -- and is trivial to reason about and test. */

#define ANIM_HZ      25u
#define REFRESH_HZ   75u
#define TWEEN_TICKS   5u                 /* 5 ticks @ 25 Hz = 200 ms (target 150-250) */
#define ANIM_TICK_MS (1000u / ANIM_HZ)   /* 40 ms */
#define REFRESH_MS   (1000u / REFRESH_HZ)/* 13 ms */

/* Bind the display chain and paint the initial (NEUTRAL) frame. Seed the PRNG
 * before calling so the first idle-motion schedule is randomised. */
void animator_init(max7219_chain_t chain);

/* Begin a tween toward `expr` (ignored if already the target). */
void animator_set_target(expression_t expr);

/* Trigger a blink immediately (used by face_blink()). */
void animator_force_blink(void);

/* Advance the tween, idle-motion schedule and looping keyframes, then compose
 * the next frame. Call at ANIM_HZ (every ANIM_TICK_MS). */
void animator_tick(void);

/* Push the composed frame to the MAX7221. Call at REFRESH_HZ (every REFRESH_MS). */
void animator_flush(void);

#endif /* DIGITAL_COMPANION_ANIMATOR_H_ */
