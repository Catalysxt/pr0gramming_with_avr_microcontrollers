# Hardware Connections — Ember

**MCU:** ATmega328P @ 16 MHz external crystal. **Programmer:** USBasp.
All `pin_t` definitions live in `src/hardware_connections.h` (single source of
truth). PB6/PB7 are the crystal (XTAL1/2) and unavailable; PD0/PD1 are reserved
for the UART dashboard stretch goal.

## Connection table

| Net          | From (RefDes, Pin)   | To (RefDes, Pin)      | MCU pin | Notes |
| :----------- | :------------------- | :-------------------- | :------ | :---- |
| RELAY_IN     | U1, PB0              | RLY1, IN              | PB0     | active-HIGH module (own coil driver + flyback) |
| BUZZER       | U1, PB1 (OC1A)       | BZ1, +                | PB1     | passive piezo; other leg → GND |
| SEG_A        | U1, PC0              | DISP, a  (pin 11)     | PC0     | via 220 Ω series resistor; active-HIGH = lit |
| SEG_B        | U1, PC1              | DISP, b  (pin 7)      | PC1     | via 220 Ω |
| SEG_C        | U1, PC2              | DISP, c  (pin 4)      | PC2     | via 220 Ω |
| SEG_D        | U1, PC3              | DISP, d  (pin 2)      | PC3     | via 220 Ω |
| SEG_E        | U1, PC4              | DISP, e  (pin 1)      | PC4     | via 220 Ω |
| SEG_F        | U1, PC5              | DISP, f  (pin 10)     | PC5     | via 220 Ω |
| SEG_G        | U1, PD5              | DISP, g  (pin 5)      | PD5     | via 220 Ω |
| SEG_DP       | U1, PD6              | DISP, dp (pin 3)      | PD6     | via 220 Ω; used as MM:SS colon on digit 2 |
| DIG1         | U1, PB2              | Q1 base (→ DISP pin12)| PB2     | NPN low-side, active-HIGH; base resistor 1 kΩ |
| DIG2         | U1, PB3              | Q2 base (→ DISP pin 9)| PB3     | NPN low-side, active-HIGH |
| DIG3         | U1, PB4              | Q3 base (→ DISP pin 8)| PB4     | NPN low-side, active-HIGH |
| DIG4         | U1, PB5              | Q4 base (→ DISP pin 6)| PB5     | NPN low-side, active-HIGH |
| ENC_CLK      | U1, PD2 (PCINT18)    | ENC1, CLK             | PD2     | internal pull-up + board 10 k |
| ENC_DT       | U1, PD3 (PCINT19)    | ENC1, DT              | PD3     | internal pull-up + board 10 k |
| ENC_SW       | U1, PD4 (PCINT20)    | ENC1, SW              | PD4     | internal pull-up; active-LOW |
| ENC_VCC      | ENC1, +              | +5 V                  | -       | - |
| ENC_GND      | ENC1, GND            | GND                   | -       | - |
| LAMP_LIVE    | Mains L              | RLY1, COM             | -       | ⚠ 240 V — mains side only |
| LAMP_SW      | RLY1, NO             | Lamp live             | -       | ⚠ NO: lamp off until energised; never GND→COM |

- **DISP** = Kingbright CC56-12SRWA (common **cathode**). Digit common pins:
  Dig1=12, Dig2=9, Dig3=8, Dig4=6. Each NPN emitter → GND, collector → digit
  common pin, base → MCU digit pin through 1 kΩ. A digit lights when its NPN is
  driven **HIGH** (common sunk to GND) while the wanted segments are driven HIGH.
- **Q1–Q4** = NPN low-side switch (2N3904 / BC337; **not** BC547 — its 100 mA
  Ic limit is below the ~100 mA an all-segments "8" draws). See docs/Theory.md §2
  for the per-pin current reasoning.

## ASCII wiring sketch

```
                 +5V ──────────────────────────────── ENC1(+)
   ATmega328P
  ┌──────────┐   1kΩ
  │ PB2..PB5 ├──/\/\──▶ Q base (active-HIGH select)
  │          │              [Q1..Q4 NPN]
  │          │        collectors → DISP COM(cathode) 12/9/8/6
  │          │        emitters   → GND
  │ PC0..PC5 ├──220Ω──▶ DISP a..f  (anodes, active-HIGH = lit)
  │ PD5,PD6  ├──220Ω──▶ DISP g,dp
  │          │
  │ PD2 ◀────┼──────────  ENC1 CLK   (PCINT18)
  │ PD3 ◀────┼──────────  ENC1 DT    (PCINT19)
  │ PD4 ◀────┼──────────  ENC1 SW    (PCINT20, active-LOW)
  │          │
  │ PB1(OC1A)├──────────▶ BZ1(+) ── piezo ── GND
  │          │
  │ PB0      ├──────────▶ RLY1 IN ──(module)── COM/NO ── ⚠ 240V lamp
  └──────────┘
        GND ── DISP commons reach GND only through Q1..Q4 (one digit at a time);
               NPN emitters, ENC GND, BZ−, module GND → GND
```

⚠ **Mains:** the relay load side (COM/NO ↔ 240 V lamp live) is lethal and must
stay physically isolated from all 5 V wiring. Break the **live** conductor via
COM→NO; never route GND to COM. See docs/Theory.md §4.
