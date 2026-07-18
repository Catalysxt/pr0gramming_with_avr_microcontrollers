/* Host-side unit test: mood model + sensor interpretation.
 *
 * Drives mood.c and sensors.c (both pure C, no AVR headers) through scripted
 * sensor events and checks the (valence, arousal) trajectory against a golden
 * table. SPI / MAX7221 / timers are never touched -- see tests/README.md. */
#include <stdio.h>
#include "mood.h"
#include "sensors.h"

static int g_fail = 0;

#define CHECK_VA(exp_v, exp_a) do {                                            \
    mood_t _m = mood_get();                                                   \
    if (_m.valence != (exp_v) || _m.arousal != (exp_a)) {                     \
        printf("FAIL %s:%d expected (%d,%d) got (%d,%d)\n", __FILE__, __LINE__,\
               (exp_v), (exp_a), _m.valence, _m.arousal);                     \
        g_fail++;                                                             \
    }                                                                         \
} while (0)

static void test_pet_gesture(void) {
    mood_init();
    sensors_init(0);
    /* Three pets inside the 2 s window -> the third fires the "force HAPPY"
     * bonus (+30 valence). Per pet: v+=12, a+=4. */
    sensors_pet(0);      CHECK_VA(12, 4);
    sensors_pet(100);    CHECK_VA(24, 8);
    sensors_pet(200);    CHECK_VA(64, 12);   /* 36+30 = 66 -> clamped to +64 */

    /* Linear decay: one unit per axis per 100 ms step, toward zero. */
    for (int i = 0; i < 10; i++) {
        mood_decay();
    }
    CHECK_VA(54, 2);
}

static void test_proximity_trend(void) {
    mood_init();
    sensors_init(0);
    /* First reading only primes the trend history (no nudge). */
    sensors_proximity_update(100, 1000);  CHECK_VA(0, 0);
    /* Approaching (10 cm closer each step, >= APPROACH_CM): arousal climbs. */
    sensors_proximity_update(90, 1060);   CHECK_VA(2, 10);
    sensors_proximity_update(80, 1120);   CHECK_VA(4, 20);   /* CURIOUS region */
    /* A sudden jump to 5 cm: SPIKE (a+=48, v-=4) AND too-close (a+=12, v-=10). */
    sensors_proximity_update(5, 1180);    CHECK_VA(-10, 64); /* arousal clamps */
}

static void test_out_of_range_resets_trend(void) {
    mood_init();
    sensors_init(0);
    sensors_proximity_update(40, 1000);   /* object seen, primes history */
    sensors_proximity_update(200, 1100);  /* nothing in range -> history dropped */
    /* Next in-range reading must NOT be diffed against the stale 40 cm. */
    sensors_proximity_update(38, 1200);   CHECK_VA(0, 0);
}

int main(void) {
    test_pet_gesture();
    test_proximity_trend();
    test_out_of_range_resets_trend();
    if (g_fail == 0) {
        printf("test_mood: all checks passed\n");
        return 0;
    }
    printf("test_mood: %d check(s) FAILED\n", g_fail);
    return 1;
}
