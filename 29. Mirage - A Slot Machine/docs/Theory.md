# Theory of operation

The physics/electronics behind each non-trivial block. Datasheet references:
MAX7219/MAX7221 (`../docs/MAX7219_MAX7221.pdf` in the stopwatch project),
ATmega328P.

## 1. MAX7219/MAX7221 LED driver + daisy chaining

A MAX7219 multiplexes up to **8 digits × 8 segments** (64 LEDs) from one chip: it
scans one common-cathode digit at a time, fast enough that persistence of vision
fuses them. You program it over a 16-bit SPI word — `D15..D8` = register address,
`D7..D0` = data — latched on the **CS rising edge**.

**No-decode mode** (decode register = 0x00) means the data byte *is* the raw
segment pattern. That single choice is what lets the same driver render 7-segment
glyphs (via `font7seg`) and arbitrary 8×8 dot-matrix rows: both are just "which of
these 8 outputs are on."

**Daisy chain:** each chip has a `DOUT` that re-emits what it clocked in, delayed
by 16 bits. Wire `MCU MOSI → chip0 DIN`, `chip0 DOUT → chip1 DIN`, … and share
CLK + CS. To load N chips you hold CS low and shift **N×16 bits**; the first word
ripples to the farthest chip, the last stays in the nearest, and one CS edge
latches all of them. Here N = 5 (score chip + 4 matrix panels). This is why
`max7219_chain_write()` shifts `data[count-1]` first.

**Init sequence** (registers power up undefined, so we set every one we use):
display-test off → decode none → scan-limit 7 → intensity → blank digits →
shutdown = normal (released last, so the panel stays dark until configured).

**Intensity** is set by a 4-bit register that PWMs the segment current (duty from
1/32 to 31/32). Peak current is set in hardware by `R_SET` on ISET.

## 2. SPI — mode 0, MSB-first, fosc/16

The ATmega's hardware SPI shifts a byte out MSB-first while shifting one in. The
MAX chips sample DIN on the **rising** SCK edge and need SCK idle-low → that's SPI
**mode 0** (CPOL = 0, CPHA = 0). We run fosc/16 = 1 MHz (far below the 10 MHz
ceiling) for breadboard margin. `SS`/PB2 must remain an output in master mode or
the hardware clears `MSTR` and stops driving the bus.

## 3. Timer0 — the 1 ms system tick (CTC)

Timing is decoupled from the CPU by a periodic interrupt. In **CTC mode** the timer
counts to `OCR0A` then resets, firing `TIMER0_COMPA`. With the /64 prescaler the
timer clock is 16 MHz/64 = 250 kHz; a match every 250 counts is 1 ms, so
`OCR0A = 250 − 1 = 249`. The division is exact → **no drift**. The ISR only
increments a `volatile uint32_t`; everything else reads it via `timing_ms()`,
which wraps the 4-byte read in an `ATOMIC_BLOCK` so a mid-update tick can't tear it.

## 4. Timer1 — tone generation (CTC toggle on OC1A)

A square wave *is* a musical tone. In CTC mode with `COM1A0 = 1`, the hardware
**toggles the OC1A pin (PB1) on every compare match** — no CPU cycles per edge.
Two matches make one full wave, so:

```
f_out = F_CPU / (2 · prescale · (OCR1A + 1))   ⇒   OCR1A = F_CPU/(2·prescale·f) − 1
```

With prescale = 8, `OCR1A` stays inside 16 bits across the audio band (~250 at
4 kHz, ~9999 at 100 Hz). A **rest** detaches OC1A and stops the clock (pin idle
low). Note *durations* are timed by the Timer0 tick in `buzzer_tick()`, which is
why the melody engine is non-blocking and interruptible — starting a new sequence
just overwrites the pointer.

## 5. Switch debounce + long-press

A mechanical contact bounces for a few ms, so one press can read as many. The
fix (`buttons.c`) samples every 1 ms and only commits a *changed* level after it
holds steady for `BUTTON_DEBOUNCE_MS` (15 ms) — longer than bounce, shorter than
human perception. Buttons are **active-low** with the internal pull-up: idle pin =
HIGH, press = LOW, no external resistor. A confirmed press whose hold passes
`BUTTON_LONG_PRESS_MS` (1 s) fires `BUTTON_LONG` mid-press; a shorter one fires
`BUTTON_SHORT` on release. That one-button split drives "short = navigate, long =
enter settings."

## 6. Randomness — xorshift32 seeded from ADC noise + TCNT1

`rand()` from libc is large and, unseeded, deterministic. **xorshift32**
(Marsaglia) is three shift/XOR steps over a 32-bit word — tiny, fast, period
2³²−1 (state must never be 0). Seeding needs entropy an MCU normally lacks:

- An **unconnected ADC pin floats**; its low bits are genuine analog noise. We
  fold several conversions together at boot for a seed.
- That is XORed with **`TCNT1` sampled at the first spin** — a free-running timer
  captured at a human-chosen instant, so the whole session's outcomes hinge on
  *when* you first pressed. Unpredictable without being cryptographic (it needn't
  be — it's a toy).

## 7. Reel easing — why fast → slow → snap

Real reels have inertia. We fake it: a reel scrolls one pixel row every
`spin_period()` ms, and that period ramps from 25 ms up to 110 ms over the last
`SPIN_EASE_MS` before the reel's (randomized, staggered) stop time — then the reel
only halts when it is aligned on a whole symbol. Growing the period *is* the
deceleration; the alignment check *is* the "snap." The buzzer's spin-loop click is
emitted per step, so its rate slows with the reels for free.

## 8. Persistence-of-vision refresh budget

The full display is 5 chips × 8 registers × 16 bits = 640 bits ≈ 80 µs of SPI at
1 MHz. Refreshing at a fixed ~66 Hz (`DISPLAY_PERIOD_MS = 15`) is flicker-free and
leaves the bus 99 % idle for game logic — far better than blasting it every loop.
