# Changelog

## [2026-07-02] - Session: initial ANVIL build-out

### Added

- `src/hardware.h` — every non-LCD pin macro (buzzer, LEDs, keypad rows/columns)
- `src/sys_tick.h/.c` — Timer0 CTC 1 ms tick, `sys_millis()`, `sys_elapsed()`
- `src/buzzer.h/.c` — Timer2 CTC/OC2A non-blocking tone + PROGMEM melody player, 5-entry SFX library
- `src/keypad.h/.c` — split-port 4×4 scan, 3-scan debounce, press-vs-long-press event model
- `src/leds.h/.c` — green/red on-off, 1 Hz red blink scheduler, one-shot pulses
- `src/storage.h/.c` — EEPROM magic-byte first-boot init, constant-time PIN compare, wear-aware writes
- `src/ui.h/.c` — CGRAM lock glyph, all 8 screen layouts, padded-row redraw helper
- `src/anvil.h/.c` — 11-state top-level FSM (splash/entry/unlocked/wrong/lockout/change-PIN sub-flow/screensaver)
- `src/main.c` — init sequence + non-blocking superloop
- `docs/hardware_connections.md`, `docs/Theory.md` — project documentation
- Top-level `README.md`

### Fixed

- `Makefile`: `LIBDIR` pointed at `../../extern_libraries` (wrong — `lcd_driver` actually
  lives at `../../lcd/lcd_driver`, not under `extern_libraries`); `SOURCES` referenced
  `i2c.c`/`24lc256.c`/`ds3231.c`/`USART.c`, none of which exist or are used by this
  project (leftover from a different, copy-pasted Makefile) and never compiled
  `lcd_driver.c` at all; removed an unrelated "host-side dump + plot" Python toolchain
  section; fixed a stale `squeaky_clean` path.
- `anvil.c`: `g.attempts_left` would have been zero-initialised by BSS instead of
  starting at 3, so the very first wrong PIN entered after boot would have looked like
  the 3rd strike and jumped straight to lockout. Fixed with an explicit static
  initializer (deliberately *not* a reset-on-every-splash-entry, since that would let a
  wrong current-PIN guess during the change-PIN flow silently refill the attempt
  counter and bypass the 3-strike lockout).
- `anvil.c`: a PIN submit that transitions out of the entry-family states mid-tick
  (e.g. `ST_ENTRY` -> `ST_UNLOCKED`) could still trigger a stale reveal-expiry redraw
  against the old entry screen, clobbering the freshly-drawn destination screen. Gated
  the reveal-expiry redraw on still being in an entry-family state.

### Files touched

- `Makefile`
- `src/hardware.h`, `src/sys_tick.h`, `src/sys_tick.c`, `src/buzzer.h`, `src/buzzer.c`,
  `src/keypad.h`, `src/keypad.c`, `src/leds.h`, `src/leds.c`, `src/storage.h`,
  `src/storage.c`, `src/ui.h`, `src/ui.c`, `src/anvil.h`, `src/anvil.c`, `src/main.c`
- `docs/hardware_connections.md`, `docs/Theory.md`
- `README.md`

### Why

- `docs/prompt.md` specified the full ANVIL firmware from scratch; the project
  directory only had a broken, copy-pasted `Makefile` and an empty `src/main.c` before
  this session.

### Spec corrections / gaps filled

- `docs/prompt.md`'s relative path to `lcd_driver.h` (`../lcd_driver/lcd_driver.h`)
  assumed it lived under `extern_libraries/`; the real path from `anvil/` is
  `../../lcd/lcd_driver/lcd_driver.h`. Left `lcd_driver` where it is (relocating it
  would touch every other project that references it) and fixed anvil's own path.
- Followed `prompt.md`'s flat `hardware.h` (`#define` macros) pin convention rather
  than the generic `docs/CLAUDE.md` template's `pin_t`/`hardware_connections.h`
  pattern, since the latter is unused anywhere else in the workspace and would
  conflict with `lcd_driver`'s own macro-based style.
- `leds_pulse_red()` was added alongside the spec-named `leds_pulse_green()` — needed
  for the wrong-PIN "red LED 500 ms flash" one-shot, which isn't a repeating blink.
- Change-PIN result screens ("PIN CHANGED"/"MISMATCH") display for a fixed 2000 ms —
  the spec calls for a "brief" display without giving an exact duration.
- Screensaver wake target: only `ST_ENTRY` resumes in place; every other state
  (including the change-PIN sub-flow) wakes back to `ST_SPLASH`, since resuming a
  partially-typed current/new PIN after an idle timeout is a security-sensitive edge
  case best avoided by aborting the flow.
- `ST_LOCKOUT` is exempted from the idle-screensaver transition — its own 30 s
  countdown is the same order of magnitude as the 30 s idle threshold, and switching
  away to the screensaver mid-countdown would freeze the lockout's own tick logic.
