/*
 * high_score.h
 *
 * EEPROM-backed high score with a magic byte to detect uninitialised EEPROM.
 * Uses eeprom_update_* to avoid unnecessary write cycles.
 *
 * ATmega328P datasheet §7 (EEPROM), avr-libc <avr/eeprom.h>
 */

#ifndef DIGITAL_DASH_HIGH_SCORE_H_
#define DIGITAL_DASH_HIGH_SCORE_H_

#include <stdint.h>

/*
 * Read the stored high score.
 * Returns 0 (and writes the initial magic byte) if EEPROM is uninitialised.
 */
uint16_t high_score_load(void);

/*
 * Persist score to EEPROM only if it exceeds the current stored value.
 * Uses eeprom_update_word to skip the write when the value is unchanged,
 * protecting against unnecessary wear.
 */
void high_score_save(uint16_t score);

#endif /* DIGITAL_DASH_HIGH_SCORE_H_ */
