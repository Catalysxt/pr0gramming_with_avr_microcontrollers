## [2026-06-27] - Session: initial bare-metal port

### Added
- `Makefile` — avr-gcc / avrdude build targeting ATmega328P @ 16 MHz (USBasp)
- `src/hardware_connections.h` — single source of truth for button, buzzer, and RNG ADC pins
- `src/sys_tick.h / sys_tick.c` — Timer0 CTC 1 ms timebase; `sys_millis()`, `sys_elapsed()`
- `src/buzzer.h / buzzer.c` — non-blocking Timer2 CTC tone generator on OC2A (PB3)
- `src/buttons.h / buttons.c` — PCINT1 ISR for PC0 (JUMP) and PC1 (DUCK) with 5 ms debounce
- `src/sprites.h / sprites.c` — 7 PROGMEM glyph patterns + CGRAM loader
- `src/rng.h / rng.c` — ADC2 floating-pin seed + xorshift16 PRNG
- `src/high_score.h / high_score.c` — EEPROM-persisted high score with magic-byte validation
- `src/game.h / game.c` — game state struct, `game_tick_update()`, `game_end()`
- `src/main.c` — top-level state machine (SPLASH → PLAYING → GAMEOVER)
- `docs/Theory.md`, `docs/hardware_connections.md`, `README.md`

### Why
- Port the Arduino `digital_dash` side-scroller to bare-metal AVR C on ATmega328P.
- Eliminates `LiquidCrystal.h`, `tone()`, `millis()`, and blocking `delay()`.
- Adds: non-blocking tick system, EEPROM high score, ADC-seeded RNG, splash screen.

### Spec corrections applied
- RNG uses ADC2 / PC2 (floating), not ADC0 — PC0 has the JUMP button pull-up.
- `hardware.h` renamed to `src/hardware_connections.h` per CLAUDE.md convention.
- Source files placed in `src/` per CLAUDE.md project structure.

### Files touched
- (all new) Makefile, src/*, docs/*, README.md
