/*
 * encoder.c
 *
 * Purpose : Bounce-immune quadrature decode of a KY-040 via a transition
 *           lookup table, entirely in the PCINT2 ISR. One detent = 4 valid
 *           quarter-steps; fractional steps accumulate so a slow, bouncy
 *           turn still resolves to exactly ±1 detent.
 * Hardware: CLK, DT (pin_t from hardware_connections.h), on PORTD/PCINT2.
 * Datasheet: KY-040 Gray-code tables (docs/KY-040.pdf p.1);
 *            ATmega328P §12.2.3 (PCICR/PCIE2), §12.2.4 (PCMSK2).
 *
 * State encoding: state = (CLK << 1) | DT, so the pair walks the Gray
 * sequence 00 -> 01 -> 11 -> 10 -> 00 (one way) or the reverse.
 *
 * The 16-entry table is indexed by (prev_state << 2) | new_state:
 *   +1  a legal step in the clockwise direction
 *   -1  a legal step counter-clockwise
 *    0  no change, or an illegal 2-bit jump (bounce) -> rejected
 * Legal Gray transitions only ever flip one bit, so the four "diagonal"
 * jumps (00<->11, 01<->10) are impossible and score 0. This is what makes
 * the decoder immune to the contact bounce that breaks naive edge counting.
 */

#include "encoder.h"
#include "hardware_connections.h"   /* EMBER_ENC_PCINT_MASK */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#define DETENT_SUBSTEPS  4   /* KY-040: 4 quarter-steps per physical detent */

static const int8_t k_qdec[16] = {
    /* prev\new  00  01  10  11  (columns are new_state)               */
    /* 00 */      0, -1, +1,  0,
    /* 01 */     +1,  0,  0, -1,
    /* 10 */     -1,  0,  0, +1,
    /* 11 */      0, +1, -1,  0,
};

static pin_t s_clk;
static pin_t s_dt;

static volatile uint8_t s_prev_state = 0;   /* last (CLK<<1|DT)             */
static volatile int8_t  s_substeps   = 0;   /* -3..+3 quarter-step remainder */
static volatile int8_t  s_detents    = 0;   /* whole detents pending drain   */

static uint8_t read_state(void)
{
    uint8_t s = 0u;
    if (pin_read(s_clk)) { s |= 0x02u; }
    if (pin_read(s_dt))  { s |= 0x01u; }
    return s;
}

void encoder_init(pin_t clk, pin_t dt)
{
    s_clk = clk;
    s_dt  = dt;
    pin_set_input(s_clk, true);   /* input + pull-up (KY-040 idles HIGH) */
    pin_set_input(s_dt,  true);

    s_prev_state = read_state();
    s_substeps   = 0;
    s_detents    = 0;

    /* Enable pin-change interrupts for CLK/DT (and SW, per the mask) so a
     * twist is caught the instant a line moves. PCMSK2 bit n == PCINT(16+n)
     * == PDn, so the mask lines up with the PORTD bits. Datasheet §12.2.4. */
    PCMSK2 |= EMBER_ENC_PCINT_MASK;
    PCICR  |= (1u << PCIE2);       /* enable the PCINT[23:16] group */
}

int8_t encoder_consume_delta(void)
{
    int8_t d;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        d = s_detents;
        s_detents = 0;
    }
    return d;
}

/*
 * PCINT2 fires on ANY change of PD0..PD7 we enabled (CLK, DT, or the SW
 * button). We only decode CLK/DT here; a lone SW edge just re-reads the same
 * state and scores 0. Kept short per the <20-line ISR rule.
 */
ISR(PCINT2_vect)
{
    uint8_t new_state = read_state();
    uint8_t idx = (uint8_t)((s_prev_state << 2) | new_state);
    s_prev_state = new_state;

    s_substeps += k_qdec[idx];

    if (s_substeps >= DETENT_SUBSTEPS) {
        s_detents++;
        s_substeps -= DETENT_SUBSTEPS;
    } else if (s_substeps <= -DETENT_SUBSTEPS) {
        s_detents--;
        s_substeps += DETENT_SUBSTEPS;
    }
}
