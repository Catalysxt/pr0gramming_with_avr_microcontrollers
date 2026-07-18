#include <stddef.h>
#include <stdbool.h>
#include "fsm.h"

#ifdef DEBUG_UART
#include "USART.h"
#endif

/* Optional per-edge extra guard and on-enter action. Most edges need neither:
 * the shared guard is simply "the mood model's target equals this edge's `to`",
 * evaluated in fsm_step(). Keeping the function-pointer fields matches the
 * {from, guard, to, on_enter} shape from the spec and leaves a seam for edges
 * that later need a side condition or an entry effect. */
typedef bool (*fsm_guard_fn)(mood_t m);
typedef void (*fsm_action_fn)(void);

typedef struct {
    expression_t  from;
    fsm_guard_fn  guard;     /* extra condition; NULL = none */
    expression_t  to;
    fsm_action_fn on_enter;  /* entry effect; NULL = none */
} fsm_transition_t;

/* Legal edges. The trailing per-row comments (tagged for the diagram generator)
 * are the machine-readable source that tools/fsm_dot.py scrapes to draw
 * docs/fsm.svg -- keep them in sync with the table rows. Any state can be
 * startled into SURPRISED (abrupt). */
static const fsm_transition_t FSM_TABLE[] = {
    /* Startle: legal and abrupt from every state (incl. SLEEPY -> SURPRISED). */
    { EXPR_NEUTRAL,   NULL, EXPR_SURPRISED, NULL }, // @fsm NEUTRAL -> SURPRISED : spike
    { EXPR_HAPPY,     NULL, EXPR_SURPRISED, NULL }, // @fsm HAPPY -> SURPRISED : spike
    { EXPR_SAD,       NULL, EXPR_SURPRISED, NULL }, // @fsm SAD -> SURPRISED : spike
    { EXPR_CURIOUS,   NULL, EXPR_SURPRISED, NULL }, // @fsm CURIOUS -> SURPRISED : spike
    { EXPR_HESITANT,  NULL, EXPR_SURPRISED, NULL }, // @fsm HESITANT -> SURPRISED : spike
    { EXPR_NERVOUS,   NULL, EXPR_SURPRISED, NULL }, // @fsm NERVOUS -> SURPRISED : spike
    { EXPR_SLEEPY,    NULL, EXPR_SURPRISED, NULL }, // @fsm SLEEPY -> SURPRISED : spike
    { EXPR_CONTENT,   NULL, EXPR_SURPRISED, NULL }, // @fsm CONTENT -> SURPRISED : spike

    /* Startle resolves downward. */
    { EXPR_SURPRISED, NULL, EXPR_CURIOUS,   NULL }, // @fsm SURPRISED -> CURIOUS : settle
    { EXPR_SURPRISED, NULL, EXPR_NEUTRAL,   NULL }, // @fsm SURPRISED -> NEUTRAL : calm

    /* NEUTRAL is the resting hub. */
    { EXPR_NEUTRAL,   NULL, EXPR_HAPPY,     NULL }, // @fsm NEUTRAL -> HAPPY : v>=24
    { EXPR_NEUTRAL,   NULL, EXPR_SAD,       NULL }, // @fsm NEUTRAL -> SAD : v<=-12
    { EXPR_NEUTRAL,   NULL, EXPR_CURIOUS,   NULL }, // @fsm NEUTRAL -> CURIOUS : approach
    { EXPR_NEUTRAL,   NULL, EXPR_HESITANT,  NULL }, // @fsm NEUTRAL -> HESITANT : ambiguous
    { EXPR_NEUTRAL,   NULL, EXPR_SLEEPY,    NULL }, // @fsm NEUTRAL -> SLEEPY : inactive
    { EXPR_NEUTRAL,   NULL, EXPR_CONTENT,   NULL }, // @fsm NEUTRAL -> CONTENT : calm+pleasant

    /* HESITANT is the bridge: it can reach the "sharp" moods a jump must avoid. */
    { EXPR_HESITANT,  NULL, EXPR_NERVOUS,   NULL }, // @fsm HESITANT -> NERVOUS : too-close
    { EXPR_HESITANT,  NULL, EXPR_CURIOUS,   NULL }, // @fsm HESITANT -> CURIOUS : approach
    { EXPR_HESITANT,  NULL, EXPR_HAPPY,     NULL }, // @fsm HESITANT -> HAPPY : reassured
    { EXPR_HESITANT,  NULL, EXPR_SAD,       NULL }, // @fsm HESITANT -> SAD : let-down
    { EXPR_HESITANT,  NULL, EXPR_NEUTRAL,   NULL }, // @fsm HESITANT -> NEUTRAL : resolve

    /* HAPPY cannot go straight to NERVOUS -- only via HESITANT. */
    { EXPR_HAPPY,     NULL, EXPR_HESITANT,  NULL }, // @fsm HAPPY -> HESITANT : unsure
    { EXPR_HAPPY,     NULL, EXPR_CONTENT,   NULL }, // @fsm HAPPY -> CONTENT : settle
    { EXPR_HAPPY,     NULL, EXPR_NEUTRAL,   NULL }, // @fsm HAPPY -> NEUTRAL : fade

    /* CONTENT: calm + pleasant, can drift to sleep. */
    { EXPR_CONTENT,   NULL, EXPR_HAPPY,     NULL }, // @fsm CONTENT -> HAPPY : pet
    { EXPR_CONTENT,   NULL, EXPR_NEUTRAL,   NULL }, // @fsm CONTENT -> NEUTRAL : fade
    { EXPR_CONTENT,   NULL, EXPR_SLEEPY,    NULL }, // @fsm CONTENT -> SLEEPY : drowsy
    { EXPR_CONTENT,   NULL, EXPR_HESITANT,  NULL }, // @fsm CONTENT -> HESITANT : unsure

    /* CURIOUS: leans in, can tip into NERVOUS or delight. */
    { EXPR_CURIOUS,   NULL, EXPR_NERVOUS,   NULL }, // @fsm CURIOUS -> NERVOUS : too-close
    { EXPR_CURIOUS,   NULL, EXPR_HESITANT,  NULL }, // @fsm CURIOUS -> HESITANT : unsure
    { EXPR_CURIOUS,   NULL, EXPR_HAPPY,     NULL }, // @fsm CURIOUS -> HAPPY : delight
    { EXPR_CURIOUS,   NULL, EXPR_NEUTRAL,   NULL }, // @fsm CURIOUS -> NEUTRAL : lost-interest

    /* NERVOUS: backs off through the bridge or recovers curiosity. */
    { EXPR_NERVOUS,   NULL, EXPR_HESITANT,  NULL }, // @fsm NERVOUS -> HESITANT : back-off
    { EXPR_NERVOUS,   NULL, EXPR_CURIOUS,   NULL }, // @fsm NERVOUS -> CURIOUS : recover
    { EXPR_NERVOUS,   NULL, EXPR_NEUTRAL,   NULL }, // @fsm NERVOUS -> NEUTRAL : calm

    /* SAD: cheered up or withdraws to sleep. */
    { EXPR_SAD,       NULL, EXPR_NEUTRAL,   NULL }, // @fsm SAD -> NEUTRAL : cheer
    { EXPR_SAD,       NULL, EXPR_HESITANT,  NULL }, // @fsm SAD -> HESITANT : stir
    { EXPR_SAD,       NULL, EXPR_SLEEPY,    NULL }, // @fsm SAD -> SLEEPY : withdraw

    /* SLEEPY: wakes gently (a startle is handled by the abrupt edge above). */
    { EXPR_SLEEPY,    NULL, EXPR_NEUTRAL,   NULL }, // @fsm SLEEPY -> NEUTRAL : wake
    { EXPR_SLEEPY,    NULL, EXPR_CONTENT,   NULL }, // @fsm SLEEPY -> CONTENT : rested
};

#define FSM_TABLE_LEN (sizeof(FSM_TABLE) / sizeof(FSM_TABLE[0]))

static expression_t s_current;

void fsm_init(void) {
    s_current = EXPR_NEUTRAL;
}

expression_t fsm_current(void) {
    return s_current;
}

#ifdef DEBUG_UART
static const char *EXPR_NAME[EXPR_COUNT] = {
    "NEUTRAL", "HAPPY", "SAD", "CURIOUS", "HESITANT",
    "NERVOUS", "SLEEPY", "SURPRISED", "CONTENT"
};
void fsm_on_transition(expression_t from, expression_t to) {
    printString(EXPR_NAME[from]);
    printString(" -> ");
    printString(EXPR_NAME[to]);
    transmitByte('\r');
    transmitByte('\n');
}
#else
void fsm_on_transition(expression_t from, expression_t to) {
    (void)from;   /* DEBUG_UART seam: no-op in a normal build. */
    (void)to;
}
#endif

/* Return the table edge from `from` to `to`, or NULL if that jump is illegal. */
static const fsm_transition_t *find_edge(expression_t from, expression_t to) {
    for (uint8_t i = 0; i < FSM_TABLE_LEN; i++) {
        if (FSM_TABLE[i].from == from && FSM_TABLE[i].to == to) {
            return &FSM_TABLE[i];
        }
    }
    return NULL;
}

static void take_edge(const fsm_transition_t *e) {
    fsm_on_transition(s_current, e->to);
    if (e->on_enter != NULL) {
        e->on_enter();
    }
    s_current = e->to;
}

/* Try to take a legal edge to `to`; return true if one was taken. */
static bool try_edge(expression_t to, mood_t m) {
    const fsm_transition_t *e = find_edge(s_current, to);
    if (e != NULL && (e->guard == NULL || e->guard(m))) {
        take_edge(e);
        return true;
    }
    return false;
}

expression_t fsm_step(mood_t m) {
    expression_t target = mood_classify(m, s_current);
    if (target == s_current) {
        return s_current;
    }

    /* Direct legal edge wins. Otherwise bridge toward the target: HESITANT is
     * the preferred crossing (it reaches the sharp moods), NEUTRAL the fallback.
     * Over successive ticks this walks an illegal jump through a legal path. */
    if (try_edge(target, m)) {
        return s_current;
    }
    if (s_current != EXPR_HESITANT && try_edge(EXPR_HESITANT, m)) {
        return s_current;
    }
    if (s_current != EXPR_NEUTRAL && try_edge(EXPR_NEUTRAL, m)) {
        return s_current;
    }
    return s_current;   /* no legal step available this tick */
}
