# Ember — Pomodoro Desk-Lamp Controller

## Overview

Ember is a standalone AVR box that makes ignoring your Pomodoro break *hard*.
You dial in work and break minutes on a rotary encoder, a 4-digit 7-segment
display counts down MM:SS, and when the work block hits **0:00 a relay switches
a real desk lamp ON** — an unmissable physical cue to step away. When the break
ends the lamp goes dark and the box returns to a µA-idle sleep until you touch
it again. What's interesting: bounce-immune quadrature decoding on pin-change
interrupts, a Timer2-multiplexed common-cathode display, `SLEEP_MODE_PWR_DOWN`
with encoder wake, and EEPROM-persisted presets — all bare-metal, no Arduino.

## Hardware

- ATmega328P @ 16 MHz crystal, USBasp programmer
- Kingbright **CC56-12SRWA** 4-digit common-cathode 7-segment display
- **KY-040** rotary encoder with push-button
- Passive piezo buzzer
- Relay module + desk lamp (240 V)
- 4× NPN transistors (2N3904/BC337) as low-side switches for the digit commons,
  8× 220 Ω segment resistors, 4× 1 kΩ base resistors

Full pin map and ⚠ mains-safety notes: [docs/hardware_connections.md](docs/hardware_connections.md).

## Theory

Quadrature Gray-code decoding, display multiplexing & persistence of vision,
the per-pin current trap (why the NPNs), Timer1 tone generation, relay/mains
safety, and the power-down-vs-async-Timer2 sleep decision:
[docs/Theory.md](docs/Theory.md).

## Build & Flash

```sh
make            # build build/pom_timer.hex
make size       # flash/SRAM footprint
make flash      # program via USBasp
make fuses      # 16 MHz external crystal fuses (LFUSE=0xF7 HFUSE=0xD9 EFUSE=0x07)
make clean
```

Pre-flash simulation (Wokwi): `wokwi.toml` + `diagram.json` are included; see
`wokwi-verify-artifacts/` for the latest run.

## How to use

1. **Idle** — display is dark, box asleep. Turn the encoder to wake it.
2. **Set work** — display shows the work minutes (default 25). Rotate to
   adjust (1–99). **Tap** the encoder button to confirm (rising chime).
3. **Set break** — display shows break minutes (default 5). Rotate to adjust
   (1–60). **Tap** to save presets and start.
4. **Work** — MM:SS counts down with a blinking colon, lamp OFF.
5. **0:00** — lamp clicks **ON**, a bright chime sounds, session counter +1,
   and the break countdown begins.
6. **Break** — MM:SS counts down, lamp stays ON.
7. **0:00** — falling chime, lamp OFF, back to idle sleep.

At any time during work/break, **hold** the button (~2 s) to abort back to idle.
Presets and the session count survive power cycles (EEPROM).

## Practical checklist

1. `make flash`. Power the board; display is dark (idle sleep).
2. Turn the encoder one detent CW → display lights and shows the work minutes.
   Each detent should change the number by exactly **±1** (no skips/jumps).
3. Rotate to `1`. Confirm it clamps at 1 and won't go lower.
4. Rotate back to `1` minute for a fast test. **Tap** to confirm → a short
   rising chime, display switches to the break value.
5. Rotate break to `1`. **Tap** → confirm chime, display shows `01:00` and
   starts counting down; the colon blinks once per second.
6. Watch it reach `00:00`: the **lamp switches ON**, a bright chime plays.
   Verify the lamp physically lights.
7. The break now counts down from `01:00`; the lamp stays ON.
8. At `00:00`: a falling chime, the **lamp switches OFF**, display goes dark.
9. Power-cycle the board, wake it, and confirm your `1`/`1` presets were
   remembered (EEPROM).
10. Start a session and **hold** the button ~2 s → it aborts to idle, lamp OFF.
11. (Optional) Measure supply current while idle — expect µA-range in
    power-down.

## Memory footprint

`avr-gcc -Os`, latest build:

| Section        | Bytes | Budget        |
| :------------- | :---- | :------------ |
| `.text` (flash)| 4340  | 32 KB         |
| `.data`        | 144   |               |
| `.bss`         | 77    |               |
| **Flash used** | 4484  | **13.7%**     |
| **SRAM used**  | 221   | **10.8%** of 2 KB |

Well under the 75% warn thresholds.

## Stretch goals

- **UART session dashboard** — stream session count + timestamps to a Python
  dashboard (PD0/PD1 reserved).
- **Long break every 4 sessions** — use the persisted session counter.
- **SSR instead of a mechanical relay** — silent switching; compare lifetimes.
- **74HC595 shift register** — reclaim display pins (frees ~10 GPIO).
- **Tune the payoff** — `on_work_complete()` in `src/ember_fsm.c` is the one
  spot with real policy latitude (count the session at work-end vs break-end;
  one chirp vs a longer alarm).
