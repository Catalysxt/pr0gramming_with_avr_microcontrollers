#include "mood.h"

static mood_t s_mood;

/* ---- axis helpers ---- */

static int8_t clamp_axis(int16_t x) {
    if (x > MOOD_MAX) {
        return (int8_t)MOOD_MAX;
    }
    if (x < MOOD_MIN) {
        return (int8_t)MOOD_MIN;
    }
    return (int8_t)x;
}

static int8_t step_toward_zero(int8_t x) {
    if (x > 0) {
        return (int8_t)(x - 1);
    }
    if (x < 0) {
        return (int8_t)(x + 1);
    }
    return 0;
}

void mood_init(void) {
    s_mood.valence = 0;
    s_mood.arousal = 0;
}

void mood_nudge(int8_t d_valence, int8_t d_arousal) {
    s_mood.valence = clamp_axis((int16_t)s_mood.valence + d_valence);
    s_mood.arousal = clamp_axis((int16_t)s_mood.arousal + d_arousal);
}

void mood_decay(void) {
    /* Linear decay: subtract sign(x) each step. Chosen over exponential
     * (x -= x>>k) because (a) it is one branch and no multiply, (b) it gives a
     * predictable, constant "cool-off slope" -- a strong event fades in a known
     * number of 100 ms steps (e.g. a +40 pet fades to the HAPPY floor of +24 in
     * ~1.6 s), which makes the mood easy to reason about and to unit-test against
     * a golden trajectory. Exponential decay would also work and feels natural
     * for big spikes, but its long tail lets tiny residuals linger near zero and
     * complicates the golden tables; linear reaches exactly (0,0) and stops. */
    s_mood.valence = step_toward_zero(s_mood.valence);
    s_mood.arousal = step_toward_zero(s_mood.arousal);
}

mood_t mood_get(void) {
    return s_mood;
}

/* ---- region classification ----
 *
 * in_region() tests one expression's predicate with every numeric bound loosened
 * by `margin`: a larger margin makes the region bigger, so passing the CURRENT
 * expression's region a +MOOD_HYSTERESIS margin makes it "sticky" -- the point
 * must move a full margin past a boundary before we abandon the current mood. */

static int in_region(expression_t e, int8_t v, int8_t a, int8_t m) {
    switch (e) {
    case EXPR_SURPRISED: return a >= 48 - m;
    case EXPR_SLEEPY:    return a <= -32 + m;
    case EXPR_NERVOUS:   return v <= -12 + m && a >= 20 - m;
    case EXPR_CURIOUS:   return v >= 0 - m  && a >= 20 - m;
    case EXPR_HAPPY:     return v >= 24 - m && a >= -8 - m && a < 40 + m;
    case EXPR_CONTENT:   return v >= 8 - m  && a <= -8 + m;
    case EXPR_HESITANT:  return v > -12 - m && v < 12 + m && a >= 8 - m && a < 20 + m;
    case EXPR_SAD:       return v <= -12 + m && a > -32 - m && a < 8 + m;
    /* NEUTRAL is a BOUNDED resting deadzone near the origin -- it must not be a
     * catch-all here, or the hysteresis "stickiness" test would trap the machine
     * in NEUTRAL permanently (every point would count as "still neutral"). Gaps
     * between regions are instead swept up by the explicit fallback in
     * classify_core(). */
    case EXPR_NEUTRAL:   return v > -12 - m && v < 12 + m && a > -8 - m && a < 8 + m;
    default:             return 0;
    }
}

/* Highest-priority region (strict, margin 0) that contains the point. */
static const expression_t PRIORITY[] = {
    EXPR_SURPRISED, EXPR_SLEEPY, EXPR_NERVOUS, EXPR_CURIOUS,
    EXPR_HAPPY, EXPR_CONTENT, EXPR_HESITANT, EXPR_SAD, EXPR_NEUTRAL
};

static expression_t classify_core(int8_t v, int8_t a) {
    for (uint8_t i = 0; i < sizeof(PRIORITY) / sizeof(PRIORITY[0]); i++) {
        if (in_region(PRIORITY[i], v, a, 0)) {
            return PRIORITY[i];
        }
    }
    return EXPR_NEUTRAL;
}

expression_t mood_classify(mood_t m, expression_t current) {
    int8_t v = m.valence;
    int8_t a = m.arousal;

    /* SURPRISED is an abrupt override -- a startle jumps in regardless of where
     * we were (SLEEPY -> SURPRISED is explicitly allowed). */
    if (in_region(EXPR_SURPRISED, v, a, 0)) {
        return EXPR_SURPRISED;
    }

    /* Sticky: stay in the current mood while the point is still inside its
     * region widened by the hysteresis margin. This is the anti-flicker band. */
    if (current != EXPR_SURPRISED && current < EXPR_COUNT &&
        in_region(current, v, a, MOOD_HYSTERESIS)) {
        return current;
    }

    /* Otherwise commit to the strict highest-priority region. */
    return classify_core(v, a);
}
