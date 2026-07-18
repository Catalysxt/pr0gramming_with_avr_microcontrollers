# Theory — Digital Companion

The electronics/physics behind each non-trivial component, and the register math
that follows from it.

## MAX7221 LED driver + 8×8 matrix

An 8×8 LED matrix has 64 LEDs but only 16 pins (8 row anodes × 8 column
cathodes). The MAX7221 **multiplexes** them: it lights one digit (row) at a time,
cycling through all 8 fast enough that persistence of vision fuses them into a
steady image. It holds an 8×8 frame in internal registers and does the scanning
itself, so the MCU only writes 8 bytes per frame.

- **No-decode mode** (`DECODE = 0x00`): the data byte for a DIGIT register is used
  directly as the raw column pattern for that row. (BCD-decode mode is only for
  7-seg numerals; a face needs raw pixels.)
- **Scan-limit = 7:** scan all 8 digits/rows.
- **Intensity:** a single global 4-bit PWM (0–15) on the segment current. Note it
  is *global* — the chip cannot dim individual pixels, which is exactly why the
  animator tweens with an XOR bit-morph rather than a per-pixel brightness fade.
- **Shutdown = 1:** leave shutdown (normal run). It is programmed **last** in
  init so the panel stays blank until every control register is set.
- **Latch timing:** each chip clocks in 16 bits MSB-first (D15..D8 = register,
  D7..D0 = data) on SCK, and latches them on the **rising edge of CS/LOAD**. That
  edge is why CS is driven per-frame in software.

The '7221 (vs the '7219) is slew-rate-limited and internally current-regulated,
so it is quieter on the supply and gentler on breadboard wiring.

## SPI (mode 0, MSB-first, fosc/16)

SPI is a synchronous shift register: MOSI shifts a bit out on one clock edge, the
peripheral samples it on the other. **Mode 0** (CPOL=0, CPHA=0) means clock idles
low and data is sampled on the rising edge — what the MAX7221 expects.
`fosc/16 = 1 MHz` at 16 MHz is far below the chip's 10 MHz limit and forgiving of
long jumper wires. SS/PB2 must stay an output while the AVR is SPI master: a low
on an SS *input* clears the MSTR bit in hardware and the master goes mute.

## Timer0 — 1 ms tick (CTC)

CTC (Clear Timer on Compare) resets the counter to 0 when it hits OCR0A, so the
period is exact. With a /64 prescaler the timer clock is `16 MHz / 64 = 250 kHz`.
For a 1 ms (1 kHz) tick: `250000 / 1000 = 250` counts, and since the count
includes 0 we set `OCR0A = 250 − 1 = 249`. Integer-exact → zero drift.

## Timer1 — HC-SR04 echo via input capture

Ultrasound travels at ~343 m/s. The HC-SR04 emits a 40 kHz burst on TRIG and
raises ECHO for the round-trip flight time. Distance = (echo_time × 343 m/s) / 2.
In convenient units, `distance_cm = echo_µs / 58`.

Rather than busy-waiting on ECHO, Timer1's **input-capture** unit copies the
free-running counter into ICR1 the instant ICP1 (PB0) changes, and fires an
interrupt. The ISR timestamps the rising edge, flips `ICES1` to catch the falling
edge, and subtracts to get the pulse width — all in hardware-precise time with no
polling. The **noise canceller** (`ICNC1`) requires 4 agreeing samples before a
capture, rejecting ultrasonic ringing glitches. A /64 prescaler gives 4 µs/tick;
the 16-bit counter wraps every `65536 × 4 µs ≈ 262 ms`, comfortably past the
~38 ms pulse the sensor emits when nothing is in range, so `end − start`
(unsigned) is always the true width.

## ADC — floating-pin entropy

An unconnected ADC input floats on thermal + coupled noise; its low bits are
effectively random. Sampling ADC0 a few times at boot and XOR-folding the low
bits yields a seed with real entropy. The /128 prescaler gives a 125 kHz ADC
clock — inside the 50–200 kHz window the datasheet requires for full 10-bit
accuracy (each conversion is 13 ADC clocks ≈ 104 µs). This seeds the PRNG.

## xorshift32 PRNG

`x ^= x<<13; x ^= x>>17; x ^= x<<5;` — Marsaglia's xorshift32: full period of
2³²−1, passes basic randomness tests, and costs a handful of cycles versus libc
`rand()`'s bulk. Zero is a fixed point and is remapped away. Seeded from ADC
noise, then mixed with `TCNT1` captured on the first pet-button press so the idle
blink/saccade pattern depends on real user timing.

## The mood model — valence & arousal

Affect is modelled as a point in a 2-D space borrowed from psychology:
**valence** (pleasant↔unpleasant) and **arousal** (calm↔alert). Sensor events
*nudge* the point; every 100 ms it **decays** one unit per axis toward (0,0).

Linear decay (subtract the sign) is chosen over exponential (`x -= x>>k`) because
it gives a constant, predictable cool-off slope — a strong event fades in a known
number of steps, which makes the behaviour easy to reason about and to pin down
in a golden unit-test table. Exponential decay is also reasonable and feels
natural for big spikes, but its long tail leaves tiny residuals lingering near
zero; linear reaches exactly (0,0) and stops.

An FSM then maps the point to an expression, with a ±8-unit **hysteresis** margin
so a point resting on a region boundary doesn't flicker between two moods — the
same Schmitt-trigger idea used for a noisy analog threshold, applied in mood
space.

## Idle micro-motion — why a still face looks dead

Human eyes blink (~every 4 s) and make tiny **saccades** constantly; a face that
holds perfectly still reads as lifeless or uncanny. The animator composites a
Poisson-ish blink (randomised interval so it never looks metronomic), small
±1-column eye darts, and a slow breathing sway of the mouth on top of the base
expression — cheap touches that make the companion feel alive.
