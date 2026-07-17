# Hardware Connections

Copied from `docs/prompt.md` — the authoritative source. If these ever
disagree, `prompt.md` wins and this file is stale.

## Pin assignment

| Function | ATmega328P Pin | Direction | Config | Notes |
| --- | --- | --- | --- | --- |
| LCD D4 | PD4 | OUT | Set by `Lcd_Init` |  |
| LCD D5 | PD5 | OUT | Set by `Lcd_Init` |  |
| LCD D6 | PD6 | OUT | Set by `Lcd_Init` |  |
| LCD D7 | PD7 | OUT | Set by `Lcd_Init` |  |
| LCD RS | PB0 | OUT | Set by `Lcd_Init` | Command/data select |
| LCD RW | PB1 | OUT | Set by `Lcd_Init` |  |
| LCD EN | PB2 | OUT | Set by `Lcd_Init` | Latch strobe |
| Buzzer | **PB3 / OC2A** | OUT | Timer2 CTC, toggle OC2A | Passive piezo |
| Green LED | **PB4** | OUT | Push-pull | Through 330 Ω to GND |
| Red LED | **PB5** | OUT | Push-pull | Through 330 Ω to GND |
| Keypad COL0 | **PC0** | IN | Internal pull-up |  |
| Keypad COL1 | **PC1** | IN | Internal pull-up |  |
| Keypad COL2 | **PC2** | IN | Internal pull-up |  |
| Keypad COL3 | **PC3** | IN | Internal pull-up |  |
| Keypad ROW0 | **PC4** | OUT | High by default, ground one at a time during scan |  |
| Keypad ROW1 | **PC5** | OUT | High by default |  |
| Keypad ROW2 | **PD2** | OUT | High by default |  |
| Keypad ROW3 | **PD3** | OUT | High by default |  |
| VCC | VCC + AVCC | — | 100 nF decoupling each | AVCC tied to VCC (10 Ω ferrite optional) |
| AREF | AREF | — | 100 nF to GND | Not used by code but cap recommended |
| XTAL1/XTAL2 | PB6/PB7 | — | 16 MHz + 2×22 pF | External clock; fuses must be set |
| RESET | PC6 | IN | 10 kΩ to VCC + ISP cap | **Do not repurpose** |

Reserved free pins for stretch work: PD0 (RXD), PD1 (TXD). PC7 does not
exist on the DIP-28 package.

## Schematic wire list

| Net | From | To | Component / Value | Notes |
| --- | --- | --- | --- | --- |
| VCC | +5 V supply | ATmega328P VCC (pin 7 DIP) | — | 100 nF decoupling |
| VCC | +5 V supply | ATmega328P AVCC (pin 20 DIP) | 10 Ω ferrite (opt.) | 100 nF decoupling |
| VCC | +5 V supply | LCD VDD (pin 2) | — | 100 nF decoupling |
| VCC | +5 V supply | LCD BLA (pin 15) | — | Backlight anode |
| VCC | +5 V supply | 10 kΩ pot terminal 1 | 10 kΩ pot | Contrast |
| GND | Ground rail | ATmega328P GND (pins 8 & 22) | — |  |
| GND | Ground rail | LCD VSS (pin 1) | — |  |
| GND | Ground rail | LCD R/W (pin 5) | — or driven by PB1 | Match what `lcd_driver` expects |
| GND | Ground rail | LCD BLK (pin 16) | — | Optional 220 Ω series for backlight dim |
| GND | Ground rail | Pot terminal 3 | — |  |
| VEE | Pot wiper | LCD VO (pin 3) | — | Adjust pot until chars visible |
| AREF | ATmega328P AREF | GND | 100 nF |  |
| RESET | ATmega328P RST (PC6) | VCC | 10 kΩ pull-up + 100 nF + ISP header | Standard reset network |
| XTAL1 | ATmega328P PB6 | 16 MHz crystal pin 1 | 22 pF to GND |  |
| XTAL2 | ATmega328P PB7 | 16 MHz crystal pin 2 | 22 pF to GND |  |
| LCD_RS | PB0 | LCD RS (pin 4) | — |  |
| LCD_RW | PB1 | LCD R/W (pin 5) | — | Or tie LCD R/W to GND |
| LCD_EN | PB2 | LCD EN (pin 6) | — |  |
| LCD_D4 | PD4 | LCD DB4 (pin 11) | — |  |
| LCD_D5 | PD5 | LCD DB5 (pin 12) | — |  |
| LCD_D6 | PD6 | LCD DB6 (pin 13) | — |  |
| LCD_D7 | PD7 | LCD DB7 (pin 14) | — |  |
| BUZZER | PB3 (OC2A) | Piezo (+) | — | Optional 100 Ω series |
| BUZZER | Piezo (−) | GND | — |  |
| LED_GREEN | PB4 | Green LED anode | — |  |
| LED_GREEN | Green LED cathode | GND | 330 Ω |  |
| LED_RED | PB5 | Red LED anode | — |  |
| LED_RED | Red LED cathode | GND | 330 Ω |  |
| KEY_COL0 | PC0 | Keypad COL0 pin | — | Internal pull-up |
| KEY_COL1 | PC1 | Keypad COL1 pin | — | Internal pull-up |
| KEY_COL2 | PC2 | Keypad COL2 pin | — | Internal pull-up |
| KEY_COL3 | PC3 | Keypad COL3 pin | — | Internal pull-up |
| KEY_ROW0 | PC4 | Keypad ROW0 pin | — | Output, idle high |
| KEY_ROW1 | PC5 | Keypad ROW1 pin | — | Output, idle high |
| KEY_ROW2 | PD2 | Keypad ROW2 pin | — | Output, idle high |
| KEY_ROW3 | PD3 | Keypad ROW3 pin | — | Output, idle high |

## ASCII block diagram

```
       +5V ──┬─[100nF]── GND        16 MHz xtal
             │
        ┌────┴─────────────────────┐
        │       ATmega328P         │
        │                          │
LCD D4 ─┤ PD4            PB0 ├──── LCD RS
LCD D5 ─┤ PD5            PB1 ├──── LCD R/W
LCD D6 ─┤ PD6            PB2 ├──── LCD EN
LCD D7 ─┤ PD7            PB3 ├──── Piezo (+)
        │                PB4 ├──── Green LED
ROW2 ───┤ PD2            PB5 ├──── Red LED
ROW3 ───┤ PD3                │
        │                PC0 ├──── KEY COL0
        │                PC1 ├──── KEY COL1
        │                PC2 ├──── KEY COL2
        │                PC3 ├──── KEY COL3
ROW0 ───┤ PC4            PC6 ├──── RESET (10k + ISP)
ROW1 ───┤ PC5                │
        │     PB6/PB7 ── 16 MHz xtal + 2×22pF
        └─────────────────────────┘
        GND ─── star-ground
```

## Keypad pinout discovery procedure

Use a multimeter in continuity mode to identify which of the module's 8
pins are rows and which are columns. The 8 pins are usually labelled
`R1–R4, C1–C4` in a horizontal strip on the back; press a key and probe
the two pins that go continuous — that's that key's (row, col).
