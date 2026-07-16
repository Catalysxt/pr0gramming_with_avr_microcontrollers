# Logging Thermometer

## Overview

Reads ambient temperature from an LM75 sensor every N seconds (configurable), stores each reading compressed into one byte in a 24LC256 external I2C EEPROM, and streams readings over UART. On power-up a 5-second menu window lets you dump the stored log, adjust the sample interval, or erase the chip. The 32 KB EEPROM holds up to 32,764 readings. This project introduces persistent external storage over I2C, multi-device bus sharing, and a 9-bit-to-8-bit temperature compression scheme.

---

## Hardware

| Part | Notes |
|------|-------|
| ATmega328P | 16 MHz external crystal |
| LM75 breakout | A0/A1/A2 internally tied to GND → I2C address 0x48 |
| 24LC256 | DIP-8 or breakout; A0/A1/A2 and WP to GND → I2C address 0x50 |
| LED + 330 Ohm resistor | Heartbeat on PB0 |
| USB-to-UART adapter | CH340, CP2102, or FT232; 19200 8N1 |
| USBasp | Programmer |

Full wiring and pinout: [docs/hardware_connections.md](docs/hardware_connections.md)

> **Critical:** Tie the 24LC256 WP pin to GND. If WP is high or floating, all writes are silently ignored and no data will be stored.

> **I2C pull-ups:** One pair of 4.7 kΩ pull-ups to VCC on SDA and SCL serves both devices. Do not add a second pair.

---

## Theory

The LM75 and 24LC256 share the same I2C bus (addresses 0x48 and 0x50 do not conflict). Each LM75 reading is 9 bits (8-bit integer °C + 0.5 °C flag); this is compressed into a single byte by shifting the integer into bits[7:1] and placing the half-degree flag in bit[0], doubling EEPROM capacity. Configuration (sample interval and write pointer) is stored at fixed addresses in the 24LC256 and persists across power cycles. A fresh chip returns 0xFFFF for all addresses — run `[e]` on first use to initialise.

Full theory: [docs/Theory.md](docs/Theory.md)

---

## Build & Flash

```bash
cd chapter_17_i2c/logging_thermometer
make flash
```

Artifacts land in `build/` — project root stays clean.

---

## How to Use

### First run (fresh EEPROM)

1. Flash the chip: `make flash`
2. Open a serial terminal at **19200 8N1**
3. Reset the board — the prompt appears:
   ```
   *** Press [m] within 5 seconds to enter menu. ***
   ```
4. Press `[m]` within 5 seconds to enter the menu
5. Press `[e]` to erase and initialise the EEPROM (~3 seconds)
6. Press `[r]` to set the sample interval to 60 seconds
7. Press `[s]` to start logging

### Normal use

- **`[<]` / `[>]`** — decrease/increase sample interval by 5 seconds (minimum 10 s, maximum 65000 s)
- **`[r]`** — reset interval to 60 seconds
- **`[p]`** — dump all stored readings over UART
- **`[e]`** — erase EEPROM and reset pointers (destructive — all readings lost)
- **`[s]`** — exit menu and begin logging
- **LED on PB0** — blinks once per second during the inter-reading delay

If you do not press `[m]` within 5 seconds, logging resumes automatically using the last saved settings.

---

## Practical Checklist

1. Wire hardware per [docs/hardware_connections.md](docs/hardware_connections.md). Confirm WP pin is tied to GND.
2. Flash: `make flash` (USBasp connected).
3. Open serial terminal at **19200 8N1**.
4. Reset board. Terminal shows: `*** Press [m] within 5 seconds to enter menu. ***`
5. Press `[m]`. Menu appears showing reading count and sample interval.
6. Press `[e]`. Terminal shows `Clearing EEPROM. Please wait (~3 s).` — wait for next menu prompt.
7. Press `[r]`. Interval resets to 60 seconds.
8. Press `[s]`. Terminal shows `OK, commencing logging...` followed by the first temperature reading.
9. Confirm LED starts blinking. A new reading prints every 60 seconds.
10. Reset board. Press `[m]` within 5 seconds. Menu shows `1 readings in EEPROM.` (or however many were logged).
11. Press `[p]`. All stored readings print — confirm no leading zeros (e.g. `25.5 degrees` not `025.5 degrees`).
12. Warm the LM75 with your finger during logging — confirm readings rise.

---

## Memory Footprint

Measured with `make size`:

| Region | Used | Limit | % |
|--------|------|-------|---|
| Flash (`.text`) | 2096 B | 32 KB | 6.4 % |
| SRAM (`.data + .bss`) | 0 B | 2 KB | 0 % |

---

## Stretch Goals

- **Sequential EEPROM read** — add `EEPROM_read_sequential(start, count, callback)` to the 24LC256 driver to dump the log in one I2C transaction rather than one per byte; dramatically faster for large logs.
- **Wear levelling for config words** — `CURRENT_LOCATION_POINTER` is rewritten every logging cycle. Over 2 years at 60 s intervals this address wears out. Rotate the pointer across a small block of reserved addresses.
- **Sleep mode** — put the ATmega into Power-Down between readings using `<avr/sleep.h>` and a watchdog timer; reduces current from ~15 mA to ~5 uA, enabling battery operation for months.
- **Timestamp** — pair with a DS3231 RTC (also I2C) to log real wall-clock timestamps alongside temperature.
- **Negative temperature** — treat `temp_high_byte` as `int8_t` before compression and handle the sign in `print_temperature`.
- **CSV output** — modify `[p]` to print a numbered CSV (`index,temperature`) for easy import into a spreadsheet.
