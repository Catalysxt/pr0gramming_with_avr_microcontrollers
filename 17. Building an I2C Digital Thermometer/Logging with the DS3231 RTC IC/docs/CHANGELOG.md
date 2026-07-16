## [2026-07-02] - Session: end-to-end verification, project complete

### Fixed
- `docs/overnight_log.csv`: DS3231 clock had been set with year `27` instead of `26` (data-entry slip during `[t]`, not a firmware bug — `ds3231.c`'s BCD<->binary conversion round-trips correctly). All 55 captured rows corrected from `2027-07-02` to `2026-07-02`; relative timing/ordering was unaffected, only the absolute year label.
- `scripts/plot_log.py`: chart title changed from "Temp vs. Real Wall-Clock Time" to "Temperature over time".

### Verified
- Full toolchain exercised end-to-end on real hardware at 9600 baud: `make flash` -> `[t]` set clock -> `[e]` erase -> `[s]` start logging -> 55 readings collected -> `make dump SERIAL_PORT=COM9` -> `make plot`.
- `testing/wrong_x_axis.png` — plot captured before the year fix (shows the 2027 mislabel). `testing/fixed_x_axis.png` — same data after correcting the CSV, confirming the chart, not just the numbers, reflects the fix.
- `make size` after final rebuild: 3260 B flash (9.9%), 0 B SRAM (0%) — unchanged from prior session.

### Files touched
- docs/overnight_log.csv, scripts/plot_log.py, testing/wrong_x_axis.png, testing/fixed_x_axis.png, docs/CHANGELOG.md

### Why
- The `[m]`-menu timing issue (see prior entry's troubleshooting) turned out to be user-side (reset-then-alt-tab-then-Enter routinely exceeds the 5 s window), not a bug — confirmed by driving the board directly over `python -m serial.tools.miniterm` and successfully entering the menu. That surfaced the actual remaining issue: a wrong clock-set year, now corrected in the captured data with guidance given to reset the DS3231 to the correct year going forward.

## Project status: Complete

DS3231 bring-up, timestamped EEPROM logging, and the host-side dump/plot toolchain are all verified working on hardware end-to-end. Remaining ideas are tracked under Stretch Goals in `README.md`.

## [2026-07-02] - Session: fix baud rate mismatch blocking `make dump`

### Fixed
- `Makefile` never defined `BAUD`, so `extern_libraries/USART.h`'s fallback default (9600) was silently in effect on the firmware, while `scripts/dump_log.py`, `src/main.c`'s comment, and all docs claimed 19200. `make dump` therefore always read garbage/no data at the wrong baud rate.
- Standardized on 9600 (the rate actually running on hardware) everywhere: `scripts/dump_log.py` (`BAUD`), `src/main.c` comment, `README.md`, `docs/hardware_connections.md`, `docs/practical_checklist.md`.

### Makefile
- Added `-DBAUD=9600UL` to `CPPFLAGS` in `logging_with_ds3231/Makefile` so the baud rate is explicit instead of relying on the driver's implicit default (flagged here per project rule 9).

### Files touched
- Makefile, src/main.c, scripts/dump_log.py, README.md, docs/hardware_connections.md, docs/practical_checklist.md, docs/CHANGELOG.md

### Why
- `make dump` was failing with "No log lines captured" because the host script and firmware were on different baud rates (19200 vs. actual 9600). The mismatch was invisible in code review since it depended on a driver's implicit fallback rather than an explicit setting.

## [2026-07-02] - Session: DS3231 bring-up, timestamped logging

### Added
- extern_libraries/ds3231.h, ds3231.c: new shared I2C driver for the DS3231 RTC (address 0x68). Exposes `rtc_time_t` and `ds3231_read_time()`/`ds3231_set_time()`; BCD<->binary conversion is internal to the driver.
- src/main.c: `log_record_t` (7-byte record: year/month/date/hour/minute/second + packed temp), `eeprom_write_record()`/`eeprom_read_record()`, `print_timestamp()`, `print_uint8_padded()`, `print_log_record()`.
- src/main.c: `[t]` menu command + `set_clock()` — one-time interactive DS3231 set via serial (`getNumber()` per field: year/month/date/hour/minute/second).
- docs/CHANGELOG.md, docs/Theory.md, docs/hardware_connections.md, README.md.

### Changed
- src/main.c: EEPROM record format is now 7 bytes/reading (timestamp + temp) instead of 1 byte/reading. Capacity drops from 32,764 to ~4,680 readings (~6.5 days at the default 120 s interval) in exchange for interpretable timestamps.
- src/main.c: `[p]` dump and the live per-reading print now emit `YYYY-MM-DD HH:MM:SS, XX.X degrees` per line (kept the "degrees" wording rather than switching to bare CSV, so the serial output stays human-readable at the terminal; a host-side script can still regex the numeric temp out for CSV/plotting).
- src/main.c: readings-in-EEPROM count in the menu now divides by `RECORD_SIZE` instead of counting bytes directly.
- src/main.c: logging-loop bound check rewritten as "does a full record still fit" (`current_memory_location + RECORD_SIZE - 1 <= EEPROM_BYTES_MAX`) instead of the single-byte check from `logging_thermometer`.

### Makefile
- Added `$(LIBDIR)/ds3231.c` to `SOURCES` in `logging_with_ds3231/Makefile` (flagged here per project rule 9 — no other Makefile changes made).

### Files touched
- src/main.c, Makefile, extern_libraries/ds3231.h, extern_libraries/ds3231.c, extern_libraries/README.md, docs/*, README.md

### Why
- Raw temperature dumps (see docs/logging_overnight.md) are uninterpretable. Adding DS3231 allows each reading to be timestamped, allowing greater capability ot interpret the readings.
