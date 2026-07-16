// 24LC256 I2C EEPROM Driver
// Microchip 24LC256 — 256 kbit (32 KB) EEPROM, I2C interface, 400 kHz max
// This driver uses the TWI peripheral via i2c.h

#ifndef EEPROM_24LC256_H_
#define EEPROM_24LC256_H_

#include <avr/io.h>
#include "i2c.h"

// Control byte format: 0b 1010 A2 A1 A0 R/W
// A2=A1=A0 wired to GND → device select bits = 000
// 24LC256 datasheet p. 5
#define EEPROM_I2C_WRITE_ADDR   0xA0    // R/W bit = 0 (write)
#define EEPROM_I2C_READ_ADDR    0xA1    // R/W bit = 1 (read)

// Maximum internal write cycle time — used after every write operation
// 24LC256 datasheet p. 3, AC characteristics, parameter Twc
#define EEPROM_WRITE_CYCLE_MS   5

// Page size for page-write operations (address bits A5..A0 are internal page counter)
// 24LC256 datasheet p. 7
#define EEPROM_BYTES_PER_PAGE   64

// Last valid address (32 KB = 0x0000..0x7FFF)
#define EEPROM_BYTES_MAX        0x7FFF

// Read a single byte from the given address
uint8_t EEPROM_read_byte(uint16_t address);

// Read two consecutive bytes starting at address (returns MSB in high byte)
uint16_t EEPROM_read_word(uint16_t address);

// Write a single byte to the given address
void EEPROM_write_byte(uint16_t address, uint8_t byte);

// Write two bytes to the given address (both bytes must fall within one 64-byte page)
void EEPROM_write_word(uint16_t address, uint16_t word);

// Zero out every byte in the EEPROM using 64-byte page writes
void EEPROM_clear_all(void);

#endif /* EEPROM_24LC256_H_ */
