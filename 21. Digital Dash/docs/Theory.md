# Theory.md — digital_dash

Explains the physics and electronics behind every non-trivial component in the project.

---

## 1. HD44780 LCD — 4-bit mode and CGRAM

The HD44780 controller stores display characters in **DDRAM** (Display Data RAM, 80 bytes). Each byte written maps to a character in its built-in ROM font. For custom glyphs the controller also provides **CGRAM** (Character Generator RAM): 8 user-defined 5×8 pixel patterns at slots 0–7. Writing a byte value 0–7 to DDRAM renders the corresponding custom glyph.

**4-bit initialisation**: on cold power-on the controller's internal state is undefined. The HD44780 specification requires a software reset sequence with three specific 8-bit "function set" nibbles sent as half-transfers before switching to 4-bit mode. `lcd_driver.c` follows this sequence with busy-flag polling.

**CGRAM addressing**: sending `LCD_CMD_SET_CGRAM_ADDR | (slot << 3)` puts the Address Counter (AC) into CGRAM space. Every subsequent `WriteData` call advances through the 8 row-bytes of that slot. After all 7 glyphs are written, `Lcd_SetCursor(0, 0)` sends `LCD_CMD_SET_DDRAM_ADDR | 0x00` to return the AC to DDRAM before any further text output.

---

## 2. Timer0 — CTC 1 ms system tick

The ATmega328P's Timer0 is an 8-bit counter. In **CTC (Clear Timer on Compare)** mode the counter resets to 0 whenever `TCNT0 == OCR0A`, and the `TIMER0_COMPA_vect` interrupt fires at that moment.

```
f_tick = F_CPU / (prescaler × (OCR0A + 1))
       = 16 000 000 / (64 × 250)
       = 1 000 Hz  →  1 ms period
```

A `volatile uint32_t` counter incremented inside the ISR gives a millisecond timebase. Multi-byte reads from `main()` are wrapped in `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)` (avr-libc `util/atomic.h`) to prevent a torn read: without atomicity, the CPU might read the low byte before an ISR fires and increments the high byte, returning a corrupted value.

---

## 3. Pin-Change Interrupts — PCINT1 group

The ATmega328P supports three pin-change interrupt groups. Buttons on PC0 and PC1 belong to **group 1** (`PCINT1_vect`). A single ISR fires whenever *any* unmasked pin in the group changes state, regardless of direction. The ISR must:

1. Sample `PINC` to read the current state.
2. XOR against a stored previous state to find which pin(s) changed.
3. Check the new level to determine edge direction (falling = press for active-low buttons).
4. Update the stored snapshot.

**Debounce**: mechanical buttons bounce for 1–10 ms on press/release. A 5 ms guard timer per button (using `sys_millis()`) ignores repeated transitions within the bounce window.

**Pull-ups**: with no external pull-down, an unconnected input floats and picks up noise. The internal pull-up (`PORTC |= bit` while `DDRC bit = 0`) holds the line at logic-1 (VCC) when the button is open, and the button shorts it to GND when pressed — yielding a clean active-low signal.

---

## 4. Timer2 CTC — Piezo tone generation

A passive piezo transducer resonates when driven with a square wave at its resonant frequency (typically 2–4 kHz, but usable from ~200 Hz to 5 kHz). The ATmega328P's **OC2A pin (PB3)** can automatically toggle on each Timer2 compare match, producing the square wave in hardware without CPU involvement.

```
f_out = F_CPU / (2 × prescaler × (OCR2A + 1))
→ OCR2A = F_CPU / (2 × prescaler × f_out) − 1
```

The prescaler is chosen at runtime as the smallest value that keeps OCR2A within the 8-bit range [0, 255]. Timer2 is started by writing `CS2x` bits; stopped by clearing them. After the tone duration expires, `TCCR2A` is cleared to disconnect OC2A from the timer, and the DDR bit is cleared to tristate PB3 (no DC bias across the piezo).

**Why Timer2, not Timer1?** Timer1 is 16-bit and the preferred resource for future PWM or input-capture work. Timer2 is 8-bit and sufficient for audio frequencies.

---

## 5. ADC floating-pin RNG seed

The ATmega328P's 10-bit ADC converts an analogue voltage to a digital value. A floating (unconnected) pin has no defined voltage — it picks up capacitive coupling from nearby signals, thermal noise in the transistors, and electromagnetic interference. The result is a pseudo-random value that differs between power cycles.

**Configuration**: AVCC reference, ADC channel 2 (PC2, physically unconnected). Prescaler /128 puts the ADC clock at 125 kHz — within the 50–200 kHz range needed for full 10-bit accuracy (datasheet §23.4). After one conversion the ADC is disabled to save ~350 µA.

The 10-bit seed initialises a **xorshift16** PRNG (Marsaglia, 2003), which passes statistical randomness tests with a tiny 2-byte state — appropriate for an 8-bit microcontroller.

---

## 6. EEPROM wear levelling

The ATmega328P has 1 KB of EEPROM rated for **100 000 write cycles per cell**. Naive `eeprom_write_*` always erases then writes regardless of whether the value changed. `eeprom_update_*` (avr-libc) reads the cell first and skips the write if the value is already correct — eliminating unnecessary wear on cells that hold stable values (e.g. the magic byte after the first power-up).

A **magic byte** (0xA5) at a known address distinguishes a freshly-erased EEPROM (all 0xFF) from a legitimately stored score of 0. Without it, the first power-up would always load a score of 0xFFFF from an erased device.
