# Theory

Short notes on the non-trivial physics/electronics/CS behind each
subsystem — not a general AVR tutorial, just the parts of this project
that aren't self-evident from the code.

## 1. HD44780 DDRAM and busy-flag timing

The LCD controller's DDRAM (Display Data RAM) holds the character codes
currently shown on screen — writing byte `0x41` to DDRAM address 0x00
shows `A` at row 0, column 0. ASCENT doesn't use CGRAM (no custom
glyphs — the expression-scroll truncation indicator is the literal ASCII
`<` character).

Every command and data byte takes a variable amount of time to execute
internally (39 µs for most instructions, 1.53 ms for Clear Display /
Return Home). Rather than hard-coding worst-case delays before every
transfer, `lcd_driver.c` polls the Busy Flag (bit 7 of the status byte,
read with RS=0, R/W=1) before each new transfer and proceeds the instant
the controller reports it's ready — faster in the common case, and
correct even if a particular command takes longer than expected.

## 2. Matrix keypad scanning

A 4×4 matrix keypad has 8 physical pins wired as 4 rows × 4 columns,
with a switch at every row/column intersection — not 16 individually
wired buttons. To find out which (if any) key is pressed, the MCU drives
one row low at a time (the other three stay high) and reads the column
bus: a column reading low means the switch at that (row, column)
intersection is closed, shorting the driven-low row through to that
column. Columns are configured with internal pull-ups so an unpressed
column reads a solid high rather than floating.

**Debounce** exists because a mechanical switch's contacts don't close
cleanly — they physically bounce for a few milliseconds, producing a
burst of spurious open/close transitions before settling. ASCENT
requires the same raw scan result to repeat for 3 consecutive 5 ms scans
(15 ms) before treating it as a real, stable press or release, which is
well past typical contact-bounce duration for a tactile keypad.

## 3. Passive piezo tone generation

A passive piezo buzzer has no internal oscillator — apply a DC voltage
and it just clicks once. To produce a tone, the driving pin must
oscillate at the desired audio frequency. Timer2's CTC (Clear Timer on
Compare match) mode does this in hardware: the counter increments every
prescaled clock tick and resets to 0 the instant it matches `OCR2A`,
while the `COM2A0` bit tells the hardware to toggle the `OC2A` pin (PB3)
on every compare match — no CPU intervention needed once configured.
Toggling every compare match produces a 50%-duty square wave at
`F_CPU / (2 * prescaler * (OCR2A + 1))` Hz. Solving for `OCR2A` given a
target frequency, and picking the smallest prescaler that keeps it in
the timer's 8-bit range, is exactly the search `buzzer.c` performs.

## 4. Two's-complement overflow, and why `abs(INT32_MIN)` is undefined behaviour

`int32_t` on this target is two's complement with range
`[-2147483648, 2147483647]` — one more negative value than positive.
That asymmetry exists because two's complement represents a negative
number as `2^32 - |n|`; the single all-ones-in-the-top-bit pattern
(`0x80000000`) is used for `-2147483648`, leaving no bit pattern free to
represent `+2147483648`. Consequently `-INT32_MIN` (and any C library
`abs()` given `INT32_MIN`) doesn't fit back into `int32_t` — evaluating
it is undefined behaviour, not a defined wraparound, and a compiler is
free to miscompile it under `-O2`-style optimisation.

This is a genuine hazard in a calculator: `calc.c`'s multiply-overflow
check needs each operand's magnitude, and `calc_format()` needs the
magnitude of whatever value it's about to print — both are exactly the
operations that trip over `INT32_MIN`. `calc.c` sidesteps the problem
with one small helper, `abs_u32()`, that converts to `uint32_t` *before*
negating:

```c
static uint32_t abs_u32(int32_t v) {
    return (v < 0) ? ((uint32_t)0u - (uint32_t)v) : (uint32_t)v;
}
```

The cast `(uint32_t)v` is well-defined for every possible `int32_t`
value, including `INT32_MIN`, per the C standard's signed-to-unsigned
conversion rule (the value is reduced modulo 2^32). Unsigned subtraction
also wraps modulo 2^32 rather than trapping, so for `v == INT32_MIN` the
expression evaluates to `0u - 0x80000000u == 0x80000000u` — exactly
`2147483648`, the correct magnitude, reached without ever negating a
signed value that couldn't hold the result.

## 5. `SLEEP_MODE_PWR_DOWN` and the PCINT wake mechanism

Power-down is the ATmega328P's deepest sleep mode: it stops the CPU
clock entirely, along with every peripheral clock derived from it —
only an asynchronous event (an external interrupt, or a pin-change
interrupt) can restart the oscillator and resume execution. That's why
`sleep_enter_power_down()` explicitly gates Timer0 and Timer2 off first
via `power_timer0_disable()`/`power_timer2_disable()` (`<avr/power.h>`,
which set bits in PRR, the Power Reduction Register): those timers
can't keep running through power-down regardless, and leaving their
clock request bits set would keep drawing a small amount of current from
whatever residual clock source remains active before the mode fully
settles.

The wake source here is a **pin-change interrupt**, not a full external
interrupt (INT0/INT1) — PCINT8-11 watch PC0-PC3 (the keypad columns) for
*any* logic change, not just one edge direction, and don't need a
specific pin mode configured beyond being an input. The trick that makes
a keypress observable while every clock is stopped: immediately before
sleeping, all four row pins are driven LOW (`keypad_rows_sleep()`).
Normally rows idle HIGH and a column only goes low when a *specific* row
is driven low during an active scan — but with every row already low,
pressing *any* of the 16 keys directly shorts its column to a
already-low row, pulling that column low immediately, no scanning
required. That transition is exactly what fires `PCINT1_vect`.

The `sleep_enable(); sei(); sleep_cpu();` ordering (rather than, say,
enabling interrupts first and sleeping second) is the standard AVR
race-free idiom: `sleep_cpu()` is guaranteed by the avr-libc contract to
execute as the very next instruction after `sei()` returns, so a
PCINT firing in the brief window between "interrupts become enabled" and
"the SLEEP instruction actually executes" still can't be lost — the CPU
services the pending interrupt and then immediately executes the SLEEP
instruction it was already committed to, rather than sleeping through an
event that already happened.
