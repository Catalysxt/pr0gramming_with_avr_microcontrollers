# Pocket Casino

Bare-metal AVR-C firmware for an ATmega328P that turns a 16×2 HD44780 LCD,
two push-buttons, and a passive piezo buzzer into a four-game pocket casino.

---

## Hardware

### Components

| Component | Part |
|---|---|
| MCU | ATmega328P @ 16 MHz external crystal |
| Display | Vishay LCD-016N002B (16×2, HD44780-compatible, ST7066 controller) |
| Button A | Normally-open push-button (active-low) |
| Button B | Normally-open push-button (active-low) |
| Buzzer | Passive piezo (any 5V-compatible type) |

### Wiring

```
ATmega328P                      LCD (16×2)
──────────                      ──────────
PB0 (pin 14) ────────────────── RS  (pin 4)
PB1 (pin 15) ────────────────── R/W (pin 5)
PB2 (pin 16) ────────────────── E   (pin 6)
PD4 (pin  2) ────────────────── DB4 (pin 11)
PD5 (pin  3) ────────────────── DB5 (pin 12)
PD6 (pin  4) ────────────────── DB6 (pin 13)
PD7 (pin  5) ────────────────── DB7 (pin 14)
GND          ────────────────── VSS, R/W when not used for reads
+5V          ────────────────── VDD
Potentiometer────────────────── V0  (contrast)
GND          ────────────────── A (backlight -)
+5V via 33Ω  ────────────────── K (backlight +)

ATmega328P                      Buttons
──────────                      ───────
PC0 (pin 23) ── [BTN A] ── GND   (ACTION / SPIN / FLIP / ROLL / SELECT)
PC1 (pin 24) ── [BTN B] ── GND   (NEXT / BET / CHANGE)
(Internal pull-ups enabled in firmware — no external resistors needed)

ATmega328P                      Buzzer
──────────                      ──────
PB3 (pin 17) ────────────────── Piezo (+)
GND          ────────────────── Piezo (-)

Buzzer note: PB3 = OC2A (Timer2 Compare A output). OC1A (PB1) is occupied
by the LCD R/W line, so Timer2 is used instead of Timer1.
```

### Fuse Bytes (16 MHz external crystal)

| Fuse | Value | Effect |
|---|---|---|
| LFUSE | 0xF7 | External crystal full-swing, 16K CK + 65 ms startup, no CLKDIV8 |
| HFUSE | 0xD9 | BOOTSZ 2KB, SPI enabled, EESAVE on |
| EFUSE | 0x07 | BOD disabled |

**Program fuses once before first flash:**

```
make fuses
```

> ⚠️ Wrong fuse values can lock out the programmer. Double-check against the
> ATmega328P datasheet Section 28 before burning.

---

## Building and Flashing

**Prerequisites:** `avr-gcc`, `avr-binutils`, `avrdude` in your PATH.
Programmer: **USBasp** (default). Edit `PROGRAMMER_TYPE` in the Makefile to change.

```bash
# Build
make

# Check size
make size

# Flash (programmer must be connected)
make flash

# Flash EEPROM separately (not used in v1)
make flash_eeprom

# Generate disassembly listing for debugging
make disassemble

# Clean build artifacts
make clean
```

---

## How to Play

### Controls (all games)

| Button | Action |
|---|---|
| BTN_A | SELECT / ACTION / SPIN / FLIP / ROLL / HIGHER |
| BTN_B | NEXT / BET / CHANGE / LOWER |
| BTN_B (hold 800 ms) | Return to main menu from any game |

### Main Menu

Power on → splash screen → press any button → main menu.

- **BTN_B** cycles through the four games (Slots → Coin Flip → Hi or Lo → Dice).
- **BTN_A** enters the highlighted game.
- Credits are shown on the right of row 0 at all times.
- Start with **100 credits**. Reach 0 → Game Over screen; BTN_A restarts with 100.

---

### 1. Slots

Classic 3-reel slot machine. Symbols: Cherry, Bell, Seven, Coin, Dice.

**Controls:**
- **BTN_B** — cycle bet amount: 1 / 5 / 10 / 25 credits
- **BTN_A** — SPIN (deducts bet)

**Reel animation:** reels spin for 800 / 1100 / 1400 ms before stopping one by one.

**Payouts:**

| Result | Payout |
|---|---|
| Three Sevens | 50× bet (JACKPOT!) |
| Three-of-a-kind (any) | 10× bet |
| Two-of-a-kind | 2× bet |
| No match | 0 (lose bet) |

**Display:**
```
Row 0: [🍒] [🔔] [7]
Row 1: Bet:5  Cr:0095
```

---

### 2. Coin Flip

Simple heads-or-tails. Fixed 10-credit bet. Win pays 20 (net +10).

**Controls:**
- **BTN_B** — toggle your call: HEADS / TAILS
- **BTN_A** — FLIP

**Animation:** ~600 ms coin flip with rising-pitch ticks.

**Display:**
```
Row 0: Call: [coin] HEADS
Row 1: Bet:10   Cr:0095
```

---

### 3. Higher or Lower

Guess whether the next card is higher or lower than the current one.
Cards run A, 2–10, J, Q, K (values 1–13). **Equal = lose.**

**Controls:**
- **BTN_A** — guess HIGHER
- **BTN_B** — guess LOWER

**Scoring:** +10 credits per correct guess. Wrong guess resets streak.
Streak 10 → JACKPOT sound!

**Display:**
```
Row 0: Card: K  > or <?
Row 1: Streak:03  +30 Cr:0095
```

---

### 4. Dice (Yahtzee-lite)

Roll two dice and match your target. Fixed 5-credit bet.

**Controls:**
- **BTN_B** — cycle target: DOUBLES / OVER 7 / UNDER 7 / EXACT 7
- **BTN_A** — ROLL

**Payouts:**

| Target | Win Condition | Payout |
|---|---|---|
| DOUBLES | Both dice same | 5× bet |
| OVER 7 | Sum > 7 | 2× bet |
| UNDER 7 | Sum < 7 | 2× bet |
| EXACT 7 | Sum = 7 | 4× bet |

**Animation:** ~700 ms tumbling animation (CGRAM pip pattern updated in-place).

**Display:**
```
Row 0: [3] [5]  Sum:8
Row 1: Tgt:OVER 7  Cr:0095
```

---

## Project Structure

```
pocket_casino/
├── Makefile                  avr-gcc + avrdude build system
├── README.md                 this file
├── hardware_connections.h    all pin assignments (single source of truth)
├── docs/
│   └── state_machine.md      Mermaid FSM diagrams
└── src/
    ├── main.c                init + superloop
    ├── buttons.h / .c        PCINT1-driven debounce, long-press, event queue
    ├── buzzer.h  / .c        Timer2 CTC tone generator + PROGMEM SFX
    ├── rng.h     / .c        16-bit xorshift PRNG, floating-ADC seed
    ├── ui.h      / .c        menu FSM, CGRAM loader, draw helpers
    └── games/
        ├── slots.h      / .c
        ├── coinflip.h   / .c
        ├── higherlower.h/ .c
        └── dice.h       / .c
```

External LCD driver (not modified):
```
../extern_libraries/lcd_driver/
    lcd_driver.h   HD44780/ST7066 4-bit driver API
    lcd_driver.c   implementation
```

---

## Stretch Goals (v2 ideas)

- **EEPROM high scores** — persist top score per game across power cycles
  (`<avr/eeprom.h>`, `eeprom_update_word`)
- **Rotary encoder** — replace BTN_B cycle with smooth rotary navigation
- **Backlight PWM dimming** — Timer0 Fast PWM on OC0A/PD6 for brightness control
- **More games** — Blackjack, Roulette (needs more buttons or encoder)
- **Sound polish** — distinct per-game background melodies using Timer1 ISR
- **Animation improvements** — smooth scroll between menu items
