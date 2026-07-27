# ASCENT — An LCD Calculator

## Overview

ASCENT is bare-metal AVR-C firmware for an ATmega328P @ 16 MHz that
turns a 4×4 matrix keypad, a 16×2 HD44780 LCD, and a passive piezo
buzzer into a chained-arithmetic calculator over `int32_t` operands
(range ±2,147,483,647), with overflow and divide-by-zero detection and a
30-second idle sleep mode that wakes on any keypress. No Arduino
framework — direct register access throughout, non-blocking superloop.

**Evaluation is strict left-to-right, with no operator precedence.**
There's no PEMDAS: `2 + 3 x 4` evaluates as `(2 + 3) x 4 = 20`, not `14`.
Each operator key immediately commits whatever's pending against the
running result, the same way a simple four-function calculator (not a
scientific one) behaves.

## Hardware

- ATmega328P (DIP-28), 16 MHz external crystal + 2×22 pF
- 16×2 HD44780-compatible LCD, 4-bit mode
- 4×4 matrix keypad (8 pins: 4 rows, 4 columns)
- 1× passive piezo buzzer, optional 100 Ω series resistor
- 10 kΩ potentiometer (LCD contrast)
- 1× LED on PB4, wired but unused by v1 firmware (reserved for future use)

Full pin table, schematic wire list, ASCII block diagram, and the
multimeter-based keypad pinout-discovery procedure:
[`docs/hardware_connections.md`](docs/hardware_connections.md).

## Theory

Covers HD44780 DDRAM/busy-flag polling, matrix-keypad scan electrics and
debounce, passive-piezo square-wave tone generation via Timer2 CTC,
two's-complement overflow (and why `abs(INT32_MIN)` is undefined
behaviour), and the `SLEEP_MODE_PWR_DOWN` + PCINT wake mechanism:
[`docs/Theory.md`](docs/Theory.md).

## Build & Flash

Requires `avr-gcc`, `avr-objcopy`, `avr-size`, and `avrdude` (USBasp
programmer) on `PATH`.

```sh
make            # build build/ascent.hex
make size       # report flash/SRAM usage
make flash      # flash over USBasp
make fuses      # set fuses for the external 16 MHz crystal (LFUSE=0xF7 HFUSE=0xD9 EFUSE=0x07)
```

## How to use

**Key map:**

| Key(s) | Function |
| --- | --- |
| `0`–`9` | Digit entry (right-aligned on row 1, cap 10 digits per operand) |
| `A` | `+` |
| `B` | `−` |
| `C` | `×` |
| `D` | `÷` |
| `#` | `=` (evaluate) |
| `*` | Clear (reset to `0` from any state) |

- On boot, the LCD shows `ASCENT v1` / `Calc Ready`; the splash clears
  on the first keypress (which is also processed as the first real key,
  not swallowed).
- Type an operand, press an operator, type the next operand, and so on —
  each operator commit immediately folds the pending operand into the
  running result and shows it on row 1, while row 0 accumulates the
  full expression (scrolling left with a `<` indicator once it exceeds
  16 characters). Press `#` to get the final result; any key after that
  wipes the slate and starts fresh (a digit re-seeds the next operand
  directly, matching how a physical calculator behaves after `=`).
- **Leading zeros are suppressed**: typing `0` then `5` shows `5`, not
  `05`; typing `0` twice in a row collapses back to a single `0`.
- **Operator-replace**: pressing an operator key while another is
  already armed (and no digit has been typed yet) replaces it instead of
  erroring — `5, +, −, 3, =` gives `5 − 3 = 2`, not `5+−3` stacking up.
- **Overflow / divide-by-zero**: `OVERFLOW` or `DIV BY ZERO` freezes the
  display for 1.5 seconds (even `*` is ignored during the freeze) before
  automatically resetting to a fresh `0`.
- **Sleep**: 30 seconds with no keypress blanks the display and powers
  down most of the chip. Press any key to wake — the wake key is the
  first key of the next interaction, and whatever you were doing before
  sleep (a half-typed operand, a pending operator, an error freeze is
  exempt from sleep entirely) picks up exactly where it left off.

## Practical checklist

1. Power on. LCD shows `ASCENT v1` / `Calc Ready`. Press any key — the
   splash clears and that key is treated as the start of a fresh
   calculation.
2. `1`, `2`, `+`, `3`, `4`, `=` → row 1 shows `46`.
3. `1`, `0`, `0`, `÷`(`D`), `7`, `=` → row 1 shows `14` (integer division
   truncates toward zero).
4. `1000000`, `×`(`C`), `1000000`, `=` → `OVERFLOW` for 1.5 s, then
   auto-resets to `0`.
5. `2147483647`, `+`(`A`), `1`, `=` → `OVERFLOW`.
6. `5`, `÷`(`D`), `0`, `=` → `DIV BY ZERO` for 1.5 s, then auto-resets.
7. `5`, `+`(`A`), `−`(`B`), `3`, `=` → `2` (the `+` is replaced by `−`
   before any digit is typed).
8. `0`, `0`, `5` → row 1 shows `5`, never `005` or `05`.
9. Enter `5`, `+`, `3`, `=` and then leave the calculator idle for 30 s —
   the display should blank. Press any key — it wakes back to the
   `5 + 3 = 8` result screen (any key from `ST_RESULT` then wipes and
   starts fresh, same as if sleep never happened).

## Memory footprint

```
   text    data     bss     dec     hex filename
   5110     134     117    5361    14f1 build/ascent.elf
```

Flash: 5244 / 32768 bytes (~16%). SRAM: 251 / 2048 bytes (~12%). Well
under the 75% warning threshold for both.

## Extensions

Called out per spec, not implemented in this build:

- Signed-operand entry (long-press `B` to negate the current operand)
- Memory register (`A` long-press to store, `#` long-press to recall)
- PEMDAS precedence with a two-stack evaluator
- Modulo / square / sqrt on long-presses of digit keys
- USART debug log behind a compile-time `DEBUG` flag (PD0/PD1 are
  reserved and untouched for exactly this)
- Floating-point or fixed-point fractional results
