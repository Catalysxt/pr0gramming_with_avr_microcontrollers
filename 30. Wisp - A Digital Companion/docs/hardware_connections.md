# Hardware connections — Digital Companion

MCU: **ATmega328P @ 16 MHz** (external crystal). Programmer: **USBasp**.
All firmware pin assignments come from `src/hardware_connections.h` (the single
source of truth). `U1` = the ATmega328P.

| Net             | From (RefDes, Pin) | To (RefDes, Pin)  | MCU pin | Notes                                   |
| :-------------- | :----------------- | :---------------- | :------ | :-------------------------------------- |
| MAX_DIN         | U1, PB3            | MAX1, DIN         | PB3     | SPI MOSI                                 |
| MAX_CLK         | U1, PB5            | MAX1, CLK         | PB5     | SPI SCK                                  |
| MAX_LOAD        | U1, PB2            | MAX1, CS/LOAD     | PB2     | active-low latch; idles high             |
| MATRIX          | MAX1, SEG/DIG      | LED8x8, rows/cols | -       | MAX7221 drives the 8x8 module            |
| ECHO            | HC1, ECHO          | U1, PB0/ICP1      | PB0     | Timer1 input capture; 5 V logic          |
| TRIG            | U1, PD4            | HC1, TRIG         | PD4     | 10 µs ping pulse                         |
| PET_BTN         | U1, PB1            | SW1, pin1         | PB1     | other side to GND; internal pull-up      |
| ADC0_FLOAT      | U1, PC0            | (none)            | PC0     | left floating — PRNG entropy; do not wire |
| MIC (Tier 3)    | MIC1, OUT          | U1, PC1/ADC1      | PC1     | scaffold only (`ENABLE_AUDIO`)           |
| SCOPE           | U1, PD3            | TP1               | PD3     | oscilloscope test point                  |
| DEBUG_TX        | U1, PD1            | FTDI, RX          | PD1     | only if built with `DEBUG_UART`          |
| VCC             | +5V                | MAX1 VCC, HC1 VCC | -       | see current note below                   |
| GND             | GND                | all GND           | -       | common ground                            |

## Power / decoupling notes

- **MAX7221 supply:** 0.1 µF ceramic across VCC/GND close to the chip, plus a
  10 µF bulk cap. RSET (~28 kΩ from ISET to VCC) sets LED segment current — the
  '7221 is internally current-limited and slew-rate limited (quieter than the
  '7219); confirm the module already carries RSET.
- **HC-SR04:** draws a current spike on each ping; give it a local 100 µF or
  brown-outs will glitch the ranger.
- **Crystal:** 16 MHz with 2×22 pF to GND on XTAL1/XTAL2 (fuses set for a
  full-swing external crystal — see the Makefile).

## HC-SR04 ECHO level

ECHO is a 5 V push-pull output. At `Vcc = 5 V` it drives PB0 directly. If you run
the ATmega at 3.3 V, divide ECHO down (e.g. 1 kΩ series + 2 kΩ to GND) before
PB0 to stay inside the input spec.
