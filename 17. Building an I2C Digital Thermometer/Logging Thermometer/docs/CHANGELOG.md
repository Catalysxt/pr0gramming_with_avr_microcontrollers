## [2026-07-01] - Session: logging_thermometer bring-up

### Fixed
- main.c: replaced `#include "25LC256.h"` (SPI, non-existent) with `#include "24lc256.h"` (I2C driver in extern_libraries)
- main.c: removed `init_SPI()` call — 24LC256 shares the I2C bus, no separate SPI init needed
- main.c: corrected `init_i2c()` -> `init_I2C()` (case mismatch with driver declaration)
- main.c: fixed syntax error `? = 1 : 0` -> `? 1 : 0` in `enter_menu` ternary
- main.c: converted all `0b...` binary literals to hex — `-Wpedantic -Werror` rejects them under `std=gnu99`
- main.c: replaced SPI-based `case 'p'` (SLAVE_SELECT, SPI_trade_byte, SPDR) with `EEPROM_read_byte(i)` from 24lc256.h
- main.c: removed unused `#include <avr/interrupt.h>`
- main.c: corrected clock comment (16 MHz external crystal, not 8 MHz)

### Added
- main.c: `print_uint8()` static helper — decimal output without leading zeros (replaces printByte)
- main.c: `print_uint16()` static helper — decimal output without leading zeros (replaces printWord)
- Makefile: added `$(LIBDIR)/24lc256.c` to SOURCES
- docs/CHANGELOG.md, docs/Theory.md, docs/hardware_connections.md, README.md

### Changed
- main.c: all `printByte()` / `printWord()` call sites replaced with `print_uint8()` / `print_uint16()`
- main.c: `print_temperature()` now calls `print_uint8()` and appends " degrees"

### Files touched
- src/main.c, Makefile

### Why
- Project referenced the wrong EEPROM chip (SPI 25LC256 vs I2C 24LC256); all SPI API calls were undefined.
  Multiple other build blockers: syntax error, binary literals, missing include, case mismatch.
  Zero-padded output (printByte/printWord) replaced for readability.
