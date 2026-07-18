# Changelog

All notable changes to this project are documented here
([Keep a Changelog](https://keepachangelog.com/) style).

## [2026-07-11] - Session: promote MAX7219 driver to extern_libraries

### Changed
- **Drivers promoted out of `src/` into `extern_libraries/`** so the new
  `slot_machine` project can share them instead of duplicating (per CLAUDE.md
  "never duplicate a driver"):
  - `extern_libraries/spi/` — master-SPI init + `spi_transfer()`, factored out
    of the old `max7219.c` (was inlined there).
  - `extern_libraries/max7219/` — **v1 → v2, BREAKING**. Single-chip API
    (`max7219_init/set_digit/clear(cs)`) replaced by a daisy-chain model
    (`max7219_chain_t {cs,count}` + `max7219_chain_*`). `MAX7219_DRIVER_VERSION 2`.
    Consumers: **stopwatch** (`count = 1`), **slot_machine** (`count = 5`).
  - `extern_libraries/font7seg/` — moved verbatim (header guard renamed).
- `src/main.c` — now describes the lone chip as a 1-device chain
  (`s_disp = { MAX7219_CS, 1 }`) and calls `max7219_chain_init/set_digit`.
  Wire-level behavior is identical (count = 1 emits the same single frames).
- `Makefile` — SOURCES/`-I` now pull `spi`, `max7219`, `font7seg` from
  `extern_libraries/`; dropped the unused `USART.c`/`-DBAUD`.

### Files touched
- `src/main.c`, `Makefile`, `docs/CHANGELOG.md`
- removed: `src/max7219.{c,h}`, `src/font7seg.{c,h}`
- added (shared): `extern_libraries/{spi,max7219,font7seg}/*`

### Why
- Two projects now need the MAX7219. Sharing one driver keeps them in lockstep;
  the daisy-chain generalization is required by slot_machine and is a strict
  superset of the stopwatch's single-chip use.

### Note
- Flash grew 2112 → 2418 B (+306) from the added SPI/chain indirection for the
  count = 1 case. SRAM unchanged (17 B .bss). Acceptable (<1 % of 32 KB) for the
  reuse win; a single 8-digit display never stresses the budget.

## [2026-07-11] - Session: convert to a two-button stopwatch

### Added
- `src/hardware_connections.h` — `BTN_STARTPAUSE` (PD6) and `BTN_CLEAR` (PD7),
  active-low with internal pull-ups.
- `diagram.json` — two `wokwi-pushbutton` parts on D6/D7 to GND for simulation.

### Changed
- `src/main.c` — rewritten from the serial scrolling-text demo into a stopwatch:
  - **Timer1 CTC @ 1 ms** (`OCR1A = 1999`, clk/8) as the time base; ISR advances
    a free-running `s_ticks_ms` and (while running) `s_elapsed_ms`. Replaces all
    `_delay_ms` timing, which a stopwatch cannot use.
  - `render_time()` decomposes elapsed ms into `MM.SS.mmm` across DIG0–6 (DIG7
    blank), reusing `char_to_segments('0'+d)` for digits and
    `char_to_segments('.')` OR'd in for the DIG1/DIG3 decimal-point separators.
  - `button_pressed()` — 1 ms-paced, 15 ms software debounce with released→pressed
    edge detection. START/PAUSE toggles running; CLEAR zeroes the count anytime.
  - 32-bit ISR globals read/cleared inside `ATOMIC_BLOCK` to avoid torn access.
  - USART removed (display-only); dropped `USART.h` and `<util/delay.h>` includes.
- Timer/time/debounce literals promoted to named `#define`s (`TIMER1_TOP`,
  `MS_ROLLOVER`, `DEBOUNCE_MS`, …) per the no-magic-numbers rule.
- Docs (`Theory.md`, `hardware_connections.md`, `README.md`) rewritten for the
  stopwatch: Timer1 CTC timekeeping + switch-debounce theory, button wiring
  table, and updated usage/checklist.

### Fixed
- `Makefile`: `LIBDIR` `../extern_libraries` → `../../extern_libraries`. This
  sub-project sits one level deeper (`max7219,max7221/hello_world/`) than the
  path assumed, so `make flash` failed with "No rule to make target
  '../extern_libraries/USART.o'". User surfaced in chat; shared drivers untouched.

### Files touched
- src/main.c, src/hardware_connections.h, Makefile, diagram.json, README.md,
  docs/{Theory,hardware_connections,CHANGELOG}.md

### Why
- Arbitrary text on 7-segment digits is awkward to read; a stopwatch is a more
  natural fit for the hardware and exercises hardware-timer interrupts, atomic
  ISR/main data sharing, and software switch debouncing.

### Verification
- `make` clean under `-Wall -Wextra -Werror`; flash 2112 B (6.4% of 32 KB),
  SRAM 17 B static (4 B `s_ticks_ms` + 4 B `s_elapsed_ms` + 1 B `s_running` +
  8 B framebuffer) + small stack.
- On hardware: display powers up at `00.00.000` with both separators lit;
  START/PAUSE toggles counting, CLEAR resets anytime. (Hardware button check
  pending user confirmation.)

## [2026-07-09] - Session: serial-driven scrolling 7-segment text

### Added
- `src/max7219.{h,c}` — single-chip MAX7219 driver (hardware SPI, no-decode
  mode): init, `set_digit`, `set_intensity`, `clear`. All registers named, no
  magic numbers, datasheet-cited.
- `src/font7seg.{h,c}` — ASCII → 7-segment segment-byte table in PROGMEM
  (`char_to_segments`), covering 0-9, A-Z (best-effort), space, `-`, `.`.
- `src/hardware_connections.h` — `MAX7219_CS` pin (PB2) as the single source of
  truth for pin assignment.
- `src/main.c` — replaces the stale "anvil" stub. Prompt/read/render loop:
  ≤8 chars static (left-aligned), >8 chars scroll right-to-left once then blank.
- `diagram.json` + `wokwi.toml` — Wokwi sim (Uno + `wokwi-max7219-matrix` as a
  protocol stand-in + `wokwi-logic-analyzer` for SPI decode).
- `docs/Theory.md`, `docs/hardware_connections.md`, and README content.

### Changed
- `Makefile`:
  - `LIBDIR` `../../lcd/lcd_driver` → `../extern_libraries` (this project needs
    USART, not the LCD driver).
  - Sources: drop `lcd_driver.c`, add `USART.c` + `pin/pin.c`; add
    `-I$(LIBDIR)/pin` and `-DBAUD=9600`.
  - `TARGET` pinned to `max7219_max7221` (the folder-name comma broke
    `-Wl,-Map,...`).
  - Dropped `-Wpedantic` (kept `-Wall -Wextra -Werror`): the shared
    `extern_libraries/USART.c` uses GCC binary literals that `-Wpedantic`
    rejects. User-approved; shared driver left untouched.

### Fixed
- Line-ending robustness: replaced the shared `readString` (CR-only) with a
  local `read_line()` in `main.c` that terminates on CR **or** LF. Caught in
  Wokwi — the serial monitor sends LF, so the CR-only version hung with a blank
  display. Real terminals (PuTTY=CR) and LF monitors now both work.

### Files touched
- Makefile, src/main.c, src/max7219.{h,c}, src/font7seg.{h,c},
  src/hardware_connections.h, diagram.json, wokwi.toml, README.md,
  docs/{Theory,hardware_connections,CHANGELOG}.md

### Why
- First MAX7219 "hello world": learn the chip by driving all 8 digits (its
  maximum) with typed serial text. No-decode mode chosen so arbitrary letters —
  not just Code-B numerals — can be shown.

### Verification
- `make` clean; flash 1744 B (5.3% of 32 KB), SRAM 8 B static + ~128 B stack.
- Wokwi: SPI bus decoded from logic-analyzer VCD. Init sequence, "HELLO" static
  render (H=0x37 E=0x4F L=0x0E L=0x0E O=0x7E), and 19-window scroll of
  "ABCDEFGHI" all confirmed byte-for-byte. See
  `wokwi-verify-artifacts/20260709T135742Z-serial-scroll/report.md`.
