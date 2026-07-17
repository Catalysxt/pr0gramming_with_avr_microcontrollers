/*
 * high_score.c
 *
 * Purpose : Persist and retrieve the high score across power cycles.
 * Hardware : ATmega328P internal EEPROM (1024 bytes).
 * Datasheet: ATmega328P §7 (EEPROM), §7.2 (read/write timing)
 *            avr-libc §<avr/eeprom.h> — eeprom_update_word/byte,
 *            eeprom_read_word/byte
 *
 * Layout (EEPROM addresses):
 *   0x00-0x01 : uint16_t high score (little-endian)
 *   0x02      : uint8_t  magic byte (0xA5 when initialised)
 *
 * The magic byte distinguishes a freshly-erased EEPROM (0xFF) from a valid
 * stored score of 0. Without it, a score of 0 and an erased cell are
 * indistinguishable.
 */

#include "high_score.h"

#include <avr/eeprom.h>     /* eeprom_read_word, eeprom_update_word,
                             * eeprom_read_byte, eeprom_update_byte  */
#include <stdint.h>

/* EEPROM variable declarations — the EEMEM attribute places them at
 * a fixed address in the .eeprom section (avr-libc §<avr/eeprom.h>). */
static uint16_t EEMEM ee_high_score = 0u;
static uint8_t  EEMEM ee_magic      = 0u;

#define MAGIC_VALUE  0xA5u

uint16_t high_score_load(void)
{
    uint8_t magic = eeprom_read_byte(&ee_magic);

    if (magic != MAGIC_VALUE) {
        /* First power-up: stamp magic byte and store an initial score of 0.
         * eeprom_update_* only writes if the value differs from what is
         * already stored — protects against unnecessary erase cycles. */
        eeprom_update_byte(&ee_magic,      MAGIC_VALUE);
        eeprom_update_word(&ee_high_score, 0u);
        return 0u;
    }

    return eeprom_read_word(&ee_high_score);
}

void high_score_save(uint16_t score)
{
    uint16_t stored = eeprom_read_word(&ee_high_score);

    if (score > stored) {
        /* eeprom_update_word skips the erase-write cycle when the new value
         * equals what is already in the cell.  (avr-libc §<avr/eeprom.h>) */
        eeprom_update_word(&ee_high_score, score);
    }
}
