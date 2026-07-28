# Pocket Casino — a bare-metal AVR walkthrough

> A guided read of the whole firmware for someone who knows C and basic
> electronics but is still building intuition for bare-metal AVR work.
> Written against the tree as of 2026-07-28.

---

## TL;DR

This is a self-contained handheld gambling toy built on an ATmega328P running at
16 MHz from an external crystal. Two buttons, a 16x2 HD44780-compatible character
LCD in 4-bit mode, and a passive piezo are the entire I/O surface. The firmware is
a classic *superloop plus two interrupts* design: a Timer0 compare-match ISR
increments a millisecond counter, a pin-change ISR turns button edges into
debounced events in a ring buffer, and everything else — game logic, animation,
LCD drawing, sound sequencing — happens in `main()`'s infinite loop by comparing
the current millisecond count against deadlines. Nothing blocks except the LCD
driver itself. Four games (slots, coin flip, higher-or-lower, dice) sit behind a
shared `enter`/`update`/`exit` interface and are dispatched from a top-level state
machine variable, `g_state`. The code is unusually well commented and
datasheet-cited for a hobby project, but it has several genuine defects flagged
below — the most consequential being a broken RNG seed, a family of non-atomic
32-bit reads shared with an ISR, and a build that does not currently compile
because of a wrong relative path.

---

## Table of contents

- [File structure](#file-structure)
- [Build system defects](#build-system-defects)
- [Hardware peripherals and why each was chosen](#what-the-hardware-actually-does-and-why-each-peripheral-was-chosen)
- [Initialisation order](#initialisation-order-matters-more-than-youd-expect)
- [The superloop](#the-superloop-and-the-division-of-labour)
- [Buttons](#buttons-debouncing-without-a-debounce-timer)
- [Buzzer](#the-buzzer-hardware-waveform-generation-and-a-prescaler-table)
- [RNG](#the-rng-and-why-its-seed-doesnt-work)
- [UI layer](#the-ui-layer-state-machine-cgram-and-a-column-counting-problem)
- [The four games](#the-games-four-variations-on-one-non-blocking-pattern)
- [LCD driver](#the-lcd-driver-bit-banging-a-parallel-bus-correctly)
- [Fuses](#fuses)
- [Concepts worth understanding deeply](#the-three-things-most-worth-understanding-deeply)
- [Suggested modification](#a-modification-that-tests-whether-you-actually-understand-these)
- [Defect summary](#appendix-defect-summary)

---

## File structure

```
my_avr_programming/
├── extern_libraries/                 (real location of shared libs)
│   └── lcd_driver/
│       ├── lcd_driver.h              HD44780/ST7066 4-bit API + register defines
│       └── lcd_driver.c              implementation (busy-flag polled)
└── lcd/
    ├── lcd_driver/                   duplicate copy of the same driver
    └── pocket_casino/                <-- the project
        ├── Makefile                  avr-gcc + avrdude, fuse targets
        ├── README.md                 wiring, fuses, gameplay
        ├── hardware_connections.h    single source of truth for pin assignments
        ├── docs/
        │   ├── state_machine.md      Mermaid FSM diagrams
        │   ├── codebase_walkthrough.md   this file
        │   └── Pocket Casino — Claude Code Prompt.md
        ├── pocket_casino.elf/.hex/.map   build artefacts (stale)
        └── src/
            ├── main.c                Timer0 tick, init order, superloop, dispatch
            ├── buttons.h / buttons.c  PCINT1 debounce, long-press, event queue
            ├── buzzer.h  / buzzer.c   Timer2 CTC tone gen + PROGMEM SFX tables
            ├── rng.h     / rng.c      16-bit xorshift, ADC-seeded
            ├── ui.h      / ui.c       g_state, g_credits, menu FSM, CGRAM glyphs
            └── games/
                ├── slots.h      / slots.c
                ├── coinflip.h   / coinflip.c
                ├── higherlower.h/ higherlower.c
                └── dice.h       / dice.c
```

---

## Build system defects

Note the `LIBDIR = ../extern_libraries` line in the Makefile. From
`lcd/pocket_casino/`, `..` is `lcd/`, so that resolves to `lcd/extern_libraries` —
which does not exist. The real library lives two levels up, and there is also a
copy at `lcd/lcd_driver/`. Running `make` today fails immediately with
*"No rule to make target `../extern_libraries/lcd_driver/lcd_driver.o`"*. The
`.map` file records that path, so at build time (late June) `lcd/extern_libraries/`
did exist and has since been moved or renamed. The checked-in `.hex` is therefore
stale relative to a build you can actually reproduce. Fix it by pointing `LIBDIR`
at `../../extern_libraries` or `..`, depending on which copy you consider
canonical.

There's a second, subtler Makefile bug hiding behind that one. Line 58 says
`HEADERS=$(SOURCES:.c=.h)`, and line 78 makes every `.o` depend on `$(HEADERS)`.
But `src/main.c` has no `src/main.h`. GNU Make silently discards a pattern rule
whose prerequisites cannot be satisfied, so even with `LIBDIR` fixed, the
`%.o: %.c` rule would quietly stop applying and Make would fall back to its
built-in rules — losing your `CFLAGS`, `-mmcu`, and `-DF_CPU` in the process.
That's a nasty failure mode, because it produces *host* object files rather than
an error. The idiomatic fix is to let the compiler generate dependencies
(`-MMD -MP` and `-include $(OBJECTS:.o=.d)`) rather than hand-rolling a header
list.

> **Try this first.** Fix `LIBDIR`, run `make`, and see whether it builds. Then
> deliberately reintroduce the `HEADERS` dependency problem by deleting
> `src/buttons.h` and running `make -d 2>&1 | grep -i "buttons"` — watch Make
> decide the pattern rule doesn't apply, and predict what compiler command it
> substitutes instead.

---

## What the hardware actually does, and why each peripheral was chosen

Four peripheral blocks are in play, and the choice of *which* timer does *what* is
driven entirely by pin conflicts, which is very typical of AVR work.

The **LCD** occupies PB0 (RS), PB1 (R/W), PB2 (E), and PD4–PD7 (the high nibble of
the data bus). Running in 4-bit mode halves the pin cost at the price of two bus
cycles per byte. The driver's `LCD_DATA_WRITE` macro is worth studying:

```c
#define LCD_DATA_WRITE(byte) (LCD_DATA_PORTREG = (uint8_t)((LCD_DATA_PORTREG & ~LCD_DATA_MASK) | ((uint8_t)(byte) & LCD_DATA_MASK)))
```

It performs a read-modify-write on `PORTD` masked to `0xF0`, deliberately
preserving PD0–PD3. PD0 and PD1 are the USART RX/TX pins; leaving them untouched
means you can bolt on serial debugging later without the LCD stomping on it. This
project doesn't use the USART at all — the Makefile still defines `-DBAUD=9600`
out of template inertia, and no USART register is written anywhere in the tree.
That's harmless but misleading.

The **buzzer** sits on PB3, and `hardware_connections.h` explains why in a comment
that is exactly the kind of reasoning worth internalising: PB3 is OC2A, the Timer2
output-compare pin. The "natural" choice for tone generation would be Timer1 —
it's 16-bit, so it can hit low frequencies directly without prescaler gymnastics —
but Timer1's OC1A output is PB1, which the LCD already drives as R/W. You cannot
have two peripherals owning one pin, so Timer2 wins by elimination and the code
has to work around Timer2's 8-bit counter with a prescaler lookup.

The **buttons** are on PC0 and PC1, active-low with internal pull-ups, handled by
`PCINT1_vect`. Note that these are *pin-change* interrupts, not the external
interrupts INT0/INT1 — those only exist on PD2/PD3, which are free here but
weren't used. Pin-change interrupts fire on both rising and falling edges and
share one vector per port group, so the ISR must read `PINC` to figure out what
actually happened. The header calls this out explicitly, correcting a spec that
apparently said "INT8/INT9".

The **ADC** appears exactly once, in `rng_init()`, to grab entropy from a floating
pin. We'll come back to that, because it doesn't work.

Timer0 is the system tick, Timer2 is the tone generator, Timer1 is unused, SPI and
I²C are untouched, and EEPROM is listed only as a v2 stretch goal.

> **Open `hardware_connections.h` and `lcd_driver.h` side by side.** Write down
> every pin either file claims, and check for collisions. Then ask yourself: could
> you move the buzzer to OC0B (PD5) and keep Timer0 as the tick? What would break?

---

## Initialisation: order matters more than you'd expect

`main()` performs six initialisations before enabling interrupts, and the header
comment in `main.c` spells out why the order is what it is. `timer0_init()` goes
first because `g_ms_tick` is the timebase everything else measures against —
`buttons_init()`'s debounce and long-press logic, `buzzer_tone()`'s auto-stop
deadline, and every game's animation frame counter all read it. `rng_init()` runs
before `sei()` because it busy-waits on the ADC and disables the ADC afterwards;
doing that with a pin-change interrupt live would be a needless hazard.
`Lcd_Init()` is last of the hardware setup because it takes over 50 ms of
`_delay_ms()` and there's no reason to hold interrupts off through it — except
that it *is* held off, because `sei()` comes after. And `ui_load_cgram()` runs
before `sei()` as well, pushing 64 bytes of glyph data into the display
controller.

Let's read the Timer0 setup closely, because it's the cleanest example of "why
each bit" in the project:

```c
TCCR0A = (1U << WGM01);                    /* CTC mode */
TCCR0B = (1U << CS01) | (1U << CS00);      /* /64 prescaler */
OCR0A  = 249U;
TIMSK0 = (1U << OCIE0A);
```

`WGM01 = 1, WGM00 = 0` selects Clear Timer on Compare Match: the counter counts up
to `OCR0A`, fires the compare interrupt, and resets to zero — rather than running
all the way to 0xFF as it would in normal mode. This is what makes the period
exactly controllable. `CS01|CS00 = 0b011` divides the 16 MHz system clock by 64,
giving a 250 kHz counting rate. `OCR0A = 249` means the counter takes 250 ticks
(0 through 249 inclusive) per cycle, so 250000 / 250 = 1000 interrupts per second.
The off-by-one — 249, not 250 — is the single most common bug in AVR timer code,
and it comes from the counter including zero in its cycle.

Why /64 specifically? It's the smallest prescaler that lets a 1 ms period fit in an
8-bit `OCR0A`. With /8 you'd need `OCR0A = 1999`, which doesn't fit. With /256
you'd need 61.5 — non-integral, so your tick would drift by about 0.8%, which over
a 1400 ms slots animation is ~11 ms of error. /64 gives an exact integer, hence a
tick with *zero* accumulated drift relative to the crystal. That exactness is the
whole reason this design can get away with treating `g_ms_tick` as wall-clock
time.

The ISR itself is a single increment:

```c
ISR(TIMER0_COMPA_vect) { g_ms_tick++; }
```

That is the right amount of work for a 1 kHz interrupt. Even so, it isn't free:
avr-gcc's ISR prologue pushes `SREG` and every call-clobbered register it touches,
and a 32-bit increment on an 8-bit core is four `adc`/`inc` instructions plus four
loads and four stores. Expect somewhere in the region of 40–60 cycles, or roughly
0.3% CPU load. Fine. But it's worth knowing that making `g_ms_tick` a `uint16_t`
would roughly halve that, at the cost of wrapping every 65.5 seconds.

> **Compute this before looking it up.** With `OCR0A = 249` and a /64 prescaler at
> 16 MHz, how long does `g_ms_tick` take to overflow its 32 bits? Then find every
> place in the codebase that computes an elapsed time as `now - start`, and
> convince yourself whether that subtraction survives the wrap. (Hint: unsigned
> arithmetic is your friend here — but only if *both* operands are the same width.)

### `src/main.c`

| Signature | What it does | Peripheral / registers | Interrupt context |
|---|---|---|---|
| `ISR(TIMER0_COMPA_vect)` | Increments the global 1 ms tick counter | Timer0 compare-match A vector | **Yes** |
| `static void timer0_init(void)` | Configures Timer0 for a 1 ms CTC tick and enables COMPA | `TCCR0A`, `TCCR0B`, `OCR0A`, `TIMSK0` | No |
| `static void return_to_menu(void)` | Calls the active game's `_exit()`, silences the buzzer, sets `g_state = STATE_MENU`, redraws | None directly (via buzzer/LCD calls) | No |
| `int main(void)` | Runs init sequence, `sei()`, then the superloop: poll buttons, advance buzzer, dispatch to state handler, check credit exhaustion | `SREG` (I bit) via `sei()`; all others indirectly | No |

---

## The superloop and the division of labour

The loop body is short enough to quote in spirit: read one button event, advance
the buzzer state machine, check for the global "long-press B returns to menu"
gesture, dispatch to the current state's `update()` function, then check whether
credits hit zero and force a game-over transition.

The architectural principle here is that **ISRs capture events and timestamps; the
main loop makes all decisions and does all I/O.** The pin-change ISR never draws to
the LCD, never plays a sound, never changes `g_state`. It writes an enum into a
four-deep ring buffer and returns. The Timer0 ISR does nothing but count.
Everything with latency — LCD writes take milliseconds; `Lcd_Clear()` alone burns
2 ms of `_delay_ms()` plus busy-flag polling — happens where it can't block an
interrupt.

That discipline pays off in a specific, observable way. Consider what happens
during a slots spin. `slots_update()` is called thousands of times per second
while `s_state == SLOTS_SPINNING`, and almost every call does nothing but compare
`now - s_last_frame` against `FRAME_MS` and return. Every 80 ms it redraws three
glyphs and kicks off a tick sound. Meanwhile the buzzer's square wave is generated
entirely in hardware by Timer2 toggling OC2A — the CPU is not involved in
producing a single edge of it. The main loop's only job for sound is to notice,
once per iteration, that a note's duration has elapsed and stop the timer. This is
what "non-blocking" buys you: the reel animation, the sound, and button
responsiveness all coexist without a scheduler, an RTOS, or a single `_delay_ms()`
in application code.

The one place blocking *does* leak in is the LCD driver, and it's worth being
precise about it. `Lcd_WriteData()` calls `lcd_wait_busy()`, which polls the busy
flag with `_delay_us(1.0)` between edges, and each character write is two nibble
transfers with an E pulse each. A full 16-character row is therefore on the order
of a hundred microseconds to a millisecond of wall time. During a `Lcd_Clear()`
plus two-row redraw, `buttons_poll()` isn't running — but the ISR still is, so
presses are captured into the queue and drained a moment later. The four-slot
queue depth is defensible for two buttons and a human operator, and
`queue_push()` drops on full rather than blocking, which is the correct choice
inside an ISR.

> **Trace one specific path yourself.** Start at `main.c:125`, follow
> `buttons_poll()` through to the point where a `BTN_A_PRESS` reaches
> `slots_update()`, and count how many distinct variables that event touches on
> the way. Then find every single place `g_state` is written across the whole
> tree — there are more than you might expect, and they're not all in `ui.c`.

---

## Buttons: debouncing without a debounce timer

`buttons.c` is the most interesting file in the project because it solves a real
problem — mechanical contact bounce — using nothing but the millisecond tick and a
state struct.

`buttons_init()` does three things. It clears the DDRC bits for PC0/PC1 to make
them inputs, sets the corresponding PORTC bits to enable the internal pull-ups,
and unmasks PCINT8/PCINT9 in `PCMSK1` while enabling the PCINT1 group in `PCICR`.
That pull-up step is the one beginners skip: with the pin floating, an unpressed
button reads whatever charge happens to be on the trace, and you get phantom
interrupts at whatever rate ambient EMI happens to induce. Here the pull-up
defines the idle state as logic high, and the button pulls it to ground when
pressed — hence "active low", and hence the inverted test in the ISR:

```c
uint8_t a_down = ((pinc & (1U << BTN_A_PIN)) == 0U) ? 1U : 0U;
```

The ISR reads `PINC` — the *input* register, reflecting the actual pin voltage —
rather than `PORTC`, which is the output latch. Reading `PORTC` on an input pin
tells you the pull-up configuration, not the pin state. This is a distinction that
costs people hours.

Debouncing works by timestamp comparison. When an edge arrives, the ISR compares
`g_ms_tick` against the last accepted transition for that button; if fewer than
20 ms have passed, the edge is discarded. This is elegant because it costs nothing
when idle — no polling timer, no periodic sampling — and it's a good fit for
pin-change interrupts, which give you edges for free. Crucially, the ISR reads the
*level* from `PINC` rather than inferring direction from edge count, so if two
bounces arrive close enough together that the second is lost while the vector is
executing, the code still converges on the true pin state.

### Defect: stuck-pressed state on a fast release

When a transition is rejected, `s_btn[i].pressed` is left unchanged, which is
correct for spurious edges — but consider a genuine release that happens to land
within 20 ms of the accepted press (a very fast tap, or a switch that bounces on
release). The release edge is discarded, `pressed` stays 1, and because pin-change
interrupts only fire on *edges*, no further interrupt will ever arrive to correct
it. The pin sits high, the software believes the button is held, and 800 ms later
`buttons_poll()` dutifully emits a spurious `BTN_x_LONG` — which in any game state
means "return to menu". You'd experience this as the device occasionally kicking
you out of a game for no reason. The fix is either to re-sample the pin level in
`buttons_poll()` and reconcile, or to always update `last_change` on a rejected
edge so the filter keeps sliding.

### Long-press detection lives in the poll, correctly

A long press is defined by the *absence* of an event — the button staying down —
and there is no interrupt for "nothing happened". Polling is the natural mechanism
for a timeout, and putting it in the main loop keeps the ISR at fixed, short
duration.

### Defect: bare `cli()`/`sei()` instead of `ATOMIC_BLOCK`

```c
cli();
now = g_ms_tick;
/* ... */
sei();
```

Using bare `cli()`/`sei()` unconditionally *enables* interrupts on exit,
regardless of whether they were enabled on entry. Here it's benign because
`buttons_poll()` is only ever called from the superloop with interrupts on. But
it's a landmine: if you ever call `buttons_poll()` from inside another critical
section, or from an ISR, you've silently re-enabled interrupts in the middle of
code that assumed atomicity. The idiomatic AVR solution is `<util/atomic.h>`'s
`ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`, which saves and restores `SREG` rather than
assuming. It compiles to the same two instructions plus one push/pop, and it
composes.

### The central defect: unguarded 32-bit reads

The reason a critical section is needed at all is the heart of this whole subject:
`g_ms_tick` is a `uint32_t` on an 8-bit machine. Reading it takes four separate
load instructions. If the Timer0 ISR fires between the second and third load at
the exact moment the counter rolls from `0x0000FFFF` to `0x00010000`, you assemble
a value that was never actually in the variable — you get `0x0001FFFF`, roughly
65 seconds in the future. `volatile` does *not* fix this; `volatile` guarantees the
compiler won't cache or reorder the access, but it says nothing about atomicity.

`buttons_poll()` is guarded. **Nothing else is.** `buzzer_update()` reads
`g_ms_tick` bare. `slots_update()`, `coinflip_update()`, `dice_update()`, and
`higherlower_update()` all open with `uint32_t now = g_ms_tick;` with interrupts
live. `buzzer_tone()` computes `s_stop_time = g_ms_tick + ms` unguarded. Every one
of those is a genuine torn-read race. In practice the window is a few cycles out
of a million per second and the consequence is a single glitched animation frame
or a tone that plays for 65 seconds, so you may never see it on the bench — but it
*is* a bug, and it's the exact bug the user of a codebase like this should learn to
spot on sight.

> **Do the arithmetic.** A four-byte load on the AVR is four `lds` instructions.
> Given a 1 kHz interrupt and a 16 MHz clock, estimate the probability that any
> single unguarded read of `g_ms_tick` is torn. Then find the one carry boundary
> where a torn read produces the *largest* error, and work out what the slots
> animation would visibly do if it happened mid-spin.

### `src/buttons.c`

| Signature | What it does | Peripheral / registers | Interrupt context |
|---|---|---|---|
| `static void queue_push(btn_event_t e)` | Pushes an event into the 4-deep ring buffer; silently drops when full | None (SRAM only) | **Yes** — called from `PCINT1_vect` *and* from `buttons_poll()` under `cli()` |
| `void buttons_init(void)` | PC0/PC1 as inputs, enables internal pull-ups, unmasks PCINT8/9 and enables the PCINT1 group | `DDRC`, `PORTC`, `PCICR`, `PCMSK1` | No |
| `ISR(PCINT1_vect)` | Reads `PINC`, debounces both buttons against `g_ms_tick`, emits short-press events on release, mixes the timestamp into the RNG | `PINC`; pin-change interrupt 1 vector | **Yes** |
| `btn_event_t buttons_poll(void)` | Checks held buttons for the 800 ms long-press threshold, then drains one event from the queue | `SREG` via `cli()`/`sei()` | No |

---

## The buzzer: hardware waveform generation and a prescaler table

`buzzer_init()` sets PB3 as an output and writes
`TCCR2A = (1U << COM2A0) | (1U << WGM21)`. `WGM21` alone puts Timer2 in CTC mode.
`COM2A0` alone — that is, `COM2A1:0 = 0b01` — selects "toggle OC2A on compare
match". Together these mean: the counter counts to `OCR2A`, flips the physical PB3
pin, and resets. No CPU involvement whatsoever. `TCCR2B = 0x00` leaves the clock
source disconnected, so the timer is stopped and the buzzer silent until a tone is
requested. Starting and stopping a tone is literally writing the clock-select bits
into `TCCR2B` and back to zero.

Because the pin *toggles* rather than completing a full cycle per compare match,
the output frequency is half the compare rate:

```
f_out = F_CPU / (2 · prescaler · (OCR2A + 1))
```

rearranged in the code to `OCR2A = F_CPU / (2 · prescaler · f) - 1`. And since
`OCR2A` is 8 bits, each prescaler setting has a *minimum* frequency below which the
required `OCR2A` exceeds 255. That's what `select_prescaler_and_set_ocr()` is doing
— walking a threshold ladder to pick the smallest prescaler that still fits, which
maximises frequency resolution for the requested note.

### Defect: wrong /8 threshold

The thresholds in the code are 3815, 977, 488, 244, 122, and the /1024 fallback.
Each row should be four times the previous, since each prescaler step is x4.
Check: 122 → 244 → 488 → 977 → **3908**, not 3815. The true minimum for the /8
prescaler is `16000000 / (2 · 8 · 256) = 3906.25 Hz`. So any requested frequency
between 3815 and 3906 Hz selects /8, computes an `OCR2A` above 255, and hits the
clamp at line 92 — producing 3906 Hz instead of what you asked for. It's latent
rather than live, because the highest note in any SFX table is 1600 Hz, but it's
exactly the kind of magic number that should have been written as a
datasheet-derived expression. `F_CPU / (2UL * 8UL * 256UL)` would have been
self-correcting and self-documenting.

### The auto-stop pattern

`buzzer_tone()` records `s_stop_time = g_ms_tick + ms` and returns immediately.
`buzzer_update()`, called once per superloop iteration, compares
`g_ms_tick >= s_stop_time` and stops the timer when the deadline passes. Melody
playback layers on top: when a note ends and `s_melody != NULL`, the index advances
and the next note starts. A four-note win fanfare therefore plays over 390 ms while
the game logic runs unimpeded.

Two things about that deadline comparison are worth flagging. First,
`g_ms_tick >= s_stop_time` breaks at the 32-bit wraparound — the robust form is
`(int32_t)(g_ms_tick - s_stop_time) >= 0`, which handles wrap correctly because the
*difference* wraps consistently even when the absolute values don't. At 1 kHz the
wrap is 49.7 days away, so it's academic for a battery toy, but the habit matters.

Second, `s_stop_time` and `s_tone_active` are declared `volatile` even though
neither is ever touched from an ISR — they're written and read entirely from main
context. That's harmless but it costs you: `volatile` forces a reload from SRAM on
every access and blocks the optimiser from keeping them in registers. It signals
"shared with an interrupt" to a reader, which here is a false signal. Meanwhile the
variables that genuinely *are* shared across contexts — `rng.c`'s `s_state` —
aren't marked at all.

### Defect: stale melody pointer

`buzzer_stop()` clears `s_tone_active` but never clears `s_melody`. So if a melody
is interrupted mid-sequence — which happens constantly, because
`coinflip_update()` calls the raw `buzzer_tone()` for its rising-pitch ticks while
a `SFX_CONFIRM` melody may still be playing — the stale melody pointer and index
survive. When the raw tone's 25 ms elapse, `buzzer_update()` sees
`s_melody != NULL`, advances the index, and resumes playing the *previous*
melody's remaining notes. You'd hear the tail end of a confirmation chime spliced
into the middle of a coin flip. Clearing `s_melody = NULL` inside `buzzer_stop()`
fixes it, though you'd then need `buzzer_play_melody()` to set it *after* its
internal `buzzer_tone()` call rather than before.

> **Predict, then verify.** Open `buzzer.c` and work out, by hand, the exact value
> written to `OCR2A` and to `TCCR2B` for the 1400 Hz note at the end of `SFX_WIN`.
> Then check your `OCR2A` answer against the real output frequency — how far off
> 1400 Hz is the square wave the piezo actually sees, and why?

### `src/buzzer.c`

| Signature | What it does | Peripheral / registers | Interrupt context |
|---|---|---|---|
| `static uint8_t select_prescaler_and_set_ocr(uint16_t freq_hz)` | Picks the smallest Timer2 prescaler keeping `OCR2A <= 255`, writes `OCR2A`, returns the CS bits | `OCR2A`; computes `TCCR2B` CS22:20 | No |
| `void buzzer_init(void)` | PB3 as output; Timer2 to CTC + toggle-OC2A, clock stopped | `DDRB`, `TCCR2A`, `TCCR2B`, `TCNT2`, `OCR2A` | No |
| `void buzzer_stop(void)` | Stops Timer2, detaches OC2A, drives PB3 low, restores toggle mode | `TCCR2B`, `TCNT2`, `TCCR2A`, `PORTB` | No |
| `void buzzer_tone(uint16_t hz, uint16_t ms)` | Starts a non-blocking tone and records its stop deadline | `TCNT2`, `TCCR2B`, `OCR2A` (via helper) | No |
| `void buzzer_play_melody(const note_t *notes, uint8_t len)` | Latches a PROGMEM note array and starts note 0 | Flash via `pgm_read_word`; Timer2 indirectly | No |
| `void buzzer_update(void)` | Per-iteration: stops an expired tone, advances melody playback | `TCCR2B` etc. via `buzzer_stop`/`buzzer_tone` | No |

---

## The RNG, and why its seed doesn't work

`rng.c` implements a 16-bit xorshift — three shift-and-XOR steps with shift
constants 13, 9, 7, which is one of Marsaglia's published triples for a full period
of 2^16 − 1. It's a genuinely good choice for this application: four instructions
per step, no multiply (the 328P has one, but still), no division, no table. The
state must never be zero, since xorshift maps zero to zero forever, and the code
guards that in three separate places.

The seeding is where it falls apart, and the failure is a beautiful illustration of
why you have to reason about *hardware* state and not just code.

```c
ADMUX  = (1U << REFS0) | (1U << ADLAR);   /* AVCC ref, left-adjust, MUX=0000 → ADC0 */
ADCSRA = (1U << ADEN) | (1U << ADPS2) | (1U << ADPS1) | (1U << ADPS0);
```

`REFS0` alone selects AVCC as the reference. `ADLAR` left-aligns the 10-bit result
so the top 8 bits land in `ADCH` and you can read a single byte — a nice trick that
avoids the 16-bit `ADC` read and its required low-byte-first ordering.
`ADPS2:0 = 0b111` is a /128 prescaler, giving a 125 kHz ADC clock, which sits
inside the datasheet's 50–200 kHz window for full 10-bit accuracy. All correct. The
busy-wait on `ADSC` is fine because this runs once, before `sei()`, and takes about
25 ADC clocks — 200 µs.

### Defect: the "floating pin" is not floating

`MUX3:0 = 0000` selects **ADC0 — which is PC0 — which is BTN_A**. And `rng_init()`
is called *after* `buttons_init()`, which has just enabled the internal pull-up on
PC0. So the "floating pin" the comment describes is not floating at all: it's tied
to VCC through a 20–50 kΩ pull-up, with a button to ground across it. The
conversion returns 0xFF every single time the button is up, or 0x00 if you happen
to be holding it at power-on. `s_state ^= 0xFF` turns `0xACE1` into `0xAC1E` —
deterministically, every boot.

The practical consequence is that the entropy claim in the header is false. What
actually saves the device is that the splash screen requires a button press before
anything happens, and `PCINT1_vect` calls `rng_reseed_mix(g_ms_tick)` on *every*
edge. Human reaction time supplies real entropy through the timestamp; the ADC
contributes nothing. The fix is one of: move `rng_init()` before `buttons_init()`,
point the mux at a genuinely unconnected ADC channel (ADC4/ADC5 are free here), or
— most robust — sample the internal bandgap or temperature sensor and mix several
conversions. While you're in there, note that `DIDR0` is never written, so the
digital input buffer on the analogue pin stays enabled, which wastes a little
current and is the standard thing to disable on ADC pins.

### Defect: `s_state` races with the ISR

`rng_reseed_mix()` is called **from inside `PCINT1_vect`**, while `rng_next()` and
`rng_u8()` are called from game code in main context. `s_state` is a plain
`static uint16_t` — not volatile, not protected. A 16-bit read-modify-write on an
8-bit core is not atomic, and this is a *sequence* of them. If a button interrupt
lands in the middle of `rng_next()`'s three-step shuffle, the mixed-in entropy is
silently discarded when the main-context code writes back its stale intermediate.
Worse, the compiler is free to keep `s_state` in a register across the whole
function precisely because it isn't volatile. The bug is benign here — the failure
mode of a corrupted PRNG is "different random numbers", which is what you wanted
anyway — but it's the exact structural pattern that would be catastrophic if
`s_state` were a linked list head or a buffer index.

> **Read `rng_init()` and predict the value in `ADCH` after the conversion
> completes.** Then check whether that prediction changes if you swap the order of
> `buttons_init()` and `rng_init()` in `main()`. Which pin would you point the mux
> at instead, and what would you need to check on the schematic first?

### `src/rng.c`

| Signature | What it does | Peripheral / registers | Interrupt context |
|---|---|---|---|
| `void rng_init(void)` | One blocking ADC0 conversion, XORs `ADCH` into the state, disables the ADC | `ADMUX`, `ADCSRA`, `ADCH` | No |
| `void rng_reseed_mix(uint16_t val)` | XORs a value in and avalanches it through a shift chain | None (SRAM only) | **Yes** — called from `PCINT1_vect` |
| `uint16_t rng_next(void)` | One 16-bit xorshift step (13, 9, 7) | None | No |
| `uint8_t rng_u8(uint8_t range)` | `rng_next() % range` — biased, acknowledged in comment | None | No |

---

## The UI layer: state machine, CGRAM, and a column-counting problem

`ui.c` owns the two globals the whole system revolves around: `g_state` and
`g_credits`. Both are declared here despite `ui.h`'s comment claiming they're
"defined in main.c" — a stale comment, not a bug, but the kind of thing that sends
you looking in the wrong file.

The most interesting hardware work in this file is `ui_load_cgram()`. The HD44780's
character generator RAM holds eight user-defined 5x8 glyphs at character codes 0–7.
Each glyph is eight bytes, one per pixel row, with only bits 4:0 used. The project
defines cherry, bell, seven, two coin faces, two arrows, and a dice frame, all
stored in flash via `PROGMEM` — 64 bytes of glyph data that would otherwise eat 3%
of the chip's 2 KB of SRAM for no reason.

But look at the copy loop:

```c
for (r = 0U; r < 8U; r++)
    row_buf[r] = pgm_read_byte(&GLYPH_TABLE[slot][r]);
Lcd_WriteCustomChar(slot, row_buf);
```

The pattern arrays are in flash, so they need `pgm_read_byte` — a `lpm`
instruction — because the AVR is a Harvard architecture where a plain pointer
dereference reads *data* space and would return whatever SRAM happens to live at
that numeric address. This is the single most common source of "my strings print as
garbage" on AVR. The copy into a stack buffer exists because
`Lcd_WriteCustomChar()` takes a RAM pointer, which is the right API boundary for a
general-purpose driver.

### Observation: pointer tables and strings are in SRAM

`GLYPH_TABLE` itself is *not* in PROGMEM. It's an array of eight pointers-to-flash
living in SRAM — 16 bytes of `.data`, plus 16 bytes of flash for the initialisers.
Small, but symptomatic: the same pattern repeats for `MENU_ITEMS`, `CARD_NAMES`,
and `TARGET_NAMES`, and it's why the linker map shows 568 bytes of `.data`.
Combined with 90 bytes of `.bss`, that's 658 of 2048 bytes of SRAM consumed before
the stack even starts — about a third. On a project with more strings this is
precisely how you run out of RAM and get mysterious stack corruption. `PROGMEM`
plus `pgm_read_word` for the pointer table, or the `PSTR`/`pgm_read_byte` idiom for
the strings, would recover most of it.

Build sizes for reference: `text 8722, data 568, bss 90` — about 9.3 KB of 32 KB
flash and 658 bytes of 2 KB SRAM. `snprintf` alone (pulled in by `slots.c`,
`dice.c`, and `higherlower.c`) links the full `vfprintf` machinery.

### Defect: rows that overflow 16 columns

`ui_draw_menu()` writes `">"`, a space, the menu item, pads to column 9, then
`"Cr:"` and four digits — total 16. That works for `"Slots"` and `"Dice"`. But
`"Coin Flip"` is nine characters, so the item alone reaches column 10, the
`while (col < 9)` padding loop does nothing, and the last two digits of the credit
count land at DDRAM addresses 0x10 and 0x11 — off-screen. On an HD44780, row 0's
DDRAM runs 0x00–0x27 (40 characters) even though only 16 are visible, so the
overflow doesn't wrap to row 1; it just silently vanishes. `"Hi or Lo"` at eight
characters has the same problem by one column.

The same arithmetic error appears in the games:

| Location | Row content | Width |
|---|---|---|
| `ui.c:ui_draw_menu` (Coin Flip) | `"> Coin Flip"` + `"Cr:"` + 4 digits | 18 |
| `slots.c:draw_result_row` | `"JACKPOT!   Cr:"` + 4 digits | 18 |
| `slots.c:draw_result_row` | `"No luck    Cr:"` + 4 digits | 18 |
| `dice.c:draw_idle` | `"Tgt:"` + 7 + `" Cr:"` + 4 digits | 19 |
| `dice.c:draw_result` | `"No match   Cr:"` + 4 digits | 18 |
| `higherlower.c:draw_guess` | `"Str:03 +030 "` + `"Cr:"` + 4 digits | 19 |
| `coinflip.c:draw_idle` | `"Bet:10   Cr:"` + 4 digits | **16 — correct** |

Of every status row in the project, `coinflip.c`'s is the only one that lands on
exactly 16.

> **Verify this with a pencil, not the simulator.** Count the characters emitted by
> `slots.c:draw_status()` and by `dice.c:draw_idle()`. Then find the one status row
> in the whole project that fits in 16 columns exactly. Once you've found it,
> decide where the fix belongs: in each drawing function, or in a shared helper
> that formats a right-aligned credit field?

### `src/ui.c`

| Signature | What it does | Peripheral / registers | Interrupt context |
|---|---|---|---|
| `void ui_load_cgram(void)` | Copies 8 PROGMEM glyph patterns into LCD CGRAM slots 0–7 | Flash via `pgm_read_byte`; LCD CGRAM | No |
| `void ui_draw_centered(uint8_t row, const char *s)` | Centres a string in a 16-char space-padded buffer and writes it | LCD DDRAM via driver | No |
| `void ui_show_score(uint16_t score)` | Writes a 4-digit zero-padded number at the cursor | LCD DDRAM | No |
| `void ui_draw_splash(void)` | Clears and draws the two-line splash screen | LCD | No |
| `void ui_draw_menu(void)` | Draws the selected item with `>` cursor, the next item below, and the credit count | LCD | No |
| `void ui_menu_update(btn_event_t e)` | Handles SPLASH→MENU, BTN_B cycles selection, BTN_A enters the chosen game | None directly; sets `g_state` | No |
| `void ui_draw_gameover(void)` | Draws the GAME OVER screen | LCD | No |
| `void ui_gameover_update(btn_event_t e)` | On BTN_A resets credits to 100 and returns to MENU | None; sets `g_state`, `g_credits` | No |

---

## The games: four variations on one non-blocking pattern

All four games implement the same three-function contract — `_enter()`,
`_update(btn_event_t)`, `_exit()` — and all four use the identical animation idiom:
record a start timestamp, compute `elapsed_frames = (now - start) / FRAME_MS`, and
act only when that value changes. This is worth naming explicitly, because it's the
alternative to `_delay_ms()` that makes a cooperative superloop work.
`_delay_ms(80)` inside a game's update function would freeze button polling, freeze
the buzzer's auto-stop, and freeze every other game's timing for the duration. The
frame-counter idiom achieves the same visual result while the loop keeps spinning.

### Slots

The most elaborate: three reels with staggered stop times at 800, 1100, and
1400 ms, an 80 ms frame rate, and a payout ladder. One thing to notice is that the
*outcome is decided at spin start*, not at reel stop — `s_final[i] =
rng_u8(REEL_SYMBOLS)` runs the moment BTN_A is pressed, and the animation is pure
theatre that eventually snaps each reel to its predetermined value. That's how real
slot machines work too, and it's the right call here because it decouples the RNG
from animation timing.

There's a small piece of dead logic: `any_change` is set to `1U` at the top of the
same block that later tests `if (any_change)`, so the condition is always true.
Harmless, but it suggests the block was refactored and the guard wasn't cleaned up.

### Dice

Does something genuinely clever with the display. Rather than defining six separate
pip glyphs, it repeatedly *rewrites CGRAM slot 7 in place* during the roll —
`Lcd_WriteCustomChar(GLYPH_DICE_PIP, ANIM_PATTERNS[...])`. Because the LCD
controller renders any on-screen character code 7 from whatever is currently in
that CGRAM slot, changing the slot instantly changes every displayed instance. Two
characters on screen, one glyph definition, four frames of tumbling animation, zero
extra DDRAM writes.

Note the comment at `dice.c:67` acknowledging that `Lcd_WriteCustomChar` resets the
DDRAM address counter to 0x00 on exit — because the driver issues a
`Set DDRAM Address` command at the end — so the cursor must be repositioned
afterwards. `dice.c` handles this correctly. It also restores the default pip
pattern when the roll ends, which matters because `slots.c` uses the same CGRAM
slot 7 as one of its five reel symbols; without the restore, playing dice then
slots would show a tumbling pattern on the reel. (The restore pattern in `dice.c`
and the original in `ui.c` are byte-identical — verified.)

### Coin flip

The simplest and, notably, the only game whose status row fits the display exactly.
It uses the raw `buzzer_tone()` rather than a melody for its rising-pitch ticks
(400 Hz climbing by 50 Hz per frame), which is what exposes the stale-`s_melody`
bug described earlier.

### Higher-or-lower

Has a structural problem that `docs/state_machine.md` confirms is intentional but
which is still worth flagging: **it never deducts credits.** A correct guess adds
10; a wrong guess resets the streak and does nothing else. That has two
consequences. First, it's a pure credit faucet — you can farm unlimited credits by
guessing repeatedly. Second, and more interesting architecturally, `g_credits` can
never reach zero while in this state, so the game-over check in `main()` is
unreachable from `STATE_HIGHERLOWER` no matter how badly you play. The
`HIGHERLOWER --> GAMEOVER : credits == 0` edge in the state diagram is dead. If the
intent was a risk-free bonus round, fine, but then the diagram shouldn't claim that
transition exists.

It also has an unused variable: `s_last_correct` is written at line 141 and never
read.

> **Pick one game and instrument it.** In `dice.c`, `update_dice_cgram()` is called
> once per 100 ms frame during a 700 ms roll. Work out how many bytes cross the
> 4-bit LCD bus per call (remember: one `Set CGRAM Address` command plus eight data
> bytes, each transferred as two nibbles with a busy-flag poll between), and
> estimate how much of each 100 ms frame the CPU actually spends inside the LCD
> driver. Then decide whether that changes your view of where the "blocking" in
> this system really lives.

### `src/games/slots.c`

| Signature | What it does | Peripheral / registers | Interrupt context |
|---|---|---|---|
| `static void draw_reels(void)` | Writes the three reel glyphs and clears the rest of row 0 | LCD DDRAM | No |
| `static void draw_status(void)` | Writes `"Bet:N Cr:"` plus the score on row 1 | LCD DDRAM | No |
| `static void draw_result_row(uint8_t won, uint16_t payout, uint8_t is_jackpot)` | Writes the win/lose/jackpot result line (overflows 16 columns) | LCD DDRAM | No |
| `void slots_enter(void)` | Resets sub-state, bet index and reels; clears and draws the screen | LCD | No |
| `void slots_update(btn_event_t e)` | Sub-FSM: bet cycling, spin start, 80 ms animation with staggered stops, payout evaluation | Reads `g_ms_tick`; LCD; buzzer; RNG | No |
| `void slots_exit(void)` | No-op | None | No |

### `src/games/coinflip.c`

| Signature | What it does | Peripheral / registers | Interrupt context |
|---|---|---|---|
| `static void draw_idle(void)` | Draws the call prompt and the (correctly sized) status row | LCD DDRAM | No |
| `static void draw_flip_frame(uint8_t heads_showing)` | Draws one animation frame with the heads or tails glyph | LCD DDRAM | No |
| `static void draw_result(uint8_t won)` | Draws the revealed face and the win/lose line | LCD DDRAM | No |
| `void coinflip_enter(void)` | Resets to IDLE with a HEADS call, clears and draws | LCD | No |
| `void coinflip_update(btn_event_t e)` | Sub-FSM: toggle call, deduct 10 credits, 600 ms flip with rising-pitch ticks, reveal | Reads `g_ms_tick`; LCD; `buzzer_tone`; RNG | No |
| `void coinflip_exit(void)` | No-op | None | No |

### `src/games/higherlower.c`

| Signature | What it does | Peripheral / registers | Interrupt context |
|---|---|---|---|
| `static void draw_guess(void)` | Draws the current card and the streak/credits row (overflows 16 columns) | LCD DDRAM | No |
| `static void draw_result_screen(uint8_t correct)` | Draws current→next card and the correct/wrong verdict | LCD DDRAM | No |
| `void higherlower_enter(void)` | Resets the streak, draws a random first card | LCD; RNG | No |
| `void higherlower_update(btn_event_t e)` | Sub-FSM: evaluate the guess (equal loses), award +10, 1 s auto-advance | Reads `g_ms_tick`; LCD; buzzer; RNG | No |
| `void higherlower_exit(void)` | No-op | None | No |

### `src/games/dice.c`

| Signature | What it does | Peripheral / registers | Interrupt context |
|---|---|---|---|
| `static void update_dice_cgram(uint8_t anim_idx)` | Rewrites CGRAM slot 7 in place to animate the tumbling pips | LCD CGRAM | No |
| `static void write_dice_row0(void)` | Writes `"[d1] [d2]  Sum:NN "` as ASCII on row 0 | LCD DDRAM | No |
| `static void draw_idle(void)` | Draws dice values and the target/credits row (overflows 16 columns) | LCD DDRAM | No |
| `static void draw_rolling(uint8_t anim_idx)` | Draws two slot-7 glyphs and the "Rolling" label | LCD DDRAM | No |
| `static void draw_result(uint8_t won, uint16_t payout)` | Draws final dice and the payout line | LCD DDRAM | No |
| `void dice_enter(void)` | Resets to IDLE with target OVER 7 and dice showing 1,1 | LCD | No |
| `void dice_update(btn_event_t e)` | Sub-FSM: cycle target, deduct 5, 700 ms roll animating CGRAM, evaluate against target, restore default pip | Reads `g_ms_tick`; LCD CGRAM/DDRAM; buzzer; RNG | No |
| `void dice_exit(void)` | No-op | None | No |

---

## The LCD driver: bit-banging a parallel bus correctly

The driver is external to this project and unmodified, but it's the largest single
consumer of CPU time so it's worth understanding.

The 4-bit protocol is: put the high nibble on PD7:PD4, pulse E, put the low nibble
on PD7:PD4, pulse E. `lcd_pulse_enable()` raises E, waits 1 µs, lowers it, waits
another 1 µs. The datasheet requires E high for at least 140 ns and a full cycle
time of at least 1200 ns, so 1 µs each way is comfortably conservative — and
deliberately so, because `_delay_us()` with a floating-point argument gets
constant-folded by the compiler into a cycle-accurate `__builtin_avr_delay_cycles`,
which means the timing is exact but *only* if interrupts don't fire. They do, here,
at 1 kHz. An interrupt landing mid-pulse stretches E high by the ISR's duration.
That's safe for the HD44780 — there's a minimum E pulse width but no maximum — but
it's the kind of thing that bites you on a peripheral with a *maximum* timing
constraint, such as a bit-banged SPI or a one-wire bus.

`lcd_wait_busy()` is the key to the driver's performance. After initialisation,
instead of blindly delaying the datasheet's worst-case execution time after every
command, the driver reads back the busy flag (DB7, with RS=0 and R/W=1) and
proceeds as soon as it clears. Since most commands take 39 µs and only Clear/Home
take 1.53 ms, this is a large win over fixed delays. The timeout constant is
derived rather than guessed: 20000 iterations at roughly 375 ns each ≈ 7.5 ms,
safely above the 1.53 ms worst case. This is exactly the kind of
magic-number-with-a-derivation the rest of the project should aspire to.

Note the bus-direction dance inside it: `LCD_DATA_AS_INPUT()` then
`LCD_DATA_WRITE(0x00)` to *disable* the pull-ups on PD7:PD4 before reading. On AVR,
writing to `PORTx` while `DDRx` is 0 controls the pull-up, not the output level.
Leaving pull-ups on while an external device drives the line creates contention and
skewed readings. And the function is documented as leaving the bus configured as
output with R/W low on return — a stated post-condition that every caller relies
on.

One structural observation: reading the busy flag requires the R/W line, which is
why PB1 is wired to the LCD rather than tied to ground. The README's wiring diagram
even hedges — "GND ── VSS, R/W when not used for reads". If you tied R/W low to
free PB1, you'd lose busy-flag polling and have to switch to fixed delays, but
you'd gain Timer1's OC1A for the buzzer and get a 16-bit counter for tone
generation. That's a genuine, interesting design tradeoff sitting right at the
centre of this project.

### `extern_libraries/lcd_driver/lcd_driver.c`

| Signature | What it does | Peripheral / registers | Interrupt context |
|---|---|---|---|
| `static void lcd_pulse_enable(void)` | Raises E, waits >=140 ns, lowers it, waits for hold/cycle time | `PORTB` (E pin) | No |
| `static lcd_status_t lcd_wait_busy(void)` | Polls DB7 via two nibble reads until BF clears or ~7.5 ms timeout; restores bus to write mode | `DDRD`, `PORTD`, `PIND`, `PORTB` (RS/RW/E) | No |
| `static void lcd_write_nibble_raw(uint8_t nibble)` | Sends one nibble with no BF check — init sequence only | `PORTD`, `PORTB` | No |
| `lcd_status_t Lcd_Init(void)` | Full power-on 4-bit init: 50 ms wait, three 0x3 nibbles, 0x2, then Function Set / Display Off / Clear / Entry Mode / Display On | `DDRB`, `DDRD`, `PORTB`, `PORTD` | No |
| `lcd_status_t Lcd_Clear(void)` | Clear Display command plus the required 2 ms settle | LCD IR | No |
| `lcd_status_t Lcd_ReturnHome(void)` | Return Home command plus 2 ms settle | LCD IR | No |
| `lcd_status_t Lcd_SetCursor(uint8_t col, uint8_t row)` | Bounds-checks, maps row to DDRAM base (0x00 / 0x40), issues Set DDRAM Address | LCD IR | No |
| `lcd_status_t Lcd_WriteChar(uint8_t ch)` | Thin wrapper over `Lcd_WriteData` | LCD DR | No |
| `lcd_status_t Lcd_WriteString(const char *str)` | Writes a NUL-terminated RAM string, stopping on first error | LCD DR | No |
| `lcd_status_t Lcd_DisplayControl(uint8_t display, uint8_t cursor, uint8_t blink)` | Builds and sends the Display ON/OFF Control command | LCD IR | No |
| `lcd_status_t Lcd_EntryModeSet(uint8_t increment, uint8_t shift)` | Sets cursor direction and display-shift behaviour | LCD IR | No |
| `lcd_status_t Lcd_CursorShift(uint8_t shiftDisplay, uint8_t shiftRight)` | Moves cursor or shifts the display without touching DDRAM | LCD IR | No |
| `lcd_status_t Lcd_WriteCustomChar(uint8_t slot, const uint8_t *pattern)` | Writes 8 pattern bytes to a CGRAM slot, then resets AC to DDRAM 0x00 | LCD CGRAM + IR | No |
| `lcd_status_t Lcd_WriteCommand(uint8_t cmd)` | Waits for BF, then sends a command as two nibbles with RS=0 | `PORTB`, `PORTD` | No |
| `lcd_status_t Lcd_WriteData(uint8_t data)` | Waits for BF, then sends a data byte as two nibbles with RS=1 | `PORTB`, `PORTD` | No |
| `uint8_t Lcd_ReadData(void)` | Reads a byte back from DDRAM/CGRAM as two nibbles and reassembles it | `DDRD`, `PIND`, `PORTB` | No |
| `lcd_status_t Lcd_ReadBusyFlag(uint8_t *busy, uint8_t *address)` | Public BF + address-counter read (note: does *not* pre-wait) | `DDRD`, `PIND`, `PORTB` | No |

---

## Fuses

The Makefile burns `LFUSE = 0xF7`, `HFUSE = 0xD9`, `EFUSE = 0x07`.

**`LFUSE = 0xF7`** decodes as CKDIV8 unprogrammed (bit 7 = 1), CKOUT unprogrammed,
`SUT1:0 = 11`, `CKSEL3:0 = 0111`. `CKSEL3:1 = 011` selects the **full-swing crystal
oscillator**, and `CKSEL0 = 1` with `SUT = 11` gives a 16K-cycle plus 65 ms startup
delay — the most conservative option, appropriate for a slowly rising supply. The
CKDIV8 bit is the one to fixate on: it ships *programmed* from the factory,
dividing the clock by 8. If you flash this firmware without burning fuses, the chip
runs at 2 MHz while `F_CPU` still says 16000000, and every `_delay_us()` becomes
eight times longer, the tick becomes 8 ms, and the LCD init sequence violates its
own timing. `make fuses` before `make flash`, once, is not optional.

Full-swing versus low-power crystal is a real choice: full-swing drives the crystal
harder, tolerates noise and longer traces better, and costs a milliamp or two. For
a battery-powered pocket device, low-power (`CKSEL3:1 = 1xx`) would have been the
more natural pick.

**`HFUSE = 0xD9`** leaves reset enabled, debugWIRE off, SPI programming enabled,
the watchdog not force-on, and BOOTRST unprogrammed so reset vectors to 0x0000.
Bit 3 — EESAVE — is **unprogrammed**, meaning EEPROM is erased on every chip erase.
The README's fuse table claims "EESAVE on", which is wrong; the Makefile's own
`set_eeprom_save_fuse` target proves it by setting HFUSE to 0xD7 to actually enable
it. That matters the moment you implement the "EEPROM high scores" stretch goal,
because your scores would evaporate on every reflash.

**`EFUSE = 0x07`** sets `BODLEVEL = 111`, which **disables brown-out detection
entirely**. For a device on a regulated bench supply this is fine. For a
battery-powered toy it's the fuse to change first. As the battery sags, the CPU can
execute garbage instructions well below the voltage at which flash and EEPROM
writes become unreliable, and the classic symptom is corrupted memory rather than a
clean reset. `BODLEVEL = 110` (2.7 V) or `101` (4.3 V, given 16 MHz needs >=4.5 V
for full spec) would be the defensive choice.

> **Decode `HFUSE = 0xD9` yourself** against Table 28-9 in the ATmega328P datasheet,
> bit by bit, and confirm the EESAVE claim. Then work out what value you'd need for
> BOD at 4.3 V, and check it against `set_eeprom_save_fuse` in the Makefile to make
> sure the two changes don't conflict.

---

## The three things most worth understanding deeply

**1. Atomicity is not the same as `volatile`, and multi-byte variables shared with
an ISR need both.** This is the single most transferable idea in the project, and
it's the one the code gets half-right. `g_ms_tick` is correctly `volatile`, which
stops the compiler caching it in a register across a loop — without that,
`while (g_ms_tick < deadline);` would compile to an infinite loop, because the
compiler can prove nothing in the loop body modifies it. But `volatile` does nothing
about the four separate `lds` instructions needed to read 32 bits on an 8-bit core.
`buttons_poll()` guards its read; `buzzer_update()` and all four games do not.
Understanding exactly why one is safe and the others aren't — and being able to
point at the specific carry boundary where a torn read produces a wrong answer — is
the difference between writing firmware that works on the bench and firmware that
works.

**2. Hardware peripherals replace CPU work, and choosing which peripheral to use is
usually a pin-allocation problem.** The buzzer produces a square wave with zero CPU
involvement because Timer2's compare-match unit toggles the pin in hardware. The
main loop's only job is to start and stop the clock. But Timer2 was chosen not
because it was best — Timer1's 16 bits would have made frequency selection trivial
— but because Timer1's output pin was already taken by the LCD's R/W line. That
kind of reasoning, where an architectural decision is forced by a pin conflict
three modules away, is most of what bare-metal design actually consists of.
`hardware_connections.h` exists precisely to make those conflicts visible in one
place, which is why the file is worth more than its 60 lines suggest.

**3. Cooperative multitasking via deadline comparison is what lets a superloop feel
responsive.** Every timed behaviour in this project — debounce, long-press, tone
duration, melody advance, reel stops, coin-flip frames, dice tumble, the 1 s result
hold — is expressed as "has `now - start` exceeded a threshold?", checked on every
pass through a loop that never blocks. There is no `_delay_ms()` anywhere in the
application code, only in the LCD driver where the hardware genuinely demands it.
Once you see this pattern, you can build surprisingly complex concurrent behaviour
without an RTOS, and you'll recognise immediately why a single stray
`_delay_ms(500)` in a game handler would break the buzzer, the buttons, and the
animation all at once.

---

## A modification that tests whether you actually understand these

**Add a 2-second splash timeout — the one the docs claim exists but the code
doesn't implement.**

`ui.h` documents `SPLASH → MENU (on any button press or after 2 s timeout)`, and
`docs/state_machine.md` shows only the button-press edge. `ui_menu_update()`
currently returns immediately unless `e != BTN_NONE`, so the splash screen waits
forever. Implement the timeout.

It looks like four lines, and it will force you to confront all three concepts at
once:

- You'll need a timestamp for when the splash was drawn — which means deciding
  where it lives and who owns it, since `ui_draw_splash()` is called from `main()`
  after `sei()` and the state variable is in `ui.c`.
- You'll need to read `g_ms_tick` to compare against it, which means deciding
  whether to guard that read and, if so, with what: bare `cli()`/`sei()` like
  `buttons_poll()` uses, or `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)` from
  `<util/atomic.h>`.
- You'll need to make sure the comparison survives the same wraparound question
  that applies to `buzzer_update()`.
- And you'll need to verify it doesn't fight the existing button-press path — if a
  press arrives at 1999 ms, exactly one transition should occur, not two.

Then, having done it once, go back and apply the same reasoning to
`buzzer_update()`'s unguarded read. If your splash timeout is correct and
`buzzer_update()` still isn't, you'll know exactly why — and that's the point.

Before you start, though, fix `LIBDIR` in the Makefile so you can actually build
and flash it.

---

## Appendix: defect summary

Ordered roughly by how much they'd bite you in practice.

| # | Location | Defect | Severity |
|---|---|---|---|
| 1 | `Makefile:15` | `LIBDIR = ../extern_libraries` resolves to a non-existent path; `make` fails | **Blocking** |
| 2 | `Makefile:58,78` | `HEADERS` includes non-existent `src/main.h`; silently disables the pattern rule | **Blocking / silent** |
| 3 | `rng.c:28` + `main.c:111-112` | ADC0 is PC0 with the pull-up already enabled by `buttons_init()`; seed is deterministic (`0xAC1E`) every boot | High |
| 4 | `buzzer.c`, all game `*_update()` | Unguarded 32-bit reads of `g_ms_tick` shared with `TIMER0_COMPA_vect` — torn-read race | High (rare) |
| 5 | `buzzer.c:120-134` | `buzzer_stop()` doesn't clear `s_melody`; interrupted melodies resume spliced into later tones | Medium (audible) |
| 6 | `buttons.c:110-113` | Bounce-rejected release leaves `pressed = 1` with no further edge to correct it → spurious `BTN_x_LONG` | Medium |
| 7 | `rng.c:20` | `s_state` written from ISR and main context, neither `volatile` nor guarded | Medium |
| 8 | `ui.c`, `slots.c`, `dice.c`, `higherlower.c` | Status rows exceed 16 columns (18–19 chars); trailing characters land in invisible DDRAM | Medium (visible) |
| 9 | `higherlower.c` | Never deducts credits — infinite credit faucet; makes the `→ GAMEOVER` edge unreachable | Medium (design) |
| 10 | `buzzer.c:76` | `/8` prescaler threshold is 3815 Hz; should be 3907 Hz. Latent — max SFX note is 1600 Hz | Low |
| 11 | `Makefile:169` | `EFUSE = 0x07` disables brown-out detection on a battery device | Low (design) |
| 12 | `README.md:58` | Claims `HFUSE 0xD9` has "EESAVE on"; bit 3 is unprogrammed, so EEPROM is erased on reflash | Low (doc) |
| 13 | `buzzer.c:50-51` | `s_stop_time` / `s_tone_active` marked `volatile` but never touched by an ISR | Low |
| 14 | `ui.c:132`, `MENU_ITEMS`, `CARD_NAMES`, `TARGET_NAMES` | Pointer tables and strings in SRAM rather than PROGMEM — 568 bytes of `.data` | Low |
| 15 | `ui.h:25,40` | Comments claim `g_credits` / `g_state` are "defined in main.c"; they're in `ui.c` | Cosmetic |
| 16 | `slots.c:167,197` | `any_change` is always true — dead condition | Cosmetic |
| 17 | `higherlower.c:141` | `s_last_correct` written, never read | Cosmetic |
| 18 | `rng.c` | `DIDR0` never written; digital input buffer left enabled on the ADC pin | Cosmetic |
| 19 | `Makefile:9` | `BAUD` defined and passed as `-DBAUD`, but no USART code exists in the tree | Cosmetic |
