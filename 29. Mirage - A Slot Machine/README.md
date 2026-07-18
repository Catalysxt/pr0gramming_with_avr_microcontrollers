# Mirage - A Slot Machine

A self-contained casino slot machine in bare-metal AVR C. Three reels scroll on a
4-panel 8×8 dot-matrix, credits and last-win show on two 4-digit 7-segment
displays, and a buzzer plays casino SFX — all driven by one ATmega328P over a
single SPI daisy chain. Two buttons run the whole UI, including a settings menu
with a satisfying accelerating cash-out.

## Overview

What makes it interesting:

- **One SPI bus, everything on it.** A score MAX7221 and a 4-panel dot-matrix
  module (four internal MAX7219s) form a 5-device daisy chain sharing MOSI/SCK and
  a single /LOAD. Both 7-seg digits and matrix rows map onto the chips' eight
  `DIGIT` registers, so one refresh loop feeds the whole display.
- **Non-blocking throughout.** Timer0 is a 1 ms tick; Timer1 hardware-generates
  tones by toggling OC1A. No `_delay_ms` in the loop or ISRs — reel easing,
  count-ups, debounce, and the melody engine are all `timing_ms()`-paced.
- **Real randomness.** A floating-ADC read at boot is XORed with `TCNT1` captured
  at your first spin, seeding a xorshift32 PRNG — the sequence depends on your
  timing.

## Hardware

- ATmega328P @ 16 MHz (external crystal), USBasp.
- 1× MAX7221 + two 4-digit 7-segment displays (Credits + Last Win).
- 1× 4-panel 8×8 dot-matrix module (5-pin SPI header; contains 4 MAX7219s).
- 2× pushbuttons, 1× passive buzzer.

Full pin table, daisy-chain diagram, connection tables, and the wiring diff vs.
the stopwatch are in [`docs/hardware_connections.md`](docs/hardware_connections.md).

## Controls

| Button | Short press                         | Long press (~1 s)     |
| :----- | :---------------------------------- | :-------------------- |
| BTN_A  | SPIN / SELECT / confirm             | —                     |
| BTN_B  | MENU: next item / wake from attract | enter **SETTINGS**    |

## States

```
        ┌──────────────────────────── BTN_B long ───────────────────────────┐
        v                                                                    │
   ┌──────────┐  BTN_A/BTN_B   ┌────────┐  BTN_A (cost ok)   ┌──────────┐     │
   │ ATTRACT  │ ─────────────► │ READY  │ ─────────────────► │ SPINNING │     │
   │ banner + │ ◄───idle 12s── │ idle   │ ◄──┐               │ 3 reels  │     │
   │ symbols  │                └────────┘    │               │ ease+stop│     │
   └──────────┘                   │  ▲       │ no win        └────┬─────┘     │
        ▲  BTN_B long             │  │       │                    │ all stop  │
        │                         │  │  ┌──────────┐  win    ┌──────────┐     │
        └── BTN_B long ───────────┘  └──│ EVALUATE │◄────────│ compute  │     │
                                        │ count-up │  payout └──────────┘     │
   ┌──────────┐  Exit / done            └────┬─────┘                          │
   │ SETTINGS │◄───────────────────────┐     │ jackpot (3× 7)                 │
   │ play cnt │  BTN_A on Exit         │     ▼                                │
   │ cash out │────────────────────────┘  ┌──────────┐  jingle+flash done     │
   │ exit     │                           │ JACKPOT  │ ──────────────────────►┘
   └──────────┘                           └──────────┘  (blocks input)
```

- **ATTRACT** — scrolling "SLOTS" banner + cycling symbol parade; occasional jingle.
- **READY** — idle reels; waiting for a spin. Falls back to ATTRACT after 12 s.
- **SPINNING** — reels scroll fast → ease → snap; each stops on a staggered timer
  with a "thunk"; spin-loop clicks slow with them.
- **EVALUATE** — payout computed, balance updated, Last-Win counts up with an
  ascending arpeggio.
- **JACKPOT** — 3× 7: dot-matrix flashes and a ~1.7 s jingle plays (input locked).
- **SETTINGS** — Play count (spins since power-up, RAM only) · Cash out
  (drains balance to 0 with accelerating coin ticks; no exit mid-cash-out) · Exit.

Every transition passes through a `state_transition(from, to)` seam in `game.c`
(a no-op today, reserved for UART debug).

## Payout table

Cost per spin: **1 credit**. Starting balance: **100**. (Tune in `src/app/game.c`.)

| Combination            | Payout                          |
| :--------------------- | :------------------------------ |
| 3× Seven **(JACKPOT)** | 777                             |
| 3× Diamond             | 400                             |
| 3× Star                | 240                             |
| 3× Bar                 | 150                             |
| 3× Bell                | 100                             |
| 3× Melon               | 70                              |
| 3× Lemon               | 50                              |
| 3× Cherry              | 40                              |
| Any pair               | ⌊(that symbol's 3-kind) ÷ 8⌋, min 2 |
| Any single cherry      | 5                               |

Reel odds are weighted so Seven/Diamond are rare (see `REEL_WEIGHT` in `game.c`).

## Build & Flash

```sh
make            # build build/slot_machine.hex (+ prints avr-size)
make size       # size report only
make flash      # flash via USBasp
make clean      # remove build/
```

Different programmer:
```sh
make flash PROGRAMMER_TYPE=arduino PROGRAMMER_ARGS="-P COM3 -b 115200"
```
Set fuses once for the external 16 MHz crystal: `make fuses`.

## How to use

1. Power on → **ATTRACT**: the "SLOTS" banner scrolls and symbols parade; the
   score display shows your balance (100) and 0 last-win.
2. Tap **BTN_A** → **READY** (idle reels).
3. Tap **BTN_A** → the three reels spin, ease, and snap to a stop one by one.
4. If they match, Last-Win counts up with an arpeggio; 3× 7 flashes the matrix
   and plays the jackpot jingle.
5. Hold **BTN_B** (~1 s) → **SETTINGS**. Tap **BTN_B** to move between *PLAY*
   (play count), *CASH* (cash out), *EXIT*. Tap **BTN_A** to select.
6. On **CASH**, BTN_A drains your balance to 0 with coin ticks that speed up then
   slow for a big finish (you can't leave until it's done). Pick **EXIT** to return.

## Practical checklist

1. Flash the firmware; power the board from a supply that can source the LED
   current (a full 8×8 panel is bright).
2. On boot the dot-matrix scrolls **SLOTS**; the left 7-seg shows `100`, the right
   shows `0`. If digits look scrambled, check DIG0→leftmost wiring.
3. Tap **BTN_A** once — the reels stop scrolling and show three idle symbols.
4. Tap **BTN_A** again — all three reels spin, then stop left-to-right with a
   click/thunk from the buzzer.
5. On a matching stop, the right display counts up to the win and the balance
   (left display) increases. On 3× 7 the whole matrix flashes.
6. With few/zero credits, tap **BTN_A**: you get a descending "raspberry" buzz and
   the reels shake — no spin.
7. Hold **BTN_B** ~1 s: the display shows `PLAY` + a spin count. Tap **BTN_B** to
   reach `CASH`, tap **BTN_A**: credits drain to 0 with accelerating coin ticks.
8. Reach `EXIT`, tap **BTN_A**: back to the game.

## Memory footprint

Latest `avr-size` (build/slot_machine.elf):

| Section         | Bytes | Budget         |
| :-------------- | ----: | :------------- |
| `.text` (flash) | 7412  | 32 768         |
| `.data` (flash+SRAM) | 82 |               |
| `.bss` (SRAM)   | 182   |                |
| **Flash total** | **7494** | 23% of 32 KB |
| **SRAM total**  | **264**  | 13% of 2 KB  |

## Stretch goals

- Persist a high-score / lifetime credits to EEPROM (wear-aware `eeprom_update`).
- Real 5×7 dot-matrix font so the banner and menu labels can show any text.
- A "hold/nudge" mechanic; a bet-per-line selector; a second payline.
- Wire the `state_transition()` seam to UART for a live FSM trace.
- Attract-mode auto-demo that fake-spins to draw a crowd.

## Docs

- [`docs/hardware_connections.md`](docs/hardware_connections.md) — pin table,
  daisy-chain diagram, connection tables, wiring diff.
- [`docs/Theory.md`](docs/Theory.md) — the electronics/theory behind each block.
- [`docs/CHANGELOG.md`](docs/CHANGELOG.md) — change history.
