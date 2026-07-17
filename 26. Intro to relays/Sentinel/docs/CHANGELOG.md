# Changelog — Sentinel (relay/sentinel)

All notable changes to this project. Format follows [Keep a Changelog](https://keepachangelog.com/).

## [2026-07-07] - Session: relay drive-chain bring-up

### Fixed
- **Relay would not actuate.** Root cause: the load is a self-contained,
  **active-HIGH relay module** (VCC/GND/IN), but it was being driven through a
  **ULN2803**. The ULN2803 is a low-side *sink* (output only pulls LOW), so it
  could never drive an active-HIGH input HIGH — the module stayed off while the
  ULN measured "correct" (IN 5 V, OUT1 0.6 V). **Fix: removed the ULN2803; PB0
  now drives the module's IN pin directly.** No firmware change — the FSM already
  drives PB0 HIGH for "on" (`relay_drive(true) → pin_high`), matching active-HIGH.
- Earlier in the same bring-up: a **missing ground on the button** left PD2
  floating/noisy, which the debounce read as a >= 2 s hold and auto-entered
  LOCKOUT (`AUTO` then `LOCKOUT` at every power-up). Reconnecting the button's GND
  leg resolved it. Firmware/debounce logic was correct throughout.

### Changed (docs)
- Rewrote `docs/hardware_connections.md` and `docs/Theory.md` §1 for the real
  build: relay **module** driven directly from PB0, no ULN2803; explained the
  bare-coil-vs-module driver distinction and trigger-polarity matching.
- Corrected a ULN2803 pinout error in the old doc: a real **ULN2803A is
  pin 9 = GND, pin 10 = COM** (the doc had them swapped). Noted only as the
  bare-coil fallback now that the ULN2803 is out of this build.
- Updated `README.md` (Overview + Hardware), and the ULN2803 references in
  `src/main.c` and `src/hardware_connections.h` comments.

### Files touched
- docs/hardware_connections.md, docs/Theory.md, docs/CHANGELOG.md, README.md,
  src/main.c, src/hardware_connections.h

### Why
- Documentation described a bare-coil + ULN2803 drive chain (inherited from
  `relay/hello_world`), but the project was actually wired to an active-HIGH relay
  module. The mismatch is exactly what made the relay silent; docs now match the
  hardware so the drive polarity is unambiguous next time.

## [2026-07-06] - Session: relay FSM extension

### Added
- Four-state relay FSM (`src/relay_fsm.{c,h}`): AUTO / MANUAL_ON / MANUAL_OFF /
  LOCKOUT, with an explicit `enter()`/`tick()` structure.
- `src/sys_tick.{c,h}` — 1 ms Timer0 CTC timebase + `sys_millis()` / `sys_elapsed()`
  non-blocking scheduler (logic copied from `keypad/anvil/src/sys_tick.c`; header
  guard and comments renamed for this project).
- `src/button.{c,h}` — single active-low button with 5 ms-sampling debounce and
  2 s long-press detection (adapted from `keypad/anvil/src/keypad.c`).
- `src/hardware_connections.h` — `static const pin_t` map: RELAY on PB0, BUTTON on PD2.
- EEPROM persistence of the operational state via `eeprom_update_byte`; LOCKOUT is
  deliberately never persisted so an MCU reset always escapes it.
- Docs: `Theory.md`, `hardware_connections.md`, and a filled-in `README.md`.

### Changed
- `src/main.c` — replaced the stale `keypad/anvil` stub (empty `main()`, irrelevant
  includes) with the FSM superloop.
- `Makefile`:
  - `LIBDIR` `../../lcd/lcd_driver` → `../../extern_libraries` (this project uses
    the shared USART + pin drivers, not the LCD driver).
  - `SOURCES`: dropped `$(LIBDIR)/lcd_driver.c`; added `$(LIBDIR)/USART.c` and
    `$(LIBDIR)/pin/pin.c`. `CPPFLAGS` gained `-I$(LIBDIR)/pin`.
  - Dropped `-Wpedantic` from `CFLAGS` (kept `-Wall -Wextra -Werror`). Reason: the
    shared `extern_libraries/USART.c` uses GCC binary literals (`0b...`) that
    `-Wpedantic` rejects under `-std=gnu99`; the shared driver was left untouched
    per extern_libraries discipline.
  - `squeaky_clean` also removes `$(LIBDIR)/pin/*.o`.
- Removed stale `build/anvil.*` artifacts (project now builds `sentinel.*`).

### Files touched
- src/main.c, src/relay_fsm.{c,h}, src/button.{c,h}, src/sys_tick.{c,h},
  src/hardware_connections.h, Makefile, docs/*, README.md

### Why
- Extension goal (docs/prompt.md): evolve the base blocking blink into a proper
  FSM with non-blocking timing and a debounced control button, reporting mode over
  UART, with EEPROM persistence as the chosen stretch goal.

### Changed (follow-up)
- LOCKOUT is now **toggled** by a long hold (>= 2 s) instead of reset-only exit:
  a hold from an operational state locks out; a hold while locked out resumes the
  last operational state (reloaded from EEPROM via `load_operational_state()`).
  Short presses remain ignored during LOCKOUT. `relay_fsm_tick()` reworked;
  `relay_fsm_init()` refactored to share the new `load_operational_state()` helper.

- Named the project **Sentinel** and renamed the folder `relay/extension_1` ->
  `relay/sentinel`. Build output is now `build/sentinel.*` (TARGET derives from the
  folder name). Renamed header guards `RELAY_EXT1_*` -> `SENTINEL_*`; updated all
  titles, path comments, and doc headers.

### Memory footprint
- `.text` 1792 B (5.5% of 32 KB flash); `.data + .bss` 38 B (1.9% of 2 KB SRAM).
