#include <avr/pgmspace.h>
#include "animator.h"
#include "expression.h"
#include "expressions.h"     /* generated EXPRESSIONS[] keyframe table (PROGMEM) */
#include "idle_motions.h"
#include "dotmatrix.h"
#include "prng.h"

/* ms -> animation ticks helper (compile-time where possible). */
#define MS_TO_TICKS(ms) ((uint16_t)(((uint32_t)(ms) * ANIM_HZ) / 1000u))
#define BLINK_TICKS         MS_TO_TICKS(BLINK_MS)
#define SACCADE_TICKS       MS_TO_TICKS(SACCADE_MS)
#define BLINK_MEAN_TICKS    MS_TO_TICKS(BLINK_MEAN_MS)
#define BLINK_JITTER_TICKS  MS_TO_TICKS(BLINK_JITTER_MS)
#define SACCADE_MIN_TICKS   MS_TO_TICKS(SACCADE_MIN_MS)
#define SACCADE_SPAN_TICKS  MS_TO_TICKS(SACCADE_SPAN_MS)

static max7219_chain_t s_chain;

static expression_t s_current_expr = EXPR_NEUTRAL;
static expression_t s_target_expr  = EXPR_NEUTRAL;

static uint8_t s_base[DOT_ROWS];      /* resolved base (post-tween/loop), no idle */
static uint8_t s_display[DOT_ROWS];   /* composed frame handed to the display */

/* Tween state. */
static uint8_t s_tween_active;
static uint8_t s_tween_k;
static uint8_t s_morph_from[DOT_ROWS];
static uint8_t s_morph_to[DOT_ROWS];

/* Looping-keyframe state (e.g. NERVOUS eye-dart). */
static uint8_t s_frame_index;
static uint8_t s_frame_timer;
static uint8_t s_frame_count;
static uint8_t s_hold_ticks;

/* Idle-motion schedule (all in animation ticks). */
static uint32_t s_tick;
static uint32_t s_next_blink_tick;
static uint8_t  s_blink_left;
static uint32_t s_next_saccade_tick;
static uint8_t  s_saccade_left;
static int8_t   s_saccade_dir = 1;

/* ---- PROGMEM keyframe accessors ---- */
static void load_keyframe(expression_t e, uint8_t idx, uint8_t out[DOT_ROWS]) {
    for (uint8_t r = 0; r < DOT_ROWS; r++) {
        out[r] = pgm_read_byte(&EXPRESSIONS[e].frames[idx][r]);
    }
}
static uint8_t expr_frame_count(expression_t e) {
    return pgm_read_byte(&EXPRESSIONS[e].frame_count);
}
static uint8_t expr_hold(expression_t e) {
    return pgm_read_byte(&EXPRESSIONS[e].hold_ticks);
}

/* ---- tween ---- */
/* Reveal the top `reveal` MSBs of diff = from^to, flipping them onto `from`. */
static uint8_t morph_blend(uint8_t from, uint8_t to, uint8_t reveal) {
    uint8_t diff = from ^ to;
    uint8_t row = from;
    for (uint8_t pos = 0; pos < 8u && pos < reveal; pos++) {
        uint8_t mask = (uint8_t)(0x80u >> pos);   /* MSB-first, stable order */
        if (diff & mask) {
            row ^= mask;
        }
    }
    return row;
}

/* ---- idle-motion scheduling ---- */
static void schedule_next_blink(void) {
    /* Poisson-ish: mean interval +/- uniform jitter, so blinks never fall on a
     * fixed beat. */
    uint16_t jitter = prng_range((uint16_t)(2u * BLINK_JITTER_TICKS + 1u));
    s_next_blink_tick = s_tick + BLINK_MEAN_TICKS + jitter - BLINK_JITTER_TICKS;
}
static void schedule_next_saccade(void) {
    s_next_saccade_tick = s_tick + SACCADE_MIN_TICKS + prng_range(SACCADE_SPAN_TICKS);
}

static void update_idle_timers(void) {
    if (s_blink_left > 0) {
        s_blink_left--;
    } else if (s_tick >= s_next_blink_tick) {
        s_blink_left = (uint8_t)BLINK_TICKS;
        schedule_next_blink();
    }

    if (s_saccade_left > 0) {
        s_saccade_left--;
    } else if (s_tick >= s_next_saccade_tick) {
        s_saccade_left = (uint8_t)SACCADE_TICKS;
        s_saccade_dir  = (prng_next() & 1u) ? 1 : -1;
        schedule_next_saccade();
    }
}

/* ---- idle-motion application (operate on the composed frame) ---- */
static void apply_blink(uint8_t disp[DOT_ROWS]) {
    for (uint8_t r = EYE_ROW_TOP; r <= EYE_ROW_BOT; r++) {
        disp[r] = 0;                       /* eyes shut */
    }
    disp[EYE_ROW_BOT] = EYE_MASK;          /* a thin closed-eye line */
}

static void apply_saccade(uint8_t disp[DOT_ROWS], int8_t dir) {
    for (uint8_t r = EYE_ROW_TOP; r <= EYE_ROW_BOT; r++) {
        disp[r] = (dir > 0) ? (uint8_t)(disp[r] >> 1)   /* dart right */
                            : (uint8_t)(disp[r] << 1);  /* dart left  */
    }
}

static void apply_breathing(uint8_t disp[DOT_ROWS]) {
    uint8_t idx = (uint8_t)((s_tick / BREATHING_STEP_TICKS) % BREATHING_LEN);
    int8_t  off = (int8_t)pgm_read_byte(&BREATHING_CURVE[idx]);
    if (off == 0) {
        return;
    }
    const uint8_t span = MOUTH_ROW_BOT - MOUTH_ROW_TOP + 1u;
    uint8_t tmp[8];
    for (uint8_t i = 0; i < span; i++) {
        tmp[i] = disp[MOUTH_ROW_TOP + i];
    }
    for (uint8_t i = 0; i < span; i++) {
        int8_t src = (int8_t)i - off;      /* off>0 sways the mouth downward */
        disp[MOUTH_ROW_TOP + i] = (src >= 0 && src < (int8_t)span) ? tmp[src] : 0;
    }
}

static void compose(void) {
    for (uint8_t r = 0; r < DOT_ROWS; r++) {
        s_display[r] = s_base[r];
    }

    /* Which expression's idle rules apply -- the target while tweening in. */
    expression_t e = s_tween_active ? s_target_expr : s_current_expr;

    if (e == EXPR_SURPRISED) {
        return;                            /* frozen: no idle motion at all */
    }

    uint8_t sleepy = (e == EXPR_SLEEPY);

    /* Breathing sways the mouth only in the calm resting moods. */
    if (!sleepy && (e == EXPR_NEUTRAL || e == EXPR_CONTENT)) {
        apply_breathing(s_display);
    }
    /* Saccades are suppressed while sleepy (drowsy eyes do not dart). */
    if (!sleepy && s_saccade_left > 0) {
        apply_saccade(s_display, s_saccade_dir);
    }
    /* Blink is applied last so it overrides the eye rows; it still runs (in fact
     * dominates) while sleepy. */
    if (s_blink_left > 0) {
        apply_blink(s_display);
    }
}

/* ---- public API ---- */
void animator_init(max7219_chain_t chain) {
    s_chain = chain;
    s_current_expr = EXPR_NEUTRAL;
    s_target_expr  = EXPR_NEUTRAL;
    s_frame_count  = expr_frame_count(EXPR_NEUTRAL);
    s_hold_ticks   = expr_hold(EXPR_NEUTRAL);
    s_frame_index  = 0;
    s_frame_timer  = 0;
    s_tween_active = 0;
    s_tick         = 0;
    s_blink_left   = 0;
    s_saccade_left = 0;
    load_keyframe(EXPR_NEUTRAL, 0, s_base);
    schedule_next_blink();
    schedule_next_saccade();
    compose();
}

void animator_set_target(expression_t expr) {
    if (expr >= EXPR_COUNT || expr == s_target_expr) {
        return;
    }
    s_target_expr = expr;
    for (uint8_t r = 0; r < DOT_ROWS; r++) {
        s_morph_from[r] = s_base[r];       /* morph from whatever is on screen */
    }
    load_keyframe(expr, 0, s_morph_to);
    s_tween_active = 1;
    s_tween_k      = 0;
}

void animator_force_blink(void) {
    s_blink_left = (uint8_t)BLINK_TICKS;
}

void animator_tick(void) {
    s_tick++;

    if (s_tween_active) {
        s_tween_k++;
        uint8_t reveal = (uint8_t)(((uint16_t)s_tween_k * 8u) / TWEEN_TICKS);
        for (uint8_t r = 0; r < DOT_ROWS; r++) {
            s_base[r] = morph_blend(s_morph_from[r], s_morph_to[r], reveal);
        }
        if (s_tween_k >= TWEEN_TICKS) {
            s_tween_active = 0;
            s_current_expr = s_target_expr;
            s_frame_count  = expr_frame_count(s_current_expr);
            s_hold_ticks   = expr_hold(s_current_expr);
            s_frame_index  = 0;
            s_frame_timer  = 0;
            load_keyframe(s_current_expr, 0, s_base);
        }
    } else if (s_frame_count > 1) {
        /* Cycle the looping keyframes of the current expression. */
        if (++s_frame_timer >= s_hold_ticks) {
            s_frame_timer = 0;
            s_frame_index = (uint8_t)((s_frame_index + 1u) % s_frame_count);
            load_keyframe(s_current_expr, s_frame_index, s_base);
        }
    }

    update_idle_timers();
    compose();
}

void animator_flush(void) {
    dotmatrix_set_rows(s_display);
    dotmatrix_flush(s_chain);
}
