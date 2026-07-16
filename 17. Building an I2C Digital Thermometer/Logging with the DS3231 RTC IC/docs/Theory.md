# Theory — Logging Thermometer + DS3231

This project builds on `logging_thermometer/docs/Theory.md` (EEPROM physics, the 24LC256 protocol, LM75 temperature compression) and `hello_world/docs/Theory.md` (I2C fundamentals). Read those first. This document covers what's new: the DS3231 RTC, BCD encoding, and the redesigned EEPROM record format.

---

## 1. Why DS3231 instead of a plain crystal + counter?

A microcontroller could keep time itself by counting Timer1 overflows against the 16 MHz crystal, but that clock drifts with temperature — the crystal's resonant frequency shifts as its physical dimensions change with heat, and ordinary crystals aren't cut to compensate for this. Over a day that drift is seconds; over a logging run of days it becomes visibly wrong.

The DS3231 solves this with a **temperature-compensated crystal oscillator (TCXO)**: an internal temperature sensor measures the die every 64 seconds and adjusts an internal capacitance array to correct the 32.768 kHz crystal's frequency in real time. This is the difference between a DS3231 and its cheaper sibling, the DS1307 — the DS1307 has no TCXO and drifts several seconds per day. Because it needs no attention from the host MCU to maintain accuracy, the ATmega328P just asks it for the time and trusts the answer.

The coin-cell (CR2032) and 32.768 kHz crystal are already populated on the breakout module, so backup power and the timekeeping crystal are a solved problem out of the box — the only wiring this project needs to add is VCC, GND, SDA, SCL.

---

## 2. BCD (Binary-Coded Decimal)

The DS3231's timekeeping registers (0x00-0x06) don't store binary integers — they store **BCD**, where each nibble (4 bits) holds one decimal digit. For example, the minute value 42 is stored as `0x42` (binary `0100 0010`): the upper nibble is the tens digit (4), the lower nibble is the units digit (2). This is *not* the same as the binary integer 42 (`0x2A`), which is a well-known source of subtle bugs — reading a BCD byte as if it were plain binary makes hour 18 print as decimal 24 (`0x18` = 24).

The DS3231 uses BCD because it was designed for direct 7-segment display driving, decades before that mattered for an AVR — each nibble maps straight to a digit with no division/modulo needed. `ds3231.c` converts in both directions with simple nibble arithmetic:

```c
// BCD -> binary
binary = (bcd >> 4) * 10 + (bcd & 0x0F);

// binary -> BCD
bcd = ((binary / 10) << 4) | (binary % 10);
```

This conversion is isolated inside the driver (`bcd_to_binary()` / `binary_to_bcd()` in `extern_libraries/ds3231.c`) precisely so `main.c` — and the on-EEPROM record format — never has to reason about BCD. Everything above the driver boundary is plain binary.

### Register bit layout

A few registers pack more than just a BCD digit pair:

| Register | Bits used for BCD | Other bits |
|---|---|---|
| Hours (0x02) | [5:0] in 24-hour mode | bit 6 = 12/24-hour select (this driver always writes 0 -> 24-hour) |
| Month (0x05) | [4:0] | bit 7 = century flag (ignored; this project assumes 2000-2099) |
| Date (0x04) | [5:0] | bit 7 reserved |

`ds3231_read_time()` masks each register (e.g. `& 0x3F` for hours) before decoding, so stray control bits never corrupt the decoded value.

---

## 3. Three I2C devices, one bus

Adding the DS3231 brings the bus to three slaves, distinguished purely by 7-bit address:

| Device | Address | Notes |
|---|---|---|
| LM75 (temperature) | 0x48 | A0-A2 strapped to GND |
| 24LC256 (EEPROM) | 0x50 | A0-A2 and WP strapped to GND |
| DS3231 (RTC) | 0x68 | Fixed — no address pins exist on this part |

No new pull-up resistors are needed: SDA/SCL are open-drain, and one pair of resistors sets the idle-high level for however many devices share the bus. The DS3231's *lack* of address pins is worth noting as a limit — unlike the LM75, you can never put two DS3231s on the same bus.

---

## 4. EEPROM record format v2 — adding a timestamp

The original project stored one byte per reading (packed LM75 temperature only). To make that byte interpretable, each record now also carries the wall-clock time it was taken:

```c
typedef struct {
    uint8_t year;    // offset from 2000
    uint8_t month;
    uint8_t date;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t temp;    // packed LM75 reading (same encoding as before)
} log_record_t;       // 7 bytes, thanks to -fpack-struct (Makefile CFLAGS)
```

All six timestamp fields are stored as **plain binary**, not BCD — the DS3231 driver already converted BCD to binary on read, and there's no reason to pay that conversion cost twice (once in the driver, again for display). A record is written/read with a loop of `EEPROM_write_byte()`/`EEPROM_read_byte()` calls (7 I2C transactions), since 7 bytes is comfortably under the 24LC256's 64-byte page and readings are never less than 10 seconds apart — there's no throughput pressure to justify a page-write path.

### Capacity trade-off

Broken-out binary fields cost more space than a packed 32-bit Unix-style timestamp would (7 bytes vs. ~5), but they avoid needing calendar/leap-year math on an 8-bit MCU just to print a human-readable date for the `[p]` dump. The capacity impact:

```
Usable bytes = 0x7FFF - MEMORY_START + 1 = 32764
Max records  = floor(32764 / 7)          = 4680

At the default 120 s interval:
4680 records x 120 s = 561,600 s ~= 156 hours ~= 6.5 days of continuous logging
```

That's down from 32,764 one-byte readings in the original design, but the recorded data is now actually interpretable without external bookkeeping of when logging started — which was the entire point of this extension.

---

## 5. Why the temperature sensor stays the LM75

The DS3231 has its own on-die temperature sensor (registers 0x11-0x12, 0.25 degC resolution, updated every 64 s as part of the TCXO compensation cycle) — it would be possible to drop the LM75 entirely and read temperature from the RTC instead. This project keeps the LM75 because: (a) it updates continuously rather than every 64 s, (b) its 9-bit reading (0.5 degC steps, on-demand conversion) is closer to the resolution and cadence the original project was built around, and (c) removing a device is a bigger scope change than the brief asked for. Using the DS3231's built-in sensor to cut the bus down to two devices is a reasonable stretch goal, not a default.
