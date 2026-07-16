# Logging Thermometer + DS3231

## Overview

Extends `logging_thermometer` with a DS3231 real-time clock so every stored reading carries a true wall-clock timestamp. Reads ambient temperature from an LM75 every N seconds (configurable), pairs it with the current time from a battery-backed DS3231, and stores both as a 7-byte record in a 24LC256 external I2C EEPROM. A host-side Python toolchain (`make dump`, `make plot`) pulls the log over serial and renders a matplotlib chart of temperature against real dates and times — proving the readings survive power loss *and* carry meaningful timestamps, unlike the original project's bare, unordered byte dump.

---

## Hardware

| Part | Notes |
|------|-------|
| ATmega328P | 16 MHz external crystal |
| LM75 breakout | A0/A1/A2 internally tied to GND -> I2C address 0x48 |
| 24LC256 | DIP-8 or breakout; A0/A1/A2 and WP to GND -> I2C address 0x50 |
| DS3231 RTC module | Fixed I2C address 0x68; onboard 32.768 kHz crystal + CR2032 backup cell |
| LED + 330 Ohm resistor | Heartbeat on PB0 |
| USB-to-UART adapter | CH340, CP2102, or FT232; 9600 8N1 |
| USBasp | Programmer |

Full wiring and pinout: [docs/hardware_connections.md](docs/hardware_connections.md)

> **Critical:** Tie the 24LC256 WP pin to GND. If WP is high or floating, all writes are silently ignored and no data will be stored.

> **I2C pull-ups:** One pair of 4.7 kOhm pull-ups to VCC on SDA and SCL serves all three devices. Do not add a second pair.

> **DS3231 SQW/32K:** Left unconnected — this project polls the clock, it doesn't use alarms or the auxiliary clock output.

---

## Theory

The LM75, 24LC256, and DS3231 share one I2C bus (addresses 0x48, 0x50, 0x68 — no conflicts). The DS3231 stores time as BCD; the driver (`extern_libraries/ds3231.h/.c`) converts to/from plain binary so the rest of the firmware never touches BCD directly. Each EEPROM record is now 7 bytes (6 timestamp fields + 1 packed LM75 byte) instead of 1, trading raw capacity (~4,680 readings vs. 32,764) for interpretability.

Full theory, including why a DS3231 rather than a DS1307 and the BCD register bit layout: [docs/Theory.md](docs/Theory.md)

---

## Build & Flash

```bash
cd chapter_17_i2c/logging_with_ds3231
make flash
```

Artifacts land in `build/` — project root stays clean.

---

## How to Use

### First run (fresh EEPROM, clock never set)

1. Flash the chip: `make flash`
2. Open a serial terminal at **9600 8N1**
3. Reset the board — the prompt appears:
   ```
   *** Press [m] within 5 seconds to enter menu. ***
   ```
4. Press `[m]` within 5 seconds to enter the menu
5. Press `[t]` and follow the prompts to set the clock (year/month/date/hour/minute/second). The DS3231's backup battery keeps this time through power cycles — **you only need to do this once**.
6. Press `[e]` to erase and initialise the EEPROM (~3 seconds)
7. Press `[r]` to set the sample interval to 60 seconds
8. Press `[s]` to start logging

### Normal use

- **`[<]` / `[>]`** — decrease/increase sample interval by 5 seconds (minimum 10 s, maximum 65000 s)
- **`[r]`** — reset interval to 60 seconds
- **`[t]`** — set the DS3231's clock
- **`[p]`** — dump all stored readings over UART as `YYYY-MM-DD HH:MM:SS, XX.X degrees`
- **`[e]`** — erase EEPROM and reset pointers (destructive — all readings lost)
- **`[s]`** — exit menu and begin logging
- **LED on PB0** — blinks once per second during the inter-reading delay

If you do not press `[m]` within 5 seconds, logging resumes automatically using the last saved settings and clock.

### Pulling the log to a chart

```bash
make dump SERIAL_PORT=COM5   # power-cycle the board when prompted, then presses [p] for you
make plot                    # renders docs/overnight_log.csv as a matplotlib chart
```

`make dump` requires `pyserial` (`pip install pyserial`); `make plot` requires `matplotlib` (`pip install matplotlib`).

Example output, real hardware capture (`testing/fixed_x_axis.png`):

![Temperature over time](testing/fixed_x_axis.png)

---

## Practical Checklist

1. Wire hardware per [docs/hardware_connections.md](docs/hardware_connections.md). Confirm the 24LC256 WP pin is tied to GND.
2. Flash: `make flash` (USBasp connected).
3. Open serial terminal at **9600 8N1**.
4. Reset board. Terminal shows: `*** Press [m] within 5 seconds to enter menu. ***`
5. Press `[m]`. Menu appears showing reading count and sample interval.
6. Press `[t]`. Enter the current date/time field by field. Terminal echoes back `Clock set to YYYY-MM-DD HH:MM:SS`.
7. Press `[e]`. Terminal shows `Clearing EEPROM. Please wait (~3 s).` — wait for the next menu prompt.
8. Press `[r]`. Interval resets to 60 seconds.
9. Press `[s]`. Terminal shows `OK, commencing logging...` followed by the first line, e.g. `2026-07-02 14:03:10, 23.5 degrees`.
10. Confirm the LED starts blinking. A new timestamped reading prints every 60 seconds, with the clock visibly advancing.
11. Power-cycle the board (unplug/replug). Press `[m]` within 5 seconds, then `[t]` — confirm the clock kept the correct time (it should not have reset to a stale value).
12. Press `[p]`. All stored readings print, each with a timestamp — confirm timestamps are monotonically increasing.
13. Run `make dump SERIAL_PORT=<your port>` then `make plot` — confirm a chart opens showing temperature against real dates.

---

## Memory Footprint

Measured with `make size`:

| Region | Used | Limit | % |
|--------|------|-------|---|
| Flash (`.text`) | 3260 B | 32 KB | 9.9 % |
| SRAM (`.data + .bss`) | 0 B | 2 KB | 0 % |

---

## Stretch Goals

- **Git milestones** — commit per milestone (RTC bring-up, set_clock, record format, dump script, plot target) with tags, per the original brief.
- **Skip blank (0xFF) records** — detect and skip fully-erased records so a partially-filled chip after a mid-log erase still plots cleanly.
- **DS3231 onboard temperature** — the DS3231 has its own 0.25 degC temperature sensor (registers 0x11-0x12); could replace the LM75 entirely and drop a device from the bus (see Theory.md section 5 for the trade-off).
- **Sleep mode** — put the ATmega into Power-Down between readings using `<avr/sleep.h>` and a watchdog timer; the DS3231 keeps time regardless.
- **Negative temperature** — treat `temp_high_byte` as `int8_t` before compression and handle the sign in `print_temperature`.
- **Alarm-driven wake** — wire up DS3231 SQW to an external interrupt pin and use its programmable alarm to wake the MCU instead of polling, for lower average power.
