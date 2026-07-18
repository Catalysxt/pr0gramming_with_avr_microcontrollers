#ifndef DIGITAL_COMPANION_EXPRESSION_H_
#define DIGITAL_COMPANION_EXPRESSION_H_

/* The emotional-expression enum, shared by mood.c, fsm.c, animator.c and face.c.
 *
 * It lives in its own tiny header (rather than in face.h) so the pure logic
 * modules -- mood and fsm -- and their host-side unit tests can name expressions
 * without dragging in the display types (max7219_chain_t etc.). NOTE the naming:
 * this file is the enum; src/assets/expressions.h (plural) is the generated
 * keyframe bitmap table, indexed by these constants. EXPR_COUNT must stay last. */

typedef enum {
    EXPR_NEUTRAL = 0,
    EXPR_HAPPY,
    EXPR_SAD,
    EXPR_CURIOUS,
    EXPR_HESITANT,
    EXPR_NERVOUS,
    EXPR_SLEEPY,
    EXPR_SURPRISED,
    EXPR_CONTENT,
    EXPR_COUNT
} expression_t;

#endif /* DIGITAL_COMPANION_EXPRESSION_H_ */
