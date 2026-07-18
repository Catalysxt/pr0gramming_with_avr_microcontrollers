#include "sensors.h"
#include "mood.h"

/* ---- interaction thresholds ---- */
#define TOO_CLOSE_CM     10u    /* nearer than this reads as threatening */
#define APPROACH_CM       4     /* min per-sample decrease that counts as approaching */
#define SPIKE_CM         20     /* a sudden jump this much closer = startle */

/* ---- pet gesture ---- */
#define PET_WINDOW_MS  2000u
#define PET_GESTURE_N     3u

/* ---- inactivity (replaces the removed LDR "sleepy" driver) ---- */
#define INACTIVE_SAD_MS    30000UL
#define INACTIVE_SLEEPY_MS 60000UL

static uint16_t s_last_dist;
static uint8_t  s_have_last;
static uint32_t s_last_activity_ms;
static uint8_t  s_pet_count;
static uint32_t s_pet_window_start;

void sensors_init(uint32_t now_ms) {
    s_have_last        = 0;
    s_last_dist        = 0;
    s_last_activity_ms = now_ms;
    s_pet_count        = 0;
    s_pet_window_start = now_ms;
}

void sensors_proximity_update(uint16_t dist_cm, uint32_t now_ms) {
    uint8_t object = (dist_cm <= SENSORS_RANGE_CM);

    if (!object) {
        /* Nothing in range: drop the trend history so a later approach is
         * measured from a fresh contact, not a stale far reading. */
        s_have_last = 0;
        return;
    }

    s_last_activity_ms = now_ms;

    if (s_have_last) {
        int16_t closer = (int16_t)s_last_dist - (int16_t)dist_cm;  /* +ve = nearer */
        if (closer >= SPIKE_CM) {
            mood_nudge(-4, 48);         /* sudden proximity -> SURPRISED (abrupt) */
        } else if (closer >= APPROACH_CM) {
            mood_nudge(2, 10);          /* approaching -> arousal up -> CURIOUS */
        }
        if (dist_cm <= TOO_CLOSE_CM) {
            mood_nudge(-10, 12);        /* too close -> valence down -> NERVOUS */
        }
    }

    s_last_dist = dist_cm;
    s_have_last = 1;
}

void sensors_pet(uint32_t now_ms) {
    mood_nudge(12, 4);                  /* each pet: pleasant, mildly rousing */
    s_last_activity_ms = now_ms;

    /* Count presses inside a rolling 2 s window. */
    if ((now_ms - s_pet_window_start) > PET_WINDOW_MS) {
        s_pet_window_start = now_ms;
        s_pet_count        = 0;
    }
    s_pet_count++;

    if (s_pet_count >= PET_GESTURE_N) {
        /* Sustained petting: shove valence high so the point sits deep in HAPPY.
         * The linear mood decay (1 unit / 100 ms) then holds it there for ~3 s
         * before it slips back -- that is the "minimum hold" without a timer. */
        mood_nudge(30, 0);
        s_pet_count        = 0;
        s_pet_window_start = now_ms;
    }
}

void sensors_idle_tick(uint32_t now_ms) {
    uint32_t idle = now_ms - s_last_activity_ms;

    /* Nudges must out-pull mood_decay (which adds +1 toward zero each 100 ms),
     * so they are sized > 1 to make real headway once the thresholds are met. */
    if (idle >= INACTIVE_SAD_MS) {
        mood_nudge(-2, 0);              /* boredom -> SAD */
    }
    if (idle >= INACTIVE_SLEEPY_MS) {
        mood_nudge(0, -3);             /* drowsiness -> SLEEPY */
    }
}

#ifdef ENABLE_AUDIO
void sensors_audio_tick(void) {
    /* TODO(tier3): compute a rolling RMS envelope from the ADC1 free-running
     * samples and nudge arousal on a loud transient. Not implemented. */
}
#endif
