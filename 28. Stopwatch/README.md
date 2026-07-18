# MAX7219 — Two-button stopwatch

## Overview

A stopwatch on **eight** 7-segment digits (two 4-digit modules) driven by a
**single MAX7219**. Two pushbuttons control it: one starts/pauses, the other
clears. The elapsed time is laid out across seven digits as **minutes . seconds
. milliseconds**, with the decimal points acting as field separators:

```
 DIG0 DIG1   DIG2 DIG3   DIG4 DIG5 DIG6   DIG7
  \minutes/   \seconds/   \ milliseconds /  (blank)
    0    0  .   0    0  .   0    0    0
```

The MAX7219 runs in **no-decode mode**: the firmware pushes raw segment bitmaps
(digit glyphs from a small ASCII font) into each digit register, so the display
is really an 8-byte framebuffer we blit to the chip.

The time base is a **Timer1 CTC interrupt firing every 1 ms** — never a
`_delay_ms()` busy-wait — so the CPU is always free to poll the buttons and
repaint, and no milliseconds are lost to display or debounce work.

## Hardware

- ATmega328P @ 16 MHz (external crystal), USBasp programmer
- 1 × MAX7219/MAX7221 LED driver
- 2 × 4-digit common-cathode 7-segment displays (8 digits total)
- 2 × momentary pushbuttons (start/pause, clear)
- `R_SET` ≈ 10 kΩ (segment current), 0.1 µF + 10 µF bypass caps

Full wire list: [docs/hardware_connections.md](docs/hardware_connections.md).

| Signal        | AVR pin | To                              |
| :------------ | :------ | :------------------------------ |
| DIN           | PB3     | MAX7219 DIN                     |
| CLK           | PB5     | MAX7219 CLK                     |
| LOAD/CS       | PB2     | MAX7219 LOAD                    |
| START/PAUSE   | PD6     | button → GND                    |
| CLEAR         | PD7     | button → GND                    |

The buttons are **active-low**: each shorts its pin to GND when pressed, and the
firmware enables the ATmega's internal pull-ups, so no external resistors are
needed.

## Theory

The MAX7219 time-multiplexes 8 digits over 3 SPI wires; the host sends 16-bit
{register, data} frames latched on CS's rising edge. We disable the on-chip
Code-B decoder and push raw segment bitmaps, which is what lets us light the
decimal-point separators alongside the digits. Full write-up:
[docs/Theory.md](docs/Theory.md).

## Build & Flash

```sh
make            # -> build/max7219_max7221.hex
make size       # flash / SRAM usage
make flash      # program via USBasp
make fuses      # one-time: external 16 MHz crystal fuses
```

## How to use

1. Power up — the display shows `00.00.000`, stopped.
2. Press **START/PAUSE** (PD6) → the count begins advancing.
3. Press **START/PAUSE** again → the count freezes (paused), holding its value.
4. Press it again → counting resumes from where it paused.
5. Press **CLEAR** (PD7) at any time → the count resets to `00.00.000`. If the
   stopwatch was running, it keeps running from zero; if paused, it stays at zero.

The milliseconds ones-digit changes 1000×/sec, so it reads as a blur — that's
normal for any stopwatch showing thousandths. Minutes wrap back to `00` after
`99.59.999`.

## Practical checklist

1. `make flash` succeeds; the display lights up showing `00.00.000`.
2. Both decimal-point separators (after minutes and after seconds) are lit.
3. START/PAUSE begins the count; the seconds field rolls the minutes over at 60.
4. START/PAUSE again holds the value steady (no drift while paused).
5. CLEAR returns the display to `00.00.000` both while running and while paused.
6. Over a ~1-minute run, the time tracks a phone stopwatch to within a fraction
   of a second (crystal-based accuracy).

## Memory footprint

| Section        | Bytes | Budget        |
| :------------- | ----: | :------------ |
| `.text` (flash)| 2112  | 32 KB (6.4 %) |
| `.bss` (SRAM)  | 17    | 2 KB          |
| stack          | ~few  | debounce state|

`.bss` = 4 (free-running tick) + 4 (elapsed-ms counter) + 1 (running flag) +
8 (framebuffer).

## Stretch goals

- **Lap / split** — a third button (or a long-press on CLEAR) that freezes the
  shown time while the count keeps running underneath; use the spare DIG7.
- **Hours mode** — switch DIG0–1 to hours once past 100 minutes, or reformat to
  `HH.MM.SS` for longer intervals.
- **Countdown timer** — preset a value and count *down* to `00.00.000`, then
  flash the display or drive a buzzer at zero.
- **Brightness control** — repurpose a button hold to step intensity 0–15 live
  (`max7219_set_intensity` is already there).
- **Hundredths instead of thousandths** — show `MM.SS.hh` (2 ms digits) for a
  steadier, more readable low field, freeing two digits for other data.
