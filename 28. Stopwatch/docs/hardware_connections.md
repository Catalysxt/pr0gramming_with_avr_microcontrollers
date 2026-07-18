# Hardware connections

**MCU:** ATmega328P @ 16 MHz external crystal · **Driver:** one MAX7219 ·
**Display:** two 4-digit common-cathode 7-segment modules (8 digits total) ·
**Controls:** two momentary pushbuttons (start/pause, clear).

A single MAX7219 multiplexes up to 8 digits — exactly our count — so one chip
drives both modules with no daisy-chaining. The stopwatch uses seven of them
(minutes · seconds · milliseconds); DIG7 is left blank.

## MCU ↔ MAX7219 (hardware SPI)

| Net       | From (RefDes, Pin) | To (RefDes, Pin) | MCU pin   | Notes                                     |
| :-------- | :----------------- | :--------------- | :-------- | :---------------------------------------- |
| SPI_DIN   | U1, PB3 (MOSI/D11) | U2, DIN (pin 1)  | PB3 / D11 | Serial data in                            |
| SPI_CLK   | U1, PB5 (SCK/D13)  | U2, CLK (pin 13) | PB5 / D13 | Serial clock, ≤10 MHz                     |
| SPI_LOAD  | U1, PB2 (SS/D10)   | U2, LOAD/CS (12) | PB2 / D10 | Active-low latch; kept output (see below) |
| VCC       | +5V                | U2, V+ (pin 19)  | -         | 4.0–5.5 V                                 |
| GND       | GND                | U2, GND (4, 9)   | -         | Both GND pins to ground                   |
| ISET      | U2, ISET (pin 18)  | +5V via R_SET    | -         | R_SET ≈ 10 kΩ sets peak segment current   |
| C_BYP     | U2, V+             | GND              | -         | 0.1 µF + 10 µF bypass close to the chip   |

**U1** = ATmega328P, **U2** = MAX7219.

> PB2 (SS) must stay an **output** while the SPI hardware is master: if it is an
> input and pulled low, the SPI unit auto-clears MSTR and goes silent
> (ATmega328P datasheet, master-mode SS note). We use PB2 as our CS, so it is
> always an output — no conflict.
>
> MISO (PB4) is unused — the MAX7219 has no data-out to read back.

## MCU ↔ pushbuttons

| Net       | From (RefDes, Pin) | To (RefDes, Pin) | MCU pin  | Notes                         |
| :-------- | :----------------- | :--------------- | :------- | :---------------------------- |
| BTN_START | U1, PD6 (D6)       | SW1, pin 1       | PD6 / D6 | Start/pause; other pin to GND |
| BTN_CLEAR | U1, PD7 (D7)       | SW2, pin 1       | PD7 / D7 | Clear/reset; other pin to GND |

**SW1/SW2** are momentary pushbuttons. Each connects its MCU pin to **GND** when
pressed; the firmware enables the ATmega's **internal pull-ups**, so an idle
button reads HIGH and a press reads LOW — **no external resistors needed**. PD6
and PD7 are otherwise free (the SPI unit owns PB2/PB3/PB5; PD0/PD1 are the USART
pins, unused here). Debouncing is handled in software (`button_pressed()` in
`src/main.c`), so no RC hardware debounce is required.

## MAX7219 ↔ 7-segment digits

The MAX7219 has 8 SEG outputs (SEG A..G, DP) shared across 8 DIG (cathode)
drivers, scanned one digit at a time.

| MAX7219 pin  | Connects to                                          |
| :----------- | :--------------------------------------------------- |
| SEG A..G, DP | The a..g + dp anode rails, tied across all 8 digits  |
| DIG0..DIG7   | The common cathode of each digit, one per digit      |

**Digit ordering:** wire **DIG0 → leftmost digit … DIG7 → rightmost** to match
the identity `s_digit_map[]` in `src/main.c`. If your module numbers digits the
other way, just reverse that one array — no driver changes needed.

## Simulation (Wokwi)

`diagram.json` substitutes a `wokwi-max7219-matrix` (Wokwi has no MAX7219 7-seg
part). The SPI wiring (DIN/CLK/CS) is identical, so the firmware path is the
same; only the visual differs. Two `wokwi-pushbutton` parts on D6/D7 to GND
stand in for the start/pause and clear buttons. A `wokwi-logic-analyzer` taps the
3 SPI nets for byte-level verification. See `wokwi-verify-artifacts/` for decoded
traces.
