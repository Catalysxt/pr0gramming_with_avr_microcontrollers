/*
 * storage.c
 *
 * Purpose : Persist the 4-digit unlock PIN in internal EEPROM.
 * Hardware: ATmega328P internal EEPROM (1 KB).
 * Datasheet: ATmega328P §7 (EEPROM), §7.2 (read/write timing)
 *            avr-libc <avr/eeprom.h> — eeprom_update_word/byte,
 *            eeprom_read_word/byte
 *
 * Layout (EEPROM addresses), per docs/prompt.md:
 *   0x00-0x01 : uint16_t magic word (0x4CA1, i.e. bytes 0xA1,0x4C
 *               little-endian — spelled "0xANVL" in the spec)
 *   0x02-0x05 : uint8_t  pin[4] — ASCII digits '0'-'9'
 *
 * The magic word distinguishes a freshly-erased EEPROM (0xFF 0xFF) from a
 * legitimately stored PIN — without it, first-boot detection would be
 * impossible to distinguish from an all-zero PIN.
 */

#include "storage.h"

#include <avr/eeprom.h>   /* eeprom_read_word, eeprom_update_word,
                           * eeprom_read_byte, eeprom_update_byte */
#include <stdint.h>

static uint16_t EEMEM ee_magic;
static uint8_t  EEMEM ee_pin[4];

#define EE_MAGIC_VALUE   0x4CA1u
#define PIN_LEN          4u

static const char k_default_pin[PIN_LEN] = { '1', '2', '3', '4' };

void storage_init(void)
{
    if (eeprom_read_word(&ee_magic) != EE_MAGIC_VALUE) {
        /* First boot (or freshly-erased EEPROM): stamp the magic word and
         * the factory-default PIN. eeprom_update_* only writes cells that
         * actually differ, protecting flash/EEPROM endurance. */
        eeprom_update_word(&ee_magic, EE_MAGIC_VALUE);
        for (uint8_t i = 0; i < PIN_LEN; i++) {
            eeprom_update_byte(&ee_pin[i], (uint8_t)k_default_pin[i]);
        }
    }
}

bool storage_check_pin(const char entered[4])
{
    uint8_t diff = 0u;

    /* XOR-accumulate every byte unconditionally — never return early on
     * the first mismatch, so the comparison takes the same time whether
     * the first digit or the last digit differs (constant-time compare). */
    for (uint8_t i = 0; i < PIN_LEN; i++) {
        diff |= (uint8_t)((uint8_t)entered[i] ^ eeprom_read_byte(&ee_pin[i]));
    }

    return diff == 0u;
}

void storage_set_pin(const char new_pin[4])
{
    for (uint8_t i = 0; i < PIN_LEN; i++) {
        /* eeprom_update_byte skips the write if the byte is unchanged. */
        eeprom_update_byte(&ee_pin[i], (uint8_t)new_pin[i]);
    }
}
