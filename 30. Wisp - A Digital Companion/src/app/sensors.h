#ifndef DIGITAL_COMPANION_SENSORS_H_
#define DIGITAL_COMPANION_SENSORS_H_

#include <stdint.h>

/* Sensor interpretation: turns raw driver readings into mood nudges.
 *
 * This is the only place that knows what a distance trend or a burst of pet
 * presses MEANS. It calls mood_nudge() and leaves the (v, a) -> expression
 * mapping to mood.c / fsm.c. Kept free of driver headers (it takes plain values)
 * so the intent logic stays testable and the drivers stay swappable.
 *
 * Tiers:
 *   Tier 2 (interaction): HC-SR04 proximity trend + pet button (implemented).
 *   Tier 1 was the LDR ambient sensor -- removed by design; SLEEPY/SAD now come
 *          from the inactivity timer here instead.
 *   Tier 3 (audio): sensors_audio_tick() stub behind ENABLE_AUDIO. */

/* Any distance at/above this (cm) means "nothing in range" (incl. the driver's
 * out-of-range sentinel). */
#define SENSORS_RANGE_CM 150u

/* Reset state; `now_ms` seeds the inactivity clock. */
void sensors_init(uint32_t now_ms);

/* Feed one HC-SR04 reading (cm). Detects approach -> CURIOUS, a sudden jump
 * closer -> SURPRISED, and too-close -> NERVOUS. Call at the ranging cadence. */
void sensors_proximity_update(uint16_t dist_cm, uint32_t now_ms);

/* Register one pet-button press. Bumps valence; >= 3 presses within 2 s forces
 * a strong HAPPY that the mood decay holds for a couple of seconds. */
void sensors_pet(uint32_t now_ms);

/* Apply inactivity drift. Call every 100 ms (alongside mood_decay): after 30 s
 * with no object/pet, valence sags toward SAD; after 60 s, arousal sags toward
 * SLEEPY. */
void sensors_idle_tick(uint32_t now_ms);

#ifdef ENABLE_AUDIO
/* TODO(tier3): rolling-RMS mic envelope on ADC1 -> arousal spike on a loud
 * transient. DSP not implemented; this is the seam. */
void sensors_audio_tick(void);
#endif

#endif /* DIGITAL_COMPANION_SENSORS_H_ */
