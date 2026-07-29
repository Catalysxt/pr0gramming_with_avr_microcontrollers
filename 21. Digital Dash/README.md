# digital_dash

A bare-metal AVR C port of a side-scrolling LCD runner game for the ATmega328P at 16 MHz.
Dodge hills by jumping and crows by ducking. Your high score persists across power cycles.

---

## Overview

A stick figure runs along a 16×2 character LCD. Hills scroll along row 1; crows (two-frame animated) scroll along row 0. Press **JUMP** to leap over hills and hold **DUCK** to pass under crows. Each obstacle successfully cleared earns one point. A passive piezo plays audio feedback on every action. The game is fully non-blocking: a Timer0 1 ms tick drives all timing, leaving the CPU free between game ticks.

This project features:
- An EEPROM-persisted high score shown on the splash screen and game-over screen
- RNG seed from a floating ADC pin
- Non-blocking architecture.

---

## Hardware

See [`docs/hardware_connections.md`](docs/hardware_connections.md) for the full connection table, schematic wire list, and bill of materials.

**Key pin assignments:**

| Signal     | MCU pin       | Notes                                |
| ---------- | ------------- | ------------------------------------ |
| LCD RS     | PB0           | Managed by lcd_driver                |
| LCD RW     | PB1           | Managed by lcd_driver                |
| LCD EN     | PB2           | Managed by lcd_driver                |
| LCD D4–D7  | PD4–PD7       | Managed by lcd_driver                |
| JUMP button | PC0 / PCINT8 | Active-low, internal pull-up         |
| DUCK button | PC1 / PCINT9 | Active-low, internal pull-up         |
| Buzzer     | PB3 / OC2A    | Passive piezo to GND                 |
| RNG seed   | PC2 / ADC2    | Leave **unconnected** (floating)     |

---

## Theory

The game uses: HD44780 4-bit CGRAM for custom glyphs, Timer0 CTC for the 1 ms tick, PCINT1 pin-change interrupts for the buttons, Timer2 CTC with OC2A toggle for the piezo, ADC2 floating-pin noise for RNG seeding, and `eeprom_update_*` for wear-levelled high score storage.

Full technical explanation: [`docs/Theory.md`](docs/Theory.md)

---

## Build & Flash

```bash
# Build
make

# Flash (USBasp)
make flash

# Check memory usage
make size

# Set fuses for 16 MHz external crystal (first-time setup)
make fuses

# Preserve EEPROM (high score) across re-flashing
make set_eeprom_save_fuse
```

Toolchain required: `avr-gcc`, `avr-binutils`, `avrdude` on PATH.

---

## How to Use

1. Power on — LCD shows:
   ```
     Digital Dash  
   HI:0    PressJMP
   ```
2. Press **JUMP** to start.
3. **JUMP** (tap): player leaps to row 0 for 3 ticks — clears hills on row 1.
4. **DUCK** (hold): player shrinks to row 1 — passes under crows on row 0.
5. Each cleared obstacle awards +1 point.
6. When a hill or crow reaches column 0 while you are not in the correct state:
   - Death sound plays (200 Hz / 500 ms).
   - Screen shows score and high score on row 0.
   - Row 1 blinks "Jump to Continue".
7. Press **JUMP** to restart immediately.

**Behavioural differences from the original Arduino sketch:**

| Aspect | Original | This port |
| --- | --- | --- |
| Button polarity | Active-high | Active-low with internal pull-up |
| Tick scheduling | `delay(90)` blocking | `sys_millis()` non-blocking |
| Tone generation | `tone()` | Timer2 CTC on OC2A |
| RNG seed | Never seeded (same game every time) | Floating ADC2 — different each power cycle |
| High score | Not tracked | EEPROM-persisted |
| Splash screen | None | Title + current HI |

---

## Practical Checklist

Follow these steps in order to validate the build on hardware.

1. Build and flash: `make && make flash`. No compiler errors or warnings expected.
2. Power on — LCD row 0 shows `  Digital Dash  `, row 1 shows `HI:0    PressJMP`.
3. Press JUMP — screen clears and the stick figure appears at column 0, row 1, walking (alternating sprites).
4. After a few seconds a hill should scroll in from the right along row 1. Let it reach column 0 without jumping — confirm death sound plays (low ~200 Hz tone) and the game-over screen appears with `Score:0`.
5. Press JUMP on the game-over screen — game restarts cleanly.
6. During gameplay, press JUMP when a hill is approaching — stick figure should move to row 0 for approximately 3 ticks, then return to row 1. Hill should disappear (scored).
7. During gameplay, hold DUCK — stick figure should switch to duck sprite on row 1. Release — returns to normal.
8. Let a crow scroll in from the right (it takes longer to appear — crow starts at ~column 40). Duck while it passes — confirm +1 score.
9. Score at least 1 point, then let a collision occur. Confirm game-over shows `Score:1` and `HI:1`.
10. Power-cycle the board (disconnect and reconnect 5 V). Confirm splash screen shows `HI:1` — high score survived.
11. Play again, beat the high score. Power-cycle again — confirm the new high score is shown.

---

## Tuning Knobs

All in `src/game.h`:

| Constant | Default | Effect |
| --- | --- | --- |
| `TICKSPEED_MS` | 90 | ms per game tick — lower = faster game |
| `JUMP_LENGTH` | 3 | ticks airborne — higher = longer jump |
| `JUMP_PITCH` | 2700 | Hz for jump SFX |
| `DUCK_PITCH` | 1350 | Hz for duck SFX |
| `DIE_PITCH` | 200 | Hz for death SFX |

Obstacle respawn distances are in `src/game.c` (`HILL_RESPAWN_BASE`, `CROW_RESPAWN_BASE` and their `_RAND` variants).

---

## Memory Footprint

*(Update after each build with `make size`)*

| Section | Size | Limit (75%) |
| ------- | ---- | ----------- |
| .text (flash) | 3 416 B (10.4%) | 24 576 B |
| .data + .bss (SRAM) | 134 B (6.5%) | 1 536 B |

---

## Stretch Goals

- **Difficulty ramp**: increase tick speed every N points (`TICKSPEED_MS` decreases toward a floor).
- **Multi-life mode**: start with 3 lives; lose one per collision before game-over.
- **Two-button menu**: on the splash screen, use JUMP/DUCK to select difficulty (easy/normal/hard), stored in EEPROM.
- **Combo multiplier**: consecutive obstacles cleared without a miss multiply the score.
- **Sound themes**: different SFX pitch sets selectable from the menu.
