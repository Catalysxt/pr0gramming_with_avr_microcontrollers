# Changelog

All notable changes to this project. Format follows
[Keep a Changelog](https://keepachangelog.com/).

## [2026-07-12] - Session: debug UART + display rotation

### Added
- Debug serial on PD1/TXD behind `-DDEBUG_UART`: HC-SR04 distance is logged as
  `cm=<n>` / `cm=---` per echo, alongside the FSM transition log (`initUSART()`
  in `main.c` also activates the pre-existing `fsm.c` logging seam).
- `EXTRA_CPPFLAGS` seam in the Makefile so opt-in flags can be passed without
  clobbering the include paths, e.g. `make flash EXTRA_CPPFLAGS=-DDEBUG_UART`.

### Changed
- Display: `dotmatrix_flush()` now rotates the frame 90° CCW to compensate for a
  panel mounted 90° CW (confirmed upright on hardware).
- USART: use the shared `extern_libraries/USART.{c,h}` instead of a project-local
  driver (per CLAUDE.md — shared drivers are never duplicated into `src/`). The
  cm log uses the shared `printWord()` (fixed 5-digit decimal).
- HC-SR04: promoted from `src/drivers/` to the shared `extern_libraries/hcsr04/`
  (new folder + README) so future projects can reuse it. Made clock-portable —
  tick period and out-of-range cutoff now derive from `F_CPU`/prescaler instead
  of a hardcoded `4 us`/`6000 ticks`; identical code at 16 MHz (normal build
  unchanged at 6536/284). ECHO stays fixed to PB0/ICP1 (hardware constraint,
  documented); only TRIG is a `pin_t`. Header guard renamed to
  `EXTERN_LIBRARIES_HCSR04_H_`.

### Removed
- Project-local `src/drivers/USART.{c,h}` (superseded by the shared driver) and
  the temporary HC-SR04 capture-counter instrumentation used to diagnose a dead
  ranging path (root cause: sensor VCC left unconnected — no firmware fault).

### Files touched
- Makefile, src/main.c, src/drivers/dotmatrix.c, src/drivers/dotmatrix.h,
  src/drivers/USART.{c,h} (deleted), src/drivers/hcsr04.{c,h} (moved out),
  extern_libraries/hcsr04/{hcsr04.c,hcsr04.h,README.md} (new shared driver)

### Why
- Debug visibility into the sensor→mood pipeline; compliance with the
  extern_libraries "shared, not duplicated" rule; correct display orientation
  for the angled mount.

## [2026-07-12] - Session: initial firmware bring-up

### Added

- **Drivers** (`src/drivers/`): `timing` (Timer0 1 ms CTC tick), `dotmatrix`
  (single 8×8 framebuffer + MAX7221 flush), `buttons` (debounced, long-press
  capable), `adc` (non-blocking single conversion + free-running seam),
  `hcsr04` (Timer1 ICP1 input-capture ultrasonic ranger — a genuinely new
  driver; none existed in the workspace).
- **App layer** (`src/app/`): `prng` (xorshift32), `mood` (valence/arousal model
  + linear decay + hysteresis region classifier), `fsm` (data-table state
  machine with illegal-transition bridging via HESITANT), `sensors` (proximity
  trend + pet gesture + inactivity drift), `animator` (XOR-morph tween + blink/
  saccade/breathing idle motion), `face` (high-level API). `expression.h` holds
  the shared `expression_t` enum.
- **Assets**: human-readable ASCII-art keyframes in `assets_src/*.txt` (source of
  truth), generated `src/assets/expressions.h`, hand-authored
  `src/assets/idle_motions.h`.
- **Tools**: `tools/png_to_progmem.py` (ASCII/PNG → `expressions.h`),
  `tools/fsm_dot.py` (transition table → `docs/fsm.dot` + `docs/fsm.svg`, with a
  stdlib SVG fallback when Graphviz is absent), `tools/requirements.txt`.
- **Tests** (`tests/`): host-side `test_mood` + `test_fsm` (native gcc), a host
  `Makefile`, and `tests/README.md` documenting what is stubbed.
- **Docs**: `HARDWARE.md`, `README.md`, `docs/Theory.md`, this changelog, and
  `docs/hardware_connections.md`.
- `src/hardware_connections.h` — all `pin_t` instances (single source of truth).

### Changed

- **`Makefile`** (was a verbatim copy of the slot_machine Makefile). Surfaced in
  chat before editing, per CLAUDE.md:
  - `TARGET`: `slot_machine` → `digital_companion`.
  - Dropped `font7seg` from the shared sources and `-I` paths — a dot-matrix face
    has no 7-segment glyphs; the MAX7219 driver runs in no-decode mode and
    `dotmatrix.c` feeds it raw row bytes.
  - Added `host-test` (delegates to `tests/Makefile`) and `fsm-diagram` (runs
    `tools/fsm_dot.py`) targets; updated `squeaky_clean` and `.PHONY`.

### Fixed

- `mood_classify` hysteresis bug caught by the host tests: `EXPR_NEUTRAL` was an
  unconditional catch-all in `in_region()`, so the stickiness check kept the FSM
  in NEUTRAL forever. NEUTRAL is now a bounded resting deadzone; the classify
  fallback still sweeps up any gaps between regions.

### Design deviation from the prompt

- **LDR ambient sensor removed** (user-directed). No Tier-1 ambient tier. SLEEPY
  now comes from sustained inactivity (arousal drifting down), SURPRISED from a
  sudden HC-SR04 proximity spike. ADC0 freed up as the floating PRNG-seed pin.

### Files touched

- `Makefile`, `src/**`, `assets_src/**`, `tools/**`, `tests/**`, `HARDWARE.md`,
  `README.md`, `docs/**`.

### Why

- First implementation of the portfolio project from `docs/prompt.md`: a
  sensor-caused expressive face with a tween engine and an explicit emotion
  model, built bottom-up on the workspace's shared SPI/MAX7219/pin drivers.

### Memory footprint (avr-size, `-Os`)

- flash `.text + .data` ≈ **6.7 KB / 32 KB (~20 %)**
- SRAM `.data + .bss` ≈ **394 B / 2 KB (~19 %)**
