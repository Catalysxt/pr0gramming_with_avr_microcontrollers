# Theory

Short notes on the non-trivial physics/electronics/CS behind each
subsystem — not a general AVR tutorial, just the parts of this project
that aren't self-evident from the code.

## 1. HD44780 DDRAM vs CGRAM, and busy-flag timing

The LCD controller has two separate internal memories. DDRAM (Display
Data RAM) holds the character codes currently shown on screen — writing
byte `0x41` to DDRAM address 0x00 shows `A` at row 0, column 0. CGRAM
(Character Generator RAM) holds up to 8 user-defined 5×8-pixel glyphs in
slots 0–7; writing character code `0x00`–`0x07` to DDRAM instead
displays whichever custom pattern lives in that CGRAM slot. ANVIL uses
CGRAM slot 0 for the padlock icon on the splash screen (`ui.c`).

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
burst of spurious open/close transitions before settling. ANVIL requires
the same raw scan result to repeat for 3 consecutive 5 ms scans (15 ms)
before treating it as a real, stable press or release, which is well
past typical contact-bounce duration for a tactile keypad.

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

## 4. EEPROM wear-leveling and the magic-byte pattern

EEPROM cells have a finite write-endurance (typically ~100,000 erase/write
cycles per byte on AVR). `storage.c` only ever calls `eeprom_update_*`
(never `eeprom_write_*`): `eeprom_update_*` reads the cell first and
skips the actual write entirely if the value hasn't changed, so
re-saving an unchanged PIN costs nothing.

Freshly-erased EEPROM reads as `0xFF` in every cell — indistinguishable
from a legitimately stored value that happens to be `0xFF`. The 2-byte
magic word at EEPROM offset 0 sidesteps this ambiguity: `storage_init()`
only trusts the stored PIN if the magic word matches a specific constant
(`0x4CA1`) that a blank chip could never have by chance; otherwise it
treats the chip as first-boot and reinitialises both the magic word and
the factory-default PIN `1234`.

## 5. Constant-time PIN comparison

A naive PIN comparison (`for` loop that `return`s the instant it finds a
mismatching byte) leaks information through timing: a guess whose first
digit is correct takes measurably longer to reject than one whose first
digit is wrong, because the loop runs one more iteration before
bailing out. `storage_check_pin()` avoids this by reading and
XOR-accumulating all 4 bytes unconditionally regardless of where (or
whether) a mismatch occurs, then checking the accumulated result once at
the end — the function takes the same number of EEPROM reads and
comparisons no matter which digits are wrong. This is overkill for a
hobby project with no realistic timing-attack threat model, but it's a
cheap habit worth building correctly.
