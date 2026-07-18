/*
 * storage.h
 *
 * EEPROM-persisted Pomodoro presets and lifetime session counter:
 *   magic word + work_min + break_min + session_count.
 * First-boot detection via the magic word; wear-aware writes throughout
 * (eeprom_update_*, which skips a cell whose value is unchanged).
 *
 * ATmega328P datasheet §7 (EEPROM); avr-libc <avr/eeprom.h>
 */

#ifndef EMBER_STORAGE_H_
#define EMBER_STORAGE_H_

#include <stdint.h>

/* Preset bounds and factory defaults (also enforced by the FSM). */
#define WORK_MIN_MIN     1u
#define WORK_MIN_MAX     99u
#define BREAK_MIN_MIN    1u
#define BREAK_MIN_MAX    60u
#define WORK_DEFAULT     25u
#define BREAK_DEFAULT    5u

/* Stamp the magic word + defaults if the EEPROM has never been initialised
 * (magic mismatch, e.g. a freshly-erased chip). Call once during init. */
void storage_init(void);

/* Load the last-used presets into *work_min / *break_min. */
void storage_load_presets(uint8_t *work_min, uint8_t *break_min);

/* Persist presets. Wear-aware: unchanged bytes are not rewritten. */
void storage_save_presets(uint8_t work_min, uint8_t break_min);

/* Lifetime count of completed work sessions. */
uint16_t storage_get_session_count(void);

/* Increment and persist the completed-session counter; returns the new value. */
uint16_t storage_increment_session(void);

#endif /* EMBER_STORAGE_H_ */
