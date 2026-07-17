# Changelog

## [2026-06-28] - Session: bit-order fix and row settle delay

### Fixed

- `main.c`: All 16 keys decoded as row 0 — row scan was reading columns before
  the 50 kΩ internal pull-up had time to recharge lines held low during the
  all-rows-active debounce phase.  Added `ROW_SETTLE_US` (10 µs) delay after
  each row drive before sampling columns.  With ~100 pF breadboard capacitance
  RC ≈ 5 µs; 10 µs covers >2τ for a clean logic-high read.
- `main.c`: LED display bit order reversed — D1 is wired to PD0 (bit 0) but
  physically read as the MSB.  Added `reverse_byte()` helper (three swap passes:
  nibbles → pairs → adjacent bits) applied to every PORTD write.

### Files touched

- `main.c`

### Why

- Row-settle: transitioning from all rows driven low to one row driven low
  leaves previously-active columns at 0 V; without a delay the first scan step
  always wins, pinning `rowloc` at 0.
- Bit-reverse: LEDs are laid out D1 (MSB) → D8 (LSB), but PORTD writes bit 0
  to D1 (LSB).  The mismatch caused mirrored ASCII patterns on the display.

---

## [2026-06-28] - Session: Changed hardware connections

### Changed

- `main.c`: Moved row outputs from PORTC high nibble (PC4-PC6) to PORTB low
  nibble (PB0-PB3).
- `main.c`: Columns remain on PORTC low nibble (PC0-PC3) — unchanged.
- `main.c`: Added row 3 (PB3) scan; previously unreachable because PC7 is not
  bonded out on the ATmega328P DIP-28 package.
- `main.c`: Replaced single-port `KEY_DDR/KEY_PRT/KEY_PIN` macros with
  separate `ROW_DDR/ROW_PRT/ROW_MASK` and `COL_DDR/COL_PRT/COL_PIN/COL_MASK`
  macros to reflect the split across two ports.
- `main.c`: All PORTB writes now use read-modify-write
  (`(ROW_PRT | ROW_MASK) & ~bit`) to avoid disturbing PB6/7 (XTAL1/XTAL2
  crystal oscillator pins).

### Files touched

- `main.c`

### Why

- Hardware re-wired so that rows connect to PB0-PB3 rather than PC4-PC6.
- The primary motivation is illuminating row 3 (the bottom row: `*`, `0`, `#`,
  `D`), which was electrically unreachable with the old PC4-PC6 mapping because
  the ATmega328P DIP-28 does not expose PC7.
