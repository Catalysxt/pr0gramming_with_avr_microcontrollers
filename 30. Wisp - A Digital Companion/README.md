# Digital Companion

An expressive **8×8 dot-matrix face** on an ATmega328P (bare-metal C) whose
emotions are *caused* by what it senses, not played back on a schedule. A hand
approaching makes it **curious**, then **nervous** if you get too close; petting
its button makes it **happy**; leaving it alone lets it get **bored/sad** and
eventually **sleepy**; a sudden lunge **surprises** it. Between the raw sensors
and the pixels sits a real **mood model** (valence/arousal) and a hysteresis
**state machine**, and every change **tweens** — expressions never snap-cut.

## Overview

- **MAX7221** drives the 8×8 matrix over SPI (no-decode mode, raw row bytes).
- **HC-SR04** ultrasonic ranger read via **Timer1 input capture** (no busy loop).
- **Pet button** on a pin-change pin (also seeds the PRNG from your timing).
- Fully **non-blocking**: a Timer0 1 ms tick paces a 25 Hz animation tick and a
  75 Hz display refresh; no `_delay_ms` in the loop or ISRs.
- Idle micro-motion (blink, saccade, breathing) keeps a resting face alive.

## Hardware

Bill of materials: ATmega328P (16 MHz crystal + 2×22 pF), MAX7221 + 8×8 LED
matrix module, HC-SR04, a momentary pushbutton, decoupling caps. Optional:
electret mic module (Tier-3 scaffold). Full pin table, SPI/ICP1 wiring diagrams,
and the **wiring diff vs. the stopwatch project** are in
[`HARDWARE.md`](HARDWARE.md) and [`docs/hardware_connections.md`](docs/hardware_connections.md).

## Emotion model

Affect is a point in `(valence, arousal)` space, each axis `int8_t` in
`[-64, +64]`. Sensors nudge it; every 100 ms it decays one unit per axis toward
`(0, 0)`. The FSM maps the point to an expression with a **±8 hysteresis** margin.

| Expression | Region (before ±8 hysteresis)        | Caused by                     |
| :--------- | :----------------------------------- | :---------------------------- |
| SURPRISED  | `a ≥ +48` (abrupt, overrides all)    | sudden proximity spike        |
| SLEEPY     | `a ≤ −32`                            | sustained inactivity          |
| NERVOUS    | `v ≤ −12 && a ≥ +20`                 | object too close (<10 cm)     |
| CURIOUS    | `v ≥ 0 && a ≥ +20`                   | object approaching            |
| HAPPY      | `v ≥ +24 && −8 ≤ a < +40`            | petting                       |
| CONTENT    | `v ≥ +8 && a ≤ −8`                   | calm + pleasant               |
| HESITANT   | `\|v\| < 12 && +8 ≤ a < +20`         | ambiguous — the bridge state  |
| SAD        | `v ≤ −12 && −32 < a < +8`            | boredom                       |
| NEUTRAL    | resting deadzone near the origin     | at rest                       |

Some transitions are legal **and abrupt** (any state → SURPRISED, including
SLEEPY → SURPRISED). Others must **bridge**: e.g. HAPPY → NERVOUS is illegal
directly and routes through HESITANT. The full graph is generated from the
transition table:

![FSM diagram](docs/fsm.svg)

Regenerate it with `make fsm-diagram` (uses Graphviz if installed, otherwise a
built-in stdlib layout).

## Controls

- **Pet button** — one press bumps valence (a "pet"); **3 presses within 2 s**
  forces a strong HAPPY the mood decay holds for a couple of seconds.
- **Hand / object in front of the HC-SR04** — approach → CURIOUS; closer than
  10 cm → NERVOUS; a sudden lunge → SURPRISED.
- **Do nothing** — after ~30 s it drifts SAD, after ~60 s it falls SLEEPY.

## Build & Flash

```sh
make               # build -> build/digital_companion.hex, prints size report
make size          # size report only
make flash         # flash via USBasp (override: make flash PROGRAMMER_TYPE=... PROGRAMMER_ARGS="...")
make fuses         # program fuses for a 16 MHz external crystal
make host-test     # run host-side unit tests (native gcc)
make fsm-diagram   # regenerate docs/fsm.dot + docs/fsm.svg
make clean         # remove build/
```

Optional build flags: `-DDEBUG_UART` (log FSM transitions on PD1 @ your BAUD),
`-DENABLE_AUDIO` (compile the Tier-3 mic seam).

Re-author expressions from the ASCII art after editing `assets_src/*.txt`:

```sh
python3 tools/png_to_progmem.py --input assets_src/ --output src/assets/expressions.h
```

## How to use (runtime walkthrough)

1. Power on: the face fades in to **NEUTRAL** and starts blinking occasionally.
2. Wave your hand toward the sensor from ~40 cm: it turns **CURIOUS** (wide eye,
   small mouth).
3. Move within 10 cm: it becomes **NERVOUS** (darting eyes, jittery mouth). Note
   it passes through **HESITANT** if it was HAPPY first.
4. Tap the pet button 3× quickly: it beams **HAPPY** and holds it briefly.
5. Walk away and wait: **SAD**, then eyes close into **SLEEPY**.
6. Suddenly put your hand right in front: it **SURPRISES** awake (frozen wide
   eyes), then settles to CURIOUS.

## Practical checklist

1. Flash the firmware (`make flash`) and power the board from 5 V.
2. On boot the matrix should show a neutral face (two eyes, level mouth) and
   blink every few seconds. If it's blank, check CS/PB2 and the MAX7221 RSET.
3. Hold a hand ~30–40 cm in front and move it slowly closer — the mouth should
   shrink to a curious "o" and one eye widen.
4. Move to within ~8 cm — the eyes should start darting (nervous).
5. Press the pet button three times within two seconds — the face should switch
   to a big smile and hold it ~3 s before relaxing.
6. Leave everything still for a minute — the face should sag (sad) and then close
   its eyes (sleepy).
7. From sleepy, jab your hand right up to the sensor — it should snap to a
   frozen wide-eyed surprise, then relax to curious.

## Scope + timing verification (proving it's non-blocking)

- **Loop period:** probe **PD3** (toggled high at the top and low at the bottom
  of the main loop). The waveform should show a stable, short high pulse every
  loop with no long "stuck low" gaps — even while an expression is tweening or
  an SPI burst is going out. A stuck-low segment would mean something blocked.
- **SPI budget:** probe **CS/PB2**. Each refresh is 8 back-to-back 16-bit bursts;
  confirm the whole burst finishes well within the 40 ms animation-tick window
  (it takes only a few hundred µs at 1 MHz SPI), so display refresh never starves
  sensing or animation.

## ~60-second demo video (what an interviewer should see)

1. **0:00** Board powers up → neutral face, an idle blink. *(narrate: "emotions
   are driven by sensors, with a mood model in between")*.
2. **0:10** Hand approaches from far → face turns **curious**; push closer →
   **nervous**, eyes darting. *(show the CURIOUS→NERVOUS tween, no snap-cut)*.
3. **0:25** Tap the pet button 3× → **happy**, held for a beat. *(mention the
   3-in-2-s gesture and the min-hold via decay)*.
4. **0:38** Step back and wait → **sad**, then eyes shut into **sleepy**.
5. **0:50** Suddenly lunge a hand at the sensor → **surprised** (frozen), settles
   to curious. *(call out SLEEPY→SURPRISED is the one legal abrupt jump)*.
6. **0:58** Cut to the oscilloscope on PD3 showing the steady loop period —
   "non-blocking, tick-driven, no delays."

## Theory

Short version: MAX7221 multiplexing + no-decode mode; SPI mode 0; Timer0 CTC math
for an exact 1 ms tick; Timer1 input capture for the echo; floating-ADC entropy;
xorshift32; the valence/arousal model and why decay is linear. Full write-up in
[`docs/Theory.md`](docs/Theory.md).

## Memory footprint

Latest `avr-size -Os` build:

- Flash `.text + .data` ≈ **6.7 KB / 32 KB (~20 %)**
- SRAM `.data + .bss` ≈ **394 B / 2 KB (~19 %)**

## Stretch goals

- **Tier-3 audio**: implement `sensors_audio_tick()` — rolling-RMS mic envelope on
  ADC1, loud transient → arousal spike (the `ENABLE_AUDIO` seam is in place).
- Per-pixel PWM brightness crossfade as an alternate tween (needs software
  bit-angle modulation since the MAX7221 intensity is global).
- Richer multi-keyframe expressions (the pipeline already supports up to 4).
- A second daisy-chained panel for a wider face (the SPI/MAX7219 driver is
  already chain-aware — bump `CHAIN_DEVICES`).
- Persist a "temperament" bias — deliberately **not** in EEPROM per project rules;
  derive it from boot entropy instead.
