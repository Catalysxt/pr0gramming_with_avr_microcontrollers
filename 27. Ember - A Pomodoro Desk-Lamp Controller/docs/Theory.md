# Theory — Ember (Pomodoro Desk-Lamp Controller)

The physics/electronics behind every non-trivial part of this project.

---

## 1. Rotary-encoder quadrature decoding (KY-040)

The KY-040 has two switches, CLK (A) and DT (B), mechanically offset so that
as you turn the shaft they open/close **90° out of phase**. Read together as a
2-bit number `(CLK<<1 | DT)` they walk a **Gray code** — a sequence where only
one bit changes per step:

```
CW :  00 → 01 → 11 → 10 → 00      (one detent = one full loop)
CCW:  00 → 10 → 11 → 01 → 00
```

Direction falls out of *which bit changed first* (datasheet p.1). Because only
one bit ever changes per legal step, the four "diagonal" jumps (00↔11, 01↔10)
are **impossible** — and that is exactly what makes the decoder immune to
contact bounce. A bouncing contact tries to skip states; those transitions
score 0 in the lookup table and are silently dropped.

**Why not just count CLK edges?** Cheap encoders bounce hard: one physical
detent can produce 3–5 CLK edges. Naive edge-counting reports phantom steps
and even wrong directions. The transition table (`k_qdec[16]` in `encoder.c`),
indexed by `(prev_state<<2 | new_state)`, converts each *legal* quarter-step to
±1 and rejects the rest. Four quarter-steps make one detent, so we accumulate
and only emit a detent every ±4 → clean ±1 counts per click.

**Why interrupt-driven (PCINT)?** Polling would have to sample faster than the
fastest twist to avoid missing a state. A pin-change interrupt fires the
instant either line moves, so no step is ever lost, and — crucially — a PCINT
also **wakes the MCU from power-down sleep** (see §5).

---

## 2. Multiplexed common-cathode 7-segment display (Kingbright CC56-12SRWA)

Four digits, but only **one** set of 8 segment lines (a–g + DP) — shared across
all four. Each digit has its own **common cathode**. To show four different
numbers at once you *can't*; instead you **multiplex**:

1. Light digit 1's segments for ~1 ms, then blank.
2. Light digit 2's segments for ~1 ms, then blank. …and so on.

At 4 digits × 1 ms = 4 ms/frame → **250 Hz**, far above the eye's ~60 Hz
flicker-fusion threshold, so **persistence of vision** fuses the four flashes
into one steady 4-digit number. `display.c` does this in a Timer2 CTC ISR that
fires every 1 ms and advances one digit.

**Common cathode = straight logic.** The cathode is the LED's `−` side, tied
together per digit. A segment lights when its **anode (segment pin) is HIGH and
its common cathode is LOW**. Segments are therefore driven *active-HIGH*, and a
digit is selected by pulling its common cathode LOW (to GND).

> **History:** the first build assumed a common-**anode** part (the "CA" family
> prefix) and stayed dark. A bench probe — shorting each digit common to GND
> with all segments driven HIGH lit every segment — proved the part is actually
> the common-**cathode** sibling (`CC56-12SRWA`). Polarity is captured entirely
> by `EMBER_SEG_ACTIVE_LOW` / `EMBER_DIGIT_ACTIVE_LOW` in
> `hardware_connections.h`, both now `0`; the driver code is polarity-agnostic.

**Ghosting** is the classic multiplex bug: if you change the segment pattern
while a digit is still selected, the new pattern briefly appears on the old
digit. The fix is strict ordering in the ISR: **deselect the old digit →
set the new segments → select the new digit.**

### The per-pin current trap (why we add transistors)

A lit "8" is all 8 segments on one digit. At even 10 mA/segment that's 80 mA
flowing through that one common pin — but the ATmega328P's absolute-max is
**40 mA per pin** (and ~200 mA total across the chip). Passing a whole digit's
current directly through an MCU pin is out of spec and will brown out or degrade
the pin. On a common-cathode display that shared pin has to *sink* to GND, so
Ember switches each digit common with an **NPN low-side transistor** (e.g.
2N3904 / BC337): the MCU pin drives the *base* (a few mA through a base resistor)
and the transistor sinks the full digit current to GND.

An NPN low-side switch conducts when its base is pulled **HIGH** — so the
digit-select logic is *active-HIGH* in hardware. `EMBER_DIGIT_ACTIVE_LOW` in
`hardware_connections.h` captures this (`0` here); flip it only if you rewire to
a high-side/common-anode arrangement. Each **segment** still gets its own series
resistor (~220 Ω) to bound the current the MCU *sources*.

**Base-drive sanity check (why 1 kΩ is fine here).** With 220 Ω segment
resistors a full "8" draws ≈ 8 × 13 mA ≈ 100 mA through the digit NPN. The 1 kΩ
base resistor delivers (5 − 0.7)/1 kΩ ≈ 4.3 mA of base current — a *forced β* of
~23, well inside saturation for a 2N3904/BC337 (β ≫ 50), so the transistor drops
only ~0.2 V and runs cool. Push segment current much higher (resistors below
~180 Ω) and you'd need a stiffer base resistor (~330 Ω) to keep it saturated.
Note this is also why **BC547 is a poor choice**: its 100 mA Ic ceiling sits
right at the digit current.

---

## 3. Passive piezo buzzer + Timer1 tone generation

A *passive* piezo has no internal oscillator — it's essentially a capacitor
that flexes with applied voltage. To make sound you must feed it a **square
wave** at the desired pitch. Timer1 in CTC mode toggles its OC1A output pin
(PB1) every time the counter hits `OCR1A`, producing a square wave in hardware
with zero CPU involvement:

```
f_out = F_CPU / (2 · prescaler · (OCR1A + 1))
```

The factor of 2 is because it takes *two* toggles (one full high-low cycle) per
output period. Timer1 is 16-bit, so a single `/8` prescaler covers the whole
100–3000 Hz range without overflow. Tone *duration* is non-blocking: the note's
stop-time is scheduled against the 1 ms `sys_tick`, and `buzzer_service()`
silences or advances it from the main loop — no `_delay_ms`.

**Timer budget note:** Timer0 = sys_tick, Timer2 = display refresh, so the
buzzer gets Timer1 (the prompt asked for this explicitly). All three timers are
in use; there is none to spare, which is why sleep uses PCINT wake, not a timer.

---

## 4. Relay switching a mains lamp — and the safety that matters

A relay is an electromagnet that pulls a mechanical contact closed. The MCU
can't drive the coil directly (tens of mA, plus a back-EMF spike when it
releases), so we use a **relay module** that carries its own coil driver
transistor and **flyback diode** (which clamps the inductive back-EMF spike
that would otherwise destroy the driver). The MCU just presents a logic level
to the module's IN pin — active-HIGH here → relay energised → lamp ON.

### ⚠ Mains safety (240 V AC)

The desk lamp switches **240 V mains**. This is lethal. Rules for the load side
of the relay:

- Break the **live/active** conductor only. Wire it: mains live → relay
  **COM**, relay **NO** → lamp live. (NO = "normally open": lamp is off until
  the relay energises.)
- **Never** connect the MCU's GND (or any low-voltage wire) to the relay's COM
  or to mains. Doing so exposes your logic ground — and you — to 240 V.
- Keep mains-side wiring physically separated from the 5 V side; observe the
  module's creepage/clearance; enclose it.
- **Inrush:** an incandescent/AC bulb draws a large cold-filament inrush
  current at switch-on. The magnetic/thermal transient can brown-out the MCU
  and reset it mid-session. Mitigate with a bulk capacitor on the 5 V rail,
  good decoupling, and a sane brown-out detector fuse setting. Consult
  *Practical Electronics for Inventors* on relays and high-power loads.

---

## 5. Sleep: µA idle without a watch crystal

The prompt suggested `SLEEP_MODE_PWR_SAVE` with an async Timer2 clocked by a
32.768 kHz crystal. On the ATmega328P that crystal must sit on TOSC1/TOSC2 —
**the same physical pins (PB6/PB7) as the 16 MHz main crystal's XTAL1/XTAL2.**
You can have one or the other, not both. And a Pomodoro *needs* an accurate
timebase (the 16 MHz crystal gives <0.1 s drift over 25 min; the internal RC
would drift *minutes*), so we keep the crystal.

The insight that removes the conflict: **Ember never has to keep time while
asleep.** It only counts down *after* you press start, and during a countdown
the display is multiplexing (CPU busy) anyway. Deep sleep is only useful in
IDLE, waiting for you to touch the encoder. So IDLE uses
`SLEEP_MODE_PWR_DOWN` — the deepest mode, stopping every clock — and wakes on a
**pin-change interrupt** from the encoder (async, works with all clocks
stopped). This reaches µA-range idle with **no extra crystal and no fuse
change**, and leaves Timer2 free for the display.

The `sleep_enable()/sei()/sleep_cpu()` ordering is the textbook race-free idiom:
enabling interrupts immediately before `SLEEP` guarantees that a wake event
firing in the gap still executes `sleep_cpu()` as its very next instruction, so
an early wake can never be missed.

---

## 6. EEPROM-persisted presets

Internal EEPROM keeps the last-used work/break minutes and a lifetime session
counter across power cycles. EEPROM cells wear out (~100 k writes), so Ember
uses `eeprom_update_*` (which reads-before-writing and **skips unchanged
bytes**) and only writes on confirm, never every tick. A 2-byte **magic word**
distinguishes a legitimately-stored config from a freshly-erased chip
(0xFF 0xFF), making first-boot defaulting unambiguous.
