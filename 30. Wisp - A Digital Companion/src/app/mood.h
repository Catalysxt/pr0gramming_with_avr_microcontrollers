#ifndef DIGITAL_COMPANION_MOOD_H_
#define DIGITAL_COMPANION_MOOD_H_

#include <stdint.h>
#include "expression.h"

/* Numeric mood model: the layer between raw sensor events and the FSM.
 *
 * Rather than a chain of if/elses, the companion's affect is a point in a 2-D
 * (valence, arousal) space that sensors NUDGE and that continuously DECAYS back
 * toward neutral. The FSM then reads the point and picks the expression whose
 * region contains it, with hysteresis so a point sitting on a boundary does not
 * flicker between two moods.
 *
 *   valence : -64 (sad / unpleasant) ... +64 (happy / pleasant)
 *   arousal : -64 (calm / drowsy)    ... +64 (alert / excited)
 *
 * (v, a) region map (before the +/-8 hysteresis margin is applied). Priority
 * (top wins on overlap): SURPRISED, SLEEPY, NERVOUS, CURIOUS, HAPPY, CONTENT,
 * HESITANT, SAD, NEUTRAL.
 *
 *   SURPRISED  a >= +48                         (sudden proximity spike; abrupt)
 *   SLEEPY     a <= -32                          (sustained inactivity)
 *   NERVOUS    v <= -12 && a >= +20             (object too close)
 *   CURIOUS    v >=  0  && a >= +20             (object approaching)
 *   HAPPY      v >= +24 && -8 <= a < +40        (petting)
 *   CONTENT    v >= +8  && a <= -8              (pleasant + calm)
 *   HESITANT   |v| < 12 && +8 <= a < +20        (ambiguous -- the bridge state)
 *   SAD        v <= -12 && -32 < a < +8         (boredom)
 *   NEUTRAL    everything else                   (resting deadzone)
 *
 * Illegal-transition enforcement (e.g. HAPPY -> NERVOUS must route through
 * HESITANT) lives in fsm.c; this module only classifies a point. */

#define MOOD_MIN        (-64)
#define MOOD_MAX        ( 64)
#define MOOD_HYSTERESIS (   8)   /* units a point must cross a boundary to switch */

typedef struct {
    int8_t valence;
    int8_t arousal;
} mood_t;

/* Reset the model to neutral (0, 0). */
void mood_init(void);

/* Add a sensor event's signed delta to each axis, clamped to [MOOD_MIN,MOOD_MAX]. */
void mood_nudge(int8_t d_valence, int8_t d_arousal);

/* One decay step toward (0, 0): each axis moves one unit toward zero. Call every
 * 100 ms. Linear (not exponential) decay is chosen deliberately -- see mood.c. */
void mood_decay(void);

/* Current (v, a) point. */
mood_t mood_get(void);

/* Classify a point into an expression, honouring the +/-MOOD_HYSTERESIS margin
 * relative to `current` (the expression currently shown). Pure function: used by
 * fsm.c on the live point and by the host tests on scripted points. */
expression_t mood_classify(mood_t m, expression_t current);

#endif /* DIGITAL_COMPANION_MOOD_H_ */
