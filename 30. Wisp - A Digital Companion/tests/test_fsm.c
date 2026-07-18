/* Host-side unit test: FSM region selection, hysteresis, and legal paths.
 *
 * Drives mood.c (classifier) and fsm.c (transition table) with constructed
 * (v, a) points. No hardware is involved -- see tests/README.md. */
#include <stdio.h>
#include "mood.h"
#include "fsm.h"

static int g_fail = 0;

#define CHECK_EQ(actual, expect) do {                                         \
    long _a = (long)(actual), _e = (long)(expect);                            \
    if (_a != _e) {                                                           \
        printf("FAIL %s:%d %s == %s (%ld != %ld)\n", __FILE__, __LINE__,      \
               #actual, #expect, _a, _e);                                     \
        g_fail++;                                                             \
    }                                                                         \
} while (0)

static mood_t va(int8_t v, int8_t a) {
    mood_t m = { v, a };
    return m;
}

/* Step the FSM until it settles (or a step budget is hit), holding `m` fixed. */
static expression_t settle(mood_t m) {
    expression_t prev = fsm_current();
    for (int i = 0; i < 8; i++) {
        expression_t now = fsm_step(m);
        if (now == prev) {
            break;
        }
        prev = now;
    }
    return fsm_current();
}

static void test_hysteresis(void) {
    /* Enters HAPPY at the v>=24 boundary from NEUTRAL. */
    CHECK_EQ(mood_classify(va(24, 0), EXPR_NEUTRAL), EXPR_HAPPY);
    /* Once HAPPY, stays HAPPY down to 8 below the boundary (sticky band). */
    CHECK_EQ(mood_classify(va(20, 0), EXPR_HAPPY), EXPR_HAPPY);
    CHECK_EQ(mood_classify(va(16, 0), EXPR_HAPPY), EXPR_HAPPY);
    /* Below the hysteresis floor it finally leaves HAPPY. */
    CHECK_EQ(mood_classify(va(15, 0), EXPR_HAPPY), EXPR_NEUTRAL);
}

static void test_illegal_transition_bridges(void) {
    /* Get into HAPPY. */
    fsm_init();
    CHECK_EQ(settle(va(40, 0)), EXPR_HAPPY);

    /* Now demand NERVOUS (v<=-12, a>=20). HAPPY->NERVOUS is illegal, so the very
     * first step must route through HESITANT, not snap to NERVOUS. */
    mood_t nervous = va(-40, 30);
    CHECK_EQ(fsm_step(nervous), EXPR_HESITANT);
    /* The next step completes the bridge into NERVOUS. */
    CHECK_EQ(fsm_step(nervous), EXPR_NERVOUS);
}

static void test_surprise_is_abrupt_from_sleepy(void) {
    fsm_init();
    /* Drift into SLEEPY (a<=-32). */
    CHECK_EQ(settle(va(0, -40)), EXPR_SLEEPY);
    /* A startle (a>=48) jumps straight to SURPRISED in one step. */
    CHECK_EQ(fsm_step(va(0, 50)), EXPR_SURPRISED);
}

int main(void) {
    test_hysteresis();
    test_illegal_transition_bridges();
    test_surprise_is_abrupt_from_sleepy();
    if (g_fail == 0) {
        printf("test_fsm: all checks passed\n");
        return 0;
    }
    printf("test_fsm: %d check(s) FAILED\n", g_fail);
    return 1;
}
