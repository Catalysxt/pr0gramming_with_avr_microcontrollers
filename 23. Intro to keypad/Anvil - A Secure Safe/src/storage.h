/*
 * storage.h
 *
 * EEPROM-persisted PIN storage: 2-byte magic + 4 ASCII-digit PIN.
 * First-boot detection via the magic word, wear-aware writes throughout.
 */

#ifndef ANVIL_STORAGE_H_
#define ANVIL_STORAGE_H_

#include <stdint.h>
#include <stdbool.h>

/* Stamps the magic word and default PIN "1234" if the EEPROM has never
 * been initialised (magic mismatch — e.g. a freshly-erased chip). Call
 * once during init, before anything else touches storage. */
void storage_init(void);

/* Constant-time comparison of entered[4] against the stored PIN: reads
 * and XOR-accumulates all 4 bytes unconditionally (never short-circuits
 * on the first mismatch), to avoid leaking match-length via timing. */
bool storage_check_pin(const char entered[4]);

/* Writes new_pin[4] to EEPROM. Uses eeprom_update_byte per digit, which
 * already skips the write if the byte is unchanged. */
void storage_set_pin(const char new_pin[4]);

#endif /* ANVIL_STORAGE_H_ */
