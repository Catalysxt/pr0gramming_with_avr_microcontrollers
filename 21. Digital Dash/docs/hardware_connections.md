# Hardware Connections — digital_dash

## Connection Table

| Net             | From (RefDes, Pin)          | To (RefDes, Pin)            | MCU pin       | Notes                                         |
| :-------------- | :-------------------------- | :-------------------------- | :------------ | :-------------------------------------------- |
| VCC             | +5 V supply                 | U1, pin 7 (VCC)             | VCC           | 100 nF decoupling cap to GND                  |
| VCC             | +5 V supply                 | U1, pin 20 (AVCC)           | AVCC          | 100 nF decoupling to GND; optional 10 Ω series |
| VCC             | +5 V supply                 | LCD1, pin 2 (VDD)           | —             | 100 nF decoupling                             |
| VCC             | +5 V supply                 | LCD1, pin 15 (BLA)          | —             | Backlight anode                               |
| VCC             | +5 V supply                 | RV1, terminal 1             | —             | Contrast pot high side                        |
| GND             | Ground rail                 | U1, pin 8 (GND)             | GND           | Star-ground point                             |
| GND             | Ground rail                 | U1, pin 22 (GND)            | GND           | —                                             |
| GND             | Ground rail                 | LCD1, pin 1 (VSS)           | —             | —                                             |
| GND             | Ground rail                 | LCD1, pin 16 (BLK)          | —             | Backlight cathode (optional 220 Ω series)     |
| GND             | Ground rail                 | RV1, terminal 3             | —             | Contrast pot low side                         |
| VEE (contrast)  | RV1, wiper (terminal 2)     | LCD1, pin 3 (VO)            | —             | Adjust until characters visible               |
| AREF            | U1, pin 21 (AREF)           | GND via 100 nF              | AREF          | Decoupling for internal AVCC reference        |
| RESET           | U1, pin 1 (PC6/RST)         | VCC via 10 kΩ               | PC6/RST       | Standard ISP reset pull-up                    |
| RESET           | U1, pin 1 (PC6/RST)         | ISP connector / reset button | —            | 100 nF series for ESD                         |
| XTAL1           | U1, pin 9 (PB6/XTAL1)       | Y1, pin 1                   | PB6           | 22 pF to GND                                 |
| XTAL2           | U1, pin 10 (PB7/XTAL2)      | Y1, pin 2                   | PB7           | 22 pF to GND                                 |
| LCD_RS          | U1, PB0                     | LCD1, pin 4 (RS)            | PB0 / D8      | Command / data select                         |
| LCD_RW          | U1, PB1                     | LCD1, pin 5 (R/W)           | PB1 / D9      | Driven by lcd_driver; or tie to GND           |
| LCD_EN          | U1, PB2                     | LCD1, pin 6 (E)             | PB2 / D10     | Latch strobe                                  |
| LCD_D4          | U1, PD4                     | LCD1, pin 11 (DB4)          | PD4 / D4      | 4-bit data bus, bit 0                         |
| LCD_D5          | U1, PD5                     | LCD1, pin 12 (DB5)          | PD5 / D5      | —                                             |
| LCD_D6          | U1, PD6                     | LCD1, pin 13 (DB6)          | PD6 / D6      | —                                             |
| LCD_D7          | U1, PD7                     | LCD1, pin 14 (DB7)          | PD7 / D7      | —                                             |
| JUMP_BTN        | U1, PC0 / PCINT8            | SW1, terminal A             | PC0 / A0      | Internal pull-up; active-low                  |
| JUMP_BTN        | SW1, terminal B             | GND                         | —             | Button shorts to GND on press                 |
| DUCK_BTN        | U1, PC1 / PCINT9            | SW2, terminal A             | PC1 / A1      | Internal pull-up; active-low                  |
| DUCK_BTN        | SW2, terminal B             | GND                         | —             | Button shorts to GND on press                 |
| BUZZER          | U1, PB3 / OC2A              | BZ1, + (anode)              | PB3 / D11     | Passive piezo; optional 100 Ω series resistor |
| BUZZER          | BZ1, − (cathode)            | GND                         | —             | —                                             |
| RNG_SEED        | U1, PC2 / ADC2              | (unconnected)               | PC2 / A2      | Floating — do NOT connect; provides noise seed |

**Reference designators**: U1 = ATmega328P, LCD1 = 16×2 HD44780 LCD, RV1 = 10 kΩ contrast pot,
Y1 = 16 MHz crystal, SW1 = JUMP pushbutton, SW2 = DUCK pushbutton, BZ1 = passive piezo.

---

## ASCII Block Diagram

```
       +5V ──┬──[100nF]── GND        16 MHz crystal (Y1)
             │                       ┌──┤├──┐
        ┌────┴────────────────────────────────┐
        │             ATmega328P (U1)          │
        │                                      │
LCD D4 ─┤ PD4                       PB0 ├──── LCD RS
LCD D5 ─┤ PD5                       PB1 ├──── LCD R/W
LCD D6 ─┤ PD6                       PB2 ├──── LCD EN
LCD D7 ─┤ PD7                       PB3 ├──── Piezo BZ1 (+)
        │                                │
 SW1 ───┤ PC0/PCINT8                     │
 SW2 ───┤ PC1/PCINT9                     │
        │            PC2 ────────────────┤── (floating — RNG seed, do not connect)
        │            AVCC/AREF ──────────┤── decoupled to GND
        │     PB6/PB7 ──── Y1 + 2×22pF  │
        │     PC6/RST ──── 10k pull-up   │
        └──────────────────────────────────┘
GND ── star-ground: LCD VSS, U1 GND, pot GND, SW1/SW2 return, Piezo −
```

---

## Bill of Materials

| Qty | RefDes | Description                         |
| --- | ------ | ----------------------------------- |
| 1   | U1     | ATmega328P-PU (DIP-28)              |
| 1   | LCD1   | 16×2 character LCD (HD44780 compatible) |
| 1   | Y1     | 16 MHz crystal, HC-49/S             |
| 2   | —      | 22 pF ceramic capacitor (crystal load) |
| 1   | RV1    | 10 kΩ trimmer pot (contrast)        |
| 1   | BZ1    | Passive piezo buzzer, 5 V           |
| 2   | SW1,SW2 | Tactile pushbutton (SPST-NO)       |
| 1   | —      | 10 kΩ resistor (RESET pull-up)      |
| 4+  | —      | 100 nF ceramic decoupling capacitors |
| 1   | —      | USBasp programmer for flashing      |
