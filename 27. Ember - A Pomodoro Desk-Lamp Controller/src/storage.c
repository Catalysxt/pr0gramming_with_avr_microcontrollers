/*
 * storage.c
 *
 * Purpose : Persist Pomodoro presets + session counter in internal EEPROM.
 * Hardware: ATmega328P internal EEPROM (1 KB).
 * Datasheet: ATmega328P §7 (EEPROM); avr-libc <avr/eeprom.h>.
 *
 * Layout (EEMEM lets the linker place these; addresses are symbolic):
 *   ee_magic         uint16_t  first-boot sentinel (EE_MAGIC_VALUE)
 *   ee_work_min      uint8_t   last-used work minutes
 *   ee_break_min     uint8_t   last-used break minutes
 *   ee_session_count uint16_t  lifetime completed work sessions
 *
 * The magic word distinguishes a freshly-erased EEPROM (0xFF...) from
 * legitimately stored data, so first boot is unambiguous.
 */

#include "storage.h"

#include <avr/eeprom.h>   /* eeprom_read/update_byte/word */

static uint16_t EEMEM ee_magic;
static uint8_t  EEMEM ee_work_min;
static uint8_t  EEMEM ee_break_min;
static uint16_t EEMEM ee_session_count;

#define EE_MAGIC_VALUE   0x454Du   /* 'E','M' — Ember */

void storage_init(void)
{
    if (eeprom_read_word(&ee_magic) != EE_MAGIC_VALUE) {
        /* First boot / erased chip: stamp magic + factory defaults.
         * eeprom_update_* only writes cells that actually differ. */
        eeprom_update_byte(&ee_work_min,  WORK_DEFAULT);
        eeprom_update_byte(&ee_break_min, BREAK_DEFAULT);
        eeprom_update_word(&ee_session_count, 0u);
        eeprom_update_word(&ee_magic, EE_MAGIC_VALUE);   /* write magic last */
    }
}

void storage_load_presets(uint8_t *work_min, uint8_t *break_min)
{
    *work_min  = eeprom_read_byte(&ee_work_min);
    *break_min = eeprom_read_byte(&ee_break_min);
}

void storage_save_presets(uint8_t work_min, uint8_t break_min)
{
    eeprom_update_byte(&ee_work_min,  work_min);
    eeprom_update_byte(&ee_break_min, break_min);
}

uint16_t storage_get_session_count(void)
{
    return eeprom_read_word(&ee_session_count);
}

uint16_t storage_increment_session(void)
{
    uint16_t n = (uint16_t)(eeprom_read_word(&ee_session_count) + 1u);
    eeprom_update_word(&ee_session_count, n);
    return n;
}
