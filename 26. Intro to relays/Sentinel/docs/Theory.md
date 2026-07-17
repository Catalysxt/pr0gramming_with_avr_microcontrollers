# Theory — Sentinel (relay/sentinel)

This project switches a 12 V LED with a relay under the control of a finite state
machine, using non-blocking timing and a debounced button. The interesting theory
is in four areas: the electromechanics of the relay, the software architecture
that replaced the blocking blink, the debounce algorithm, and EEPROM wear.

## 1. Relay coil, back-EMF, and why a module (not a ULN2803)

A relay closes its contacts by energising an electromagnet (the coil). Two facts
drive the electronics:

- **The coil needs more current than an MCU pin can source, and it is inductive.**
  An ATmega328P GPIO sources ~20 mA, often less than a coil wants; and because the
  coil is an inductor, *its current cannot change instantly* (`V = −L·di/dt`). When
  the drive switches the coil **off**, the collapsing field induces a large
  reverse-voltage spike — **back-EMF** — that would punch through a bare
  transistor. The classic fix is a **flyback (freewheeling) diode** across the coil
  that harmlessly circulates the decaying current (the same reason a solenoid or
  motor needs one).
- **A relay *module* already solves both problems for you.** This project drives a
  self-contained relay module — a small board carrying the relay plus its own
  driver transistor, base resistor, and flyback diode (often an opto-isolator on
  the input too). The current sink and the back-EMF clamp are all on the module, so
  **PB0 drives the module's `IN` pin directly** — no external ULN2803. An MCU pin
  easily supplies the few mA the module's input needs.

This is the key contrast with the base `relay/hello_world`, which switched a
**bare** coil and therefore *did* need a ULN2803 (a Darlington array that sinks
coil current, with integral clamp diodes whose common cathode — pin 10, COM, on a
real ULN2803A — ties to the coil rail). Put a ULN2803 in front of a *module* and
you stack two drivers: worse, the ULN2803 only **sinks** (drives its output LOW),
so against this **active-HIGH** module (energises when `IN` is HIGH) it can never
turn the relay on. Rule of thumb: **bare coil → ULN2803 + flyback; module → drive
`IN` straight from the pin, matching the module's trigger polarity.**

The relay's *contacts* then switch the isolated 12 V LED circuit — the whole point
of a relay is that a tiny 5 V logic signal gates a physically separate,
higher-voltage load.

## 2. Finite state machine + non-blocking timing (the heart of the extension)

The base project was a **blocking** loop:

```c
while (1) { PORTB |= 1; _delay_ms(1000); PORTB &= ~1; _delay_ms(1000); }
```

During each `_delay_ms` the CPU spins uselessly and can respond to nothing — a
button press during the delay is simply missed. This project replaces that with a
**superloop + finite state machine**:

- **Timebase.** `sys_tick` configures **Timer0 in CTC mode** with a /64 prescaler
  and `OCR0A = 249`, so the compare-match interrupt fires every
  `(249+1)·64 / 16 MHz = 1 ms`. The ISR just increments a `volatile uint32_t`
  millisecond counter; `sys_millis()` reads it inside an `ATOMIC_BLOCK` to avoid a
  torn read of the 4-byte value on an 8-bit core.
- **Scheduling.** `sys_elapsed(&anchor, period)` returns true once per `period`
  and advances the anchor **by the period, not to "now"**, so repeated intervals
  do not accumulate drift. AUTO mode toggles the relay off this primitive instead
  of blocking.
- **The FSM.** Four states — `AUTO`, `MANUAL_ON`, `MANUAL_OFF`, `LOCKOUT` — with an
  explicit `enter()` function (on-entry side effects: drive relay, print, persist)
  and a `tick()` function (decide transitions, run per-state background work).
  Transitions are always explicit `enter(next)` calls. Because nothing blocks, the
  button is serviced every loop pass regardless of what the relay is doing.

`LOCKOUT` is a **safety state**: the relay is forced OFF and *short* presses are
ignored. It is **toggled by a long hold (≥ 2 s)** — the same deliberate gesture
both arms and clears it — and a reset also clears it. Requiring a long hold (never
a stray tap) to leave LOCKOUT is what keeps it a "freeze everything safe" that
cannot be casually undone. On unlock it resumes the operational state it was in
before locking out (read back from EEPROM, since LOCKOUT itself is never saved).

## 3. Software debounce via a sampling timer

A mechanical button does not switch cleanly — the contacts **bounce** for a few
milliseconds, producing a burst of rapid HIGH/LOW transitions. Reading the pin
once would register that burst as many phantom presses.

The debounce here is a **sampling filter**, not a `_delay_ms` blockade:

- Every `BTN_SCAN_MS = 5 ms` (gated by `sys_elapsed`), sample the raw level.
- A new level must **repeat for `BTN_DEBOUNCE_SCANS = 3` consecutive samples**
  (~15 ms of stability) before it is accepted as the real state. Bounce, being
  shorter and noisier than that, never survives the filter.
- **Long-press detection** rides on the same confirmed state: once a press is
  confirmed, a timer runs; crossing `BTN_LONGPRESS_MS = 2000 ms` fires a single
  long-press event and suppresses the short-tap that would otherwise fire on
  release. This is why *bounce is never mistaken for a long press* and *a real
  2 s hold is never mistaken for a tap*.

## 4. EEPROM wear and safe persistence

The FSM's operational state is saved to EEPROM so a power-cycle resumes it. Two
disciplines matter:

- **Wear.** An EEPROM cell tolerates ~100 000 writes. `eeprom_update_byte` reads
  first and writes *only if the value changed*, so re-entering the same state
  costs no wear. (Never use `eeprom_write_byte` for state that may repeat.)
- **LOCKOUT is never persisted.** Only `AUTO/MANUAL_ON/MANUAL_OFF` are saved, for
  two reasons: (1) a board that lost power mid-lockout should boot to a normal
  operational mode, not resume a safety freeze; and (2) because the saved byte
  therefore always holds the last *operational* state, unlocking can simply reload
  it to resume where the user left off. Blank EEPROM reads `0xFF` and falls back to
  the safe `AUTO` default.

## References

- ATmega328P datasheet §15 (Timer0/CTC), §8 (EEPROM), §14 (I/O ports).
- ULN2803A datasheet — see `../../hello_world/docs/uln2803c.pdf` (Darlington array
  with integral clamp diodes).
- avr-libc: `<util/atomic.h>` (ATOMIC_BLOCK), `<avr/eeprom.h>` (eeprom_update_*).
