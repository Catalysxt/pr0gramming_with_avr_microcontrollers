# Anvil — A Secure Safe

## Overview

ANVIL turns an ATmega328P (and other components) into an electronic safe!

## Hardware

- ATmega328P (DIP-28), 16 MHz external crystal + 2×22 pF
- 16×2 HD44780-compatible LCD, 4-bit mode
- 4×4 matrix keypad (8 pins: 4 rows, 4 columns)
- 2× LED (green, red), each with a 330 Ω series resistor
- 1× passive piezo buzzer, optional 100 Ω series resistor
- 10 kΩ potentiometer (LCD contrast)
- Internal EEPROM (magic word + 4-byte PIN, ≤ 7 bytes used)

Full pin table, schematic wire list, ASCII block diagram, and the
multimeter-based keypad pinout-discovery procedure:
[`docs/hardware_connections.md`](docs/hardware_connections.md).

## Theory

Covers HD44780 DDRAM/CGRAM and busy-flag polling, matrix-keypad scan
electrics and debounce, passive-piezo square-wave tone generation via
Timer2 CTC, EEPROM wear-leveling, and constant-time PIN comparison:
[`docs/Theory.md`](docs/Theory.md).

## Build & Flash

Requires `avr-gcc`, `avr-objcopy`, `avr-size`, and `avrdude` (USBasp
programmer) on `PATH`.

```sh
make            # build build/anvil.hex
make size       # report flash/SRAM usage
make flash      # flash over USBasp
make fuses      # set fuses for the external 16 MHz crystal (LFUSE=0xF7 HFUSE=0xD9 EFUSE=0x07)
```

By default `HFUSE=0xD9` erases EEPROM on every chip-erase (i.e. every
`make flash` that does a full erase). Run `make set_eeprom_save_fuse`
once to switch to `HFUSE=0xD7` if you want the stored PIN to survive
re-flashing; `make clear_eeprom_save_fuse` reverts it.

## How to use

- **Default PIN: `1234`** — written to EEPROM automatically the first
  time the firmware boots against blank/erased EEPROM.
- Type digits `0`–`9` to enter a PIN. `B` backspaces one digit, `C`
  clears the whole entry, `A` submits (ignored with an error beep if
  fewer than 4 digits have been typed).
- Correct PIN → green LED, success chime, `UNLOCKED` for 10 s (any
  keypress during that window extends it back to 10 s), then
  auto-relocks to the splash screen.
- Wrong PIN → red LED flash, fail tone, `WRONG PIN` for 1 s, then back
  to entry. Three wrong PINs in a row trigger a 30 s lockout with a
  live countdown and a slow-blinking red LED; the attempt counter
  resets to 3 once the lockout ends (or on power-cycle — it's RAM-only
  by design, see below).
- **To change the PIN**: from the entry screen, hold `D` for at least
  one second. You'll be prompted for the current PIN, then a new PIN,
  then to confirm the new PIN. A match writes the new PIN to EEPROM and
  shows `PIN CHANGED`; any mismatch along the way shows `MISMATCH` and
  aborts back to the splash screen without changing anything.
- 30 seconds of no keypress blanks the display (screensaver); any
  keypress wakes it. Waking from the PIN-entry screen resumes entry
  with a blank buffer; waking from anywhere else (including mid change-
  PIN flow) returns to the splash screen.
- Keys `*`, `#`, and a short press of `D` are reserved/unused.

## Practical checklist

1. Power on with a blank/erased chip. LCD should show the splash screen
   (lock icon + `ANVIL` / `Enter PIN:`). Type `1234` then `A` — LED
   goes green, `UNLOCKED` shows for 10 s, then it auto-relocks.
2. **First-boot test**: erase EEPROM (`avrdude -c usbasp -p atmega328p
   -U eeprom:w:0xFF:m` after building a blank-fill file, or use your
   programmer's chip-erase) and power-cycle. Confirm it still unlocks
   with `1234` (the magic-word check reinitialised it).
3. **Lockout test**: enter 3 wrong PINs in a row. Confirm the LCD shows
   a `LOCKED` countdown from `30s`, the red LED blinks about once a
   second, and after it reaches 0 the display returns to PIN entry
   with a fresh 3 attempts.
4. **Change-PIN happy path**: hold `D` from the entry screen (≥ 1 s),
   enter the current PIN, then a new 4-digit PIN twice matching. Should
   show `PIN CHANGED`. Power-cycle and confirm the new PIN unlocks and
   the old one no longer does.
5. **Change-PIN mismatch path**: repeat the flow but enter the wrong
   current PIN (or type two different "new" PINs). Should show
   `MISMATCH` and land back on the splash screen with the PIN
   unchanged.
6. **Power-cycle resets the fail counter**: get 1–2 wrong PINs in (not
   enough to lock out), power-cycle, and confirm the tries-left counter
   is back at 3 rather than continuing where it left off — the fail
   counter is RAM-only by design.
7. Let it sit idle for 30 s at the entry screen — the display should
   blank. Press any key — it should wake back to a blank entry prompt.
8. Type a digit and watch it briefly show as a plain digit before
   masking to `*` (the 200 ms reveal-then-mask).

## Memory footprint

```
   text    data     bss     dec     hex filename
   5092     230      42    5364    14f4 build/anvil.elf
```

Flash: 5322 / 32768 bytes (~16%). SRAM: 272 / 2048 bytes (~13%). Well
under the 75% warning threshold for both.

## Stretch goals

- PD0/PD1 (RXD/TXD) are reserved and untouched — a serial console for
  remote status/logging would be a natural extension.
- LCD backlight PWM dimming (the idle screensaver currently just clears
  the display and turns it off; no backlight control exists yet).
- **V2 (explicitly out of scope for this build)**: a solenoid actuator
  that physically locks/unlocks access to whatever the safe contains.
