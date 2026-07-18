# Hardware connections — Slot machine (ATmega328P)

MCU: **ATmega328P @ 16 MHz** external crystal · Programmer: **USBasp** ·
bare-metal avr-gcc. **Displays:** score MAX7221 (two 4-digit 7-seg) + 4-panel 8×8
dot-matrix module (4 internal MAX7219s) · **Controls:** 2 pushbuttons · **Audio:**
1 passive buzzer. All display chips share one SPI bus (MOSI/SCK/CS). Reuses the
stopwatch's SPI + button wiring and adds the buzzer and the dot-matrix module.

## Pin table

| Function          | MCU pin | Arduino | Direction   | Connects to                          |
| :---------------- | :------ | :------ | :---------- | :----------------------------------- |
| SPI MOSI (DIN)    | PB3     | D11     | out         | chain DIN (score chip only)          |
| SPI SCK  (CLK)    | PB5     | D13     | out         | chain CLK (shared, all chips)        |
| SPI /LOAD (CS)    | PB2     | D10     | out         | chain CS / LOAD (shared, all chips)  |
| Buzzer            | PB1     | D9      | out         | passive buzzer / piezo → GND (OC1A)  |
| BTN_A SPIN/SELECT | PD6     | D6      | in, pull-up | pushbutton → GND                     |
| BTN_B MENU/BACK   | PD7     | D7      | in, pull-up | pushbutton → GND                     |
| PRNG seed         | PC0     | A0      | in (float)  | **unconnected** — floating ADC noise |
| MISO              | PB4     | D12     | —           | unused (MAX chips have no read-back) |

`PB2` must stay an output while the SPI unit is master; as our CS it always is
(else `MSTR` auto-clears and the bus goes silent — ATmega328P datasheet,
master-mode SS note). MISO (PB4) is unused — the MAX chips have no data-out.

## SPI daisy chain (both MAX subsystems on one bus)

Everything hangs off **one** SPI bus sharing MOSI, SCK, and a single active-low
/LOAD (CS). Only one data wire leaves the MCU (PB3) and it reaches the score chip
alone; the dot-matrix module's DIN is fed from the score chip's **DOUT**. The
driver shifts **5 cascaded 16-bit frames per register**, farthest chip first; one
CS rising edge latches all five.

```
 ATmega328P                       ┌──────────────── 4-in-1 8x8 dot-matrix module ───────────────┐
                                  │  (self-contained: 4 internal MAX7219s, one per 8x8 panel)   │
  PB3 MOSI ─DIN►[ MAX7221 ]DOUT─►─┼─DIN►[MAX7219]►[MAX7219]►[MAX7219]►[MAX7219]►DOUT (n/c)       │
              │   score chip      │        panel0    panel1    panel2    panel3                  │
  PB5 SCK ────┴───────────────────┼──► CLK  (fanned to all 4 internally) ──────────────────────►│
  PB2 /LOAD ──┬───────────────────┼──► CS   (fanned to all 4 internally) ──────────────────────►│
              │                   └─────────────────────────────────────────────────────────────┘
              └─► CS/CLK also to the score chip (shared with the module)

  device index:   0                    1         2         3         4
                  score (8x 7-seg)     ── dot-matrix panels 0..3 (reels + accent) ──
```

The dot-matrix module exposes a **5-pin header (VCC, GND, CLK, CS, DIN)** and
plugs straight onto the SPI bus — it already contains its four MAX7219 drivers, so
**no external driver chip is added**. Its DIN comes from the score chip's DOUT; its
CLK and CS tie to the shared lines.

### Driver split (explicit confirmation)

- The **existing MAX7221 (device 0)** drives **only** the two 4-digit 7-segment
  displays (8 digits): Display A = Credits/Balance, Display B = Last Win.
- The **dot-matrix module's own four MAX7219s (devices 1–4)** drive **only** the
  four 8×8 panels. The score chip and the matrix never share a driver IC — they
  only share the SPI bus + CS.

## Connection tables

### MCU ↔ SPI chain

| Net       | From (RefDes, Pin) | To (RefDes, Pin)      | MCU pin   | Notes                                 |
| :-------- | :----------------- | :-------------------- | :-------- | :------------------------------------ |
| SPI_DIN   | U1, PB3 (MOSI/D11) | U2, DIN               | PB3 / D11 | Serial data into the score chip       |
| SPI_CLK   | U1, PB5 (SCK/D13)  | U2+MOD, CLK           | PB5 / D13 | Shared clock, ≤10 MHz (run at 1 MHz)  |
| SPI_LOAD  | U1, PB2 (SS/D10)   | U2+MOD, CS/LOAD       | PB2 / D10 | Shared active-low latch; kept output  |
| CHAIN     | U2, DOUT           | MOD, DIN              | -         | score DOUT → dot-matrix module DIN    |
| VCC       | +5 V               | U2 V+, MOD VCC        | -         | 4.0–5.5 V; bypass each chip 0.1/10 µF |
| GND       | GND                | U2 GND, MOD GND       | -         | common ground                         |
| ISET      | U2, ISET           | +5 V via R_SET ≈10 kΩ | -         | sets 7-seg peak segment current       |

**U1** = ATmega328P, **U2** = score MAX7221, **MOD** = dot-matrix module.

### MCU ↔ pushbuttons

| Net    | From (RefDes, Pin) | To (RefDes, Pin) | MCU pin  | Notes                            |
| :----- | :----------------- | :--------------- | :------- | :------------------------------- |
| BTN_A  | U1, PD6 (D6)       | SW1, pin 1       | PD6 / D6 | SPIN/SELECT; other pin to GND    |
| BTN_B  | U1, PD7 (D7)       | SW2, pin 1       | PD7 / D7 | MENU/BACK, long-press = settings |

Both use the internal pull-up (idle HIGH, pressed LOW) — no external resistors.
Debounce + long-press in `src/drivers/buttons.c`.

### MCU ↔ buzzer

| Net       | From (RefDes, Pin) | To (RefDes, Pin) | MCU pin  | Notes                   |
| :-------- | :----------------- | :--------------- | :------- | :---------------------- |
| BUZZ      | U1, PB1 (OC1A/D9)  | BZ1, +           | PB1 / D9 | Timer1 CTC toggles OC1A |
| BUZZ_GND  | BZ1, −             | GND              | -        | passive buzzer / piezo  |

### MCU — PRNG seed

| Net   | From (RefDes, Pin) | To (RefDes, Pin) | MCU pin  | Notes                        |
| :---- | :----------------- | :--------------- | :------- | :--------------------------- |
| SEED  | U1, PC0 (ADC0/A0)  | *(none)*         | PC0 / A0 | **leave floating** for noise |

### Score chip ↔ 7-segment digits

| MAX7221 pin  | Connects to                                          |
| :----------- | :--------------------------------------------------- |
| SEG A..G, DP | a..g + dp anode rails, tied across all 8 digits      |
| DIG0..DIG7   | common cathode per digit (DIG0 = leftmost of Disp A) |
| V+           | +5 V (with 0.1 µF + 10 µF bypass close to the chip)  |
| ISET         | +5 V via R_SET ≈ 10 kΩ (sets peak segment current)   |
| GND          | ground (both GND pins)                               |

Wire **DIG0 → leftmost digit of Display A … DIG7 → rightmost of Display B** so
`seg7.c`'s buffer index maps 1:1 to physical digits.

### Dot-matrix module

The module is a 5-pin part (VCC, GND, DIN, CS, CLK) containing four cascaded
MAX7219s (one per 8×8 panel). It plugs straight onto the SPI bus — **no external
driver chip**. Panels 0/1/2 = left/center/right reels; panel 3 = payline accent.

## Wiring diff vs. the `stopwatch` project

The stopwatch already wired PB3/PB5/PB2 (SPI) and PD6/PD7 (buttons). **New for the
slot machine:**

| New net           | From                 | To                             | Notes                         |
| :---------------- | :------------------- | :----------------------------- | :---------------------------- |
| Chain cascade     | score DOUT           | dot-matrix module DIN          | extends the 1-chip chain to 5 |
| Matrix CLK/CS/PWR | shared CLK/CS/5V/GND | dot-matrix module 5-pin header | same shared bus lines         |
| Buzzer            | PB1 (D9)             | passive buzzer → GND           | Timer1 CTC toggles OC1A       |
| PRNG seed         | PC0 (A0)             | *(left floating)*              | do not connect anything       |

The buttons keep the stopwatch's PD6/PD7 pins (BTN_A/BTN_B here). Nothing on the
stopwatch side changes physically.

## Simulation (Wokwi)

`../diagram.json` stands in the two 4-digit 7-seg displays with a
`wokwi-max7219-matrix` (Wokwi has no MAX7219 7-seg part) — SPI wiring is identical,
so the firmware path matches; only device 0's visual differs. Buttons on D6/D7,
buzzer on D9, logic analyzer on the SPI + buzzer nets. See
`../wokwi-verify-artifacts/` for a run record.
