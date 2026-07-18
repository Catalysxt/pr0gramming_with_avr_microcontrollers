# Theory

## Why a MAX7219 at all — the multiplexing problem

Eight 7-segment digits have 8 × 8 = 64 LEDs. Driving them directly would need
64 GPIOs and 64 current-limiting resistors. The MAX7219 solves this with
**time-multiplexed common-cathode scanning**: the 8 segment anodes (A–G, DP) are
tied in parallel across all digits, and the chip pulls exactly one digit's
common cathode low at a time, cycling through all 8 fast enough (~800 Hz per
digit) that persistence of vision fuses them into a steady display. So 8 digits
cost the chip just 8 SEG + 8 DIG pins, one external resistor (`R_SET`) sets the
peak segment current for *all* LEDs, and the host talks to it over 3 wires.

## Serial interface — 16-bit frames over SPI

Each transaction is 16 bits, MSB first: **D15–D8 = register address, D7–D0 =
data**. Data is clocked into a shift register on each rising CLK edge while
LOAD/CS is low; the **rising edge of CS latches** the 16 bits into the addressed
register. That maps cleanly onto the ATmega328P hardware SPI in **mode 0**
(CPOL=0, CPHA=0 — sample on rising edge), which is why `max7219.c` needs no mode
bits set. We run SPI at f_osc/16 = 1 MHz, far under the part's 10 MHz limit.

Key registers (`max7219.c`):

| Reg  | Name       | We write            | Meaning                                    |
| :--- | :--------- | :------------------ | :----------------------------------------- |
| 0x09 | Decode     | 0x00                | **No-decode** — data bits map 1:1 to segments |
| 0x0B | Scan-limit | 0x07                | Scan all 8 digits                          |
| 0x0A | Intensity  | 0x07                | 16-step PWM brightness (mid)               |
| 0x0C | Shutdown   | 0x01                | Normal run (0 = shutdown/blank)            |
| 0x0F | Disp-test  | 0x00                | Lamp test off                              |
| 0x01–0x08 | Digit 0–7 | segment byte     | The per-digit pixel data                   |

## No-decode vs Code-B — why we drive raw segments

The MAX7219 has a built-in **Code-B decoder**: write `5` and it renders the
digit `5`. We *disable* it and send raw segment bitmaps instead. Bit order
(datasheet Table 6):

```
 D7  D6  D5  D4  D3  D2  D1  D0
 DP   A   B   C   D   E   F   G
```

Two reasons for raw segments here. First, it lets us **OR the decimal point (D7)
onto a digit**, which is how the minute/second separators in `00.00.000` are
drawn — the DP and the digit share one register byte. Second, it reuses the
existing `font7seg.c` table unchanged: `render_time()` looks up `'0'..'9'` for
the digits and `'.'` for the bare DP segment. The stopwatch only ever shows
those eleven glyphs, so the font's letter-approximation caveats don't apply.

## Timekeeping with a Timer1 CTC interrupt

A stopwatch needs an accurate, non-blocking time base. Busy-wait delays
(`_delay_ms`) can't be used: while the CPU is parked in a delay it can't watch
the buttons, and every millisecond spent pushing SPI or debouncing would be
lost from the count. Instead **Timer1 runs in CTC (Clear Timer on Compare)
mode**: the counter counts up to `OCR1A` and auto-clears, raising a compare
interrupt each time. With prescaler clk/8 at 16 MHz and `OCR1A = 1999`:

```
tick period = (OCR1A + 1) / (F_CPU / prescale)
            = 2000 / (16e6 / 8) = 2000 / 2e6 = 1.000 ms  (exact)
```

The division is exact, so there is **no rounding drift** — accuracy is limited
only by the crystal. `ISR(TIMER1_COMPA_vect)` advances two counters: a
free-running `s_ticks_ms` (always) and `s_elapsed_ms` (only while running).
The ISR stays tiny — a couple of increments — and `main()` does all the heavy
lifting (rendering, button logic), per the "ISRs stay short" rule.

Because these counters are 32-bit and the AVR is an 8-bit machine, a read in
`main()` can be interrupted mid-way (the ISR could update the low byte after
`main` grabbed it but before the high bytes), yielding a torn value. Reads and
the clear-to-zero write are therefore wrapped in `ATOMIC_BLOCK`, which briefly
disables interrupts to make the access indivisible.

## Debouncing the pushbuttons

A mechanical switch does not close cleanly: for a few milliseconds after a press
the contacts *bounce*, making the input flip HIGH/LOW many times. Acting on a
single raw sample would read one physical press as several. `button_pressed()`
suppresses this by **requiring a changed level to hold steady**: it samples each
button once per millisecond (paced by `s_ticks_ms`, so the timing is independent
of how fast the render loop spins) and only commits a new level after it has read
the same for `DEBOUNCE_MS` (15) consecutive samples. It then reports the
*edge* — true exactly once, on the released→pressed transition — so a single
press toggles start/pause exactly once. The buttons are active-low (pressed =
LOW), so a committed LOW level is what counts as a press.
