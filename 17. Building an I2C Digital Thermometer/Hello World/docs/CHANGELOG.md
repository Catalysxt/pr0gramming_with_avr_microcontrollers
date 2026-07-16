## [2026-07-01] - Session: i2c thermometer bring-up

### Fixed
- Makefile: replaced `lcd_driver/lcd_driver.c` in SOURCES with `USART.c` and `i2c.c` (lcd driver not used; USART and i2c were never compiled, causing linker errors)
- Makefile: replaced `-I$(LIBDIR)/lcd_driver` with `-I$(LIBDIR)` so compiler can find `USART.h`, `pinDefines.h`, `i2c.h`
- Makefile: `squeaky_clean` now removes `$(LIBDIR)/*.o`
- main.c: added missing `#include "i2c.h"`
- main.c: corrected `init_i2c()` → `init_I2C()` (case mismatch with driver declaration)
- main.c: corrected clock comment — `clock_div_1` with LFUSE=0xF7 (external crystal) runs at 16 MHz, not 8 MHz
- main.c: replaced `printByte()` (zero-padded 3-digit) with `print_uint8()` static helper — output now shows `25.5 degrees` instead of `025.5`
- main.c / USART.c: converted `0b...` binary literals to hex — `-Wpedantic -Werror` rejects binary literals under `std=gnu99`

### Added
- main.c: `print_uint8()` static helper — prints uint8_t as decimal without leading zeros
- main.c: appended " degrees" to temperature output strings

### Files touched
- src/main.c, Makefile, extern_libraries/USART.c

### Why
- Project failed to build due to missing sources, wrong include path, missing header, and function name mismatch. Binary literal extension tripped `-Wpedantic`. Display output was zero-padded by `printByte()`.
