# Changelog

All notable changes to this project are documented here
([Keep a Changelog](https://keepachangelog.com/) style).

## [2026-07-11] - Session: initial slot-machine firmware

### Added
- **Full slot-machine firmware** (ATmega328P @ 16 MHz, bare-metal), per
  `docs/prompt.md`. Explicit FSM (ATTRACT/READY/SPINNING/EVALUATE/JACKPOT/
  SETTINGS) with a `state_transition()` seam.
- `src/app/game.c` — reel logic with easing (fast→slow→snap, staggered stops),
  weighted-random symbols, one-place payout table, last-win count-up, jackpot
  flash, and a settings menu (play count [RAM only], accelerating cash-out that
  can't be exited mid-drain, exit).
- `src/app/prng.c` — xorshift32 seeded from a floating-ADC read XORed with
  `TCNT1` captured at the first spin.
- `src/drivers/` — `timing` (Timer0 1 ms tick), `buttons` (15 ms debounce +
  1 s long-press), `buzzer` (Timer1 CTC-toggle tone/melody engine on OC1A),
  `seg7` (two 4-digit numbers + text), `dotmatrix` (4-panel buffer, symbol blit,
  vertical scroll, invert).
- `src/assets/symbols.h` (8× hand-drawn 8×8 icons, PROGMEM, preview comments) and
  `melodies.h` (button/click/thunk/small-win/arpeggio/jackpot/coin/insufficient/
  attract SFX).
- `src/hardware_connections.h`, `src/main.c` (pin map + non-blocking loop +
  unified 5-device chain refresh), `Makefile` (all/flash/clean/size, prints
  `avr-size`), `diagram.json` + `wokwi.toml`.
- Docs: `README.md`, `docs/Theory.md`, `docs/hardware_connections.md` (pin table,
  daisy-chain diagram, connection tables, and wiring diff — the prompt's HARDWARE.md
  content consolidated into this one file), `docs/CHANGELOG.md`.

### Changed
- Depends on the **daisy-chain-capable MAX7219 driver v2** newly promoted to
  `extern_libraries/` (with `spi` + `font7seg`); see the stopwatch CHANGELOG for
  that migration. This project uses `count = 5` (1 score chip + 4 matrix panels).

### Timers
- Timer0 = 1 ms tick, Timer1 = buzzer tone (OC1A/PB1), Timer2 unused. (The
  stopwatch used Timer1 for its tick; moving the tick to Timer0 frees Timer1 for
  hardware tone generation.)

### Verification
- Clean build with `-Wall -Wextra -Werror`. `avr-size`: **.text 7412, .data 82,
  .bss 182** → flash 7494 B (23% of 32 KB), SRAM 264 B (13% of 2 KB).
- Wokwi: boots and runs stably; SPI chain verified electrically at idle (CS
  high, CLK low, buzzer silent). Scripted short-press gameplay could not be driven
  because the sim fast-forwards between MCP calls; see
  `wokwi-verify-artifacts/20260711T124658Z-slot-attract-spin/report.md`. Reel /
  payout / audio behavior to be confirmed on hardware.

### Files touched
- `src/**`, `Makefile`, `README.md`, `docs/**`, `diagram.json`, `wokwi.toml`

### Why
- New portfolio project. The MAX7219 driver was promoted to `extern_libraries/`
  (not duplicated) so the stopwatch and slot machine share one daisy-chain-capable
  driver, per the project's `extern_libraries` discipline.
