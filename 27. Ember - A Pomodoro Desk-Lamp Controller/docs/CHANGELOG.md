# Changelog — Ember (Pomodoro Desk-Lamp Controller)

All notable changes to this project. [Keep a Changelog](https://keepachangelog.com/) style.

## [2026-07-08] - Session: common-cathode display fix

### Fixed
- **Root cause of the dark display: the 7-segment part is common-CATHODE, not
  common-anode.** A bench probe (all segments driven HIGH, then each digit
  common shorted to GND) lit every segment — the definitive common-cathode
  signature. The "CA" family prefix denotes common-anode; the actual part is the
  `CC56-12SRWA` sibling.
- Set `EMBER_SEG_ACTIVE_LOW 0` and `EMBER_DIGIT_ACTIVE_LOW 0` in
  `hardware_connections.h`. The `display.c` driver is polarity-agnostic (all
  handled by those two macros), so no driver logic changed.

### Changed
- **Hardware:** digit drivers reworked from PNP high-side to **NPN low-side**
  switches (2N3904/BC337): emitter → GND, collector → digit common, base → PBx
  via the existing 1 kΩ. A digit now selects on an active-HIGH base.
- Docs (README, Theory §2, hardware_connections) updated to common-cathode /
  NPN low-side throughout.

### Files touched
- src/hardware_connections.h, src/display.c, src/main.c (debug test blocks),
  README.md, docs/Theory.md, docs/hardware_connections.md

### Why
- Firmware + PNP high-side hardware assumed common-anode; with a common-cathode
  part the digit commons must sink to GND (PNP high-side can only source), so
  nothing could ever light until both the polarity macros and the digit-switch
  topology were corrected.

## [2026-07-08] - Session: initial build

### Added
- `src/hardware_connections.h` — single source of truth for all pin_t
  (relay, buzzer, SEG[8]/DIGIT[4] display, encoder CLK/DT/SW) + display
  polarity macros + encoder PCINT mask.
- `src/sys_tick.{c,h}` — Timer0 1 ms timebase. Copied from `relay/sentinel`.
- `src/button.{c,h}` — debounced encoder push-button (short/long). Copied
  from `relay/sentinel`.
- `src/display.{c,h}` — 4-digit common-anode multiplex driven by a Timer2 CTC
  ISR at ~250 Hz; PROGMEM font; MM:SS + right-aligned uint render; colon.
- `src/encoder.{c,h}` — KY-040 quadrature decode via a 16-entry transition
  table in the PCINT2 ISR; bounce-rejecting; whole-detent accumulator.
- `src/buzzer.{c,h}` — non-blocking tone/melody on Timer1 CTC (OC1A/PB1);
  SFX_CONFIRM / SFX_BREAK / SFX_DONE.
- `src/storage.{c,h}` — EEPROM presets (work/break minutes) + lifetime
  session counter; magic-word first-boot defaults; wear-aware updates.
- `src/ember_fsm.{c,h}` — Pomodoro FSM: IDLE→SET_WORK→SET_BREAK→WORK→BREAK.
  Owns relay/display/buzzer + countdown. Lamp ON == break.
- `src/sleep.{c,h}` — SLEEP_MODE_PWR_DOWN in IDLE with PCINT2 (encoder) wake.
  Adapted from `keypad/ascent`.
- `wokwi.toml` + `diagram.json` — Wokwi pre-flash simulation harness.

### Changed
- `src/main.c` — replaced the leftover keypad/anvil scaffold (which included
  lcd_driver/keypad/anvil/ui/leds) with the Ember superloop.
- **Makefile** (surfaced per CLAUDE.md rule 9):
  - `LIBDIR`: `../../lcd/lcd_driver` → `../../extern_libraries` (this project
    has no LCD).
  - `SOURCES`: `$(LIBDIR)/lcd_driver.c` → `$(LIBDIR)/pin/pin.c`.
  - `CPPFLAGS` include path: `-I$(LIBDIR)` → `-I$(LIBDIR)/pin`.
  - Fuses unchanged (16 MHz external crystal; no change needed for PWR_DOWN).

### Why
- Clock/sleep decision: the prompt's async-Timer2 + 32.768 kHz crystal is
  physically incompatible with the standard 16 MHz crystal (both need
  TOSC1/2 = XTAL1/2 = PB6/PB7). A Pomodoro also needs an *accurate* timebase,
  which the 16 MHz crystal gives (<0.1 s/25 min) and the internal RC does not.
  Since the box never needs to keep time while asleep (it only counts after
  you start it), µA idle is achieved instead with SLEEP_MODE_PWR_DOWN +
  encoder PCINT wake — no extra crystal, no fuse change, Timer2 free for the
  display.

### Verified
- Clean build, `-Werror -Wall -Wextra -Wpedantic`. Flash 4484 B (13.7% of
  32 KB), SRAM 221 B (10.8% of 2 KB).
- Wokwi (see `wokwi-verify-artifacts/`): boot→IDLE power-down and the Timer2
  common-anode display multiplex confirmed (COM3 caught HIGH). Encoder
  value-change, wake-from-sleep, and countdown→lamp left for hardware
  (sim harness/timescale limits).

### Timer allocation
- Timer0 = sys_tick (1 ms) · Timer1 = buzzer (OC1A) · Timer2 = display refresh.

### TODO / follow-ups
- Bench the encoder on real hardware: prove clean ±1 detents.
- Add PNP high-side drivers on the digit anodes + 8 segment resistors
  (per-pin current limit — see docs/Theory.md).
- Stretch: UART session dashboard (PD0/PD1 reserved), long-break-every-4,
  SSR vs relay lifetime, 74HC595 to reclaim display pins.
