# I2C Hello World — LM75 Thermometer

## Overview

Reads the ambient temperature from an LM75 sensor over the ATmega328P's hardware TWI (I2C) peripheral and prints a reading every 3 seconds over UART. This project introduces bare-metal I2C: bus initialisation, the pointer-register write / repeated-START / multi-byte read sequence, and decoding the LM75's 9-bit two's complement temperature format.

---

## Hardware

| Part | Notes |
|------|-------|
| ATmega328P | 16 MHz external crystal |
| LM75 breakout | Address pins A0/A1/A2 hardwired to GND → I2C address 0x48 |
| USB-to-UART adapter | CH340, CP2102, or FT232 |
| USBasp | For flashing only |

Full wiring: [docs/hardware_connections.md](docs/hardware_connections.md)

> **I2C pull-ups:** SDA and SCL require pull-up resistors to VCC. Most LM75 breakout boards include 4.7 kΩ pull-ups on-board.

---

## Theory

The LM75 uses an on-chip bandgap reference and sigma-delta ADC to produce a 9-bit two's complement temperature with 0.5 °C resolution. The ATmega328P TWI peripheral handles the open-drain I2C bus in hardware; the CPU only writes command words to TWCR and polls the TWINT flag. A repeated START (rather than STOP + new START) is used between the register-pointer write and the two-byte temperature read to maintain bus ownership.

Full theory: [docs/Theory.md](docs/Theory.md)

---

## Build & Flash

```bash
cd chapter_17_i2c/hello_world
make flash
```

Artifacts (`.elf`, `.hex`, `.map`) are written to `build/` — the project root stays clean.

---

## How to Use

1. Wire up the hardware per [docs/hardware_connections.md](docs/hardware_connections.md).
2. Flash the chip: `make flash`
3. Open a serial terminal at **19200 8N1** on the USB-UART adapter's COM port.
4. Power-cycle or reset the ATmega328P.
5. The terminal prints a header line followed by one temperature reading every 3 seconds:

```
==== i2c Thermometer ====
25.0 degrees
25.0 degrees
25.5 degrees
```

---

## Practical Checklist

1. Open a serial terminal (PuTTY, screen, minicom) at **19200 8N1** on the AVR's COM port.
2. Power on or reset the board. The terminal should display:
   ```
   ==== i2c Thermometer ====
   ```
3. Wait 3 seconds. A temperature reading such as `25.0 degrees` should appear.
4. Readings should continue appearing every 3 seconds.
5. Pinch the LM75 chip gently between your fingers to warm it. Within a few readings the displayed temperature should rise by at least 0.5 degrees.
6. Release the chip. Temperature should drift back toward ambient over the next several readings.
7. If all readings show `0.0 degrees` or garbage values, check SDA/SCL pull-ups and verify the LM75's VCC and GND connections.

---

## Memory Footprint

Measured with `make size` (`avr-size`):

| Region | Used | Limit | % |
|--------|------|-------|---|
| Flash (`.text`) | 618 B | 32 KB | 1.9 % |
| SRAM (`.data + .bss`) | 0 B | 2 KB | 0 % |

---

## Stretch Goals

- **Min/max tracking** — record the lowest and highest readings in SRAM; print on demand over UART.
- **EEPROM logging** — store readings in external EEPROM (e.g. 24LC256) so the log survives power cycles. *(Next project: `logging_thermometer`)*
- **Negative temperature support** — treat `temp_high_byte` as `int8_t` and handle the sign in `print_uint8` to correctly display sub-zero readings.
- **Configurable interval** — read a sampling interval (in seconds) from UART at startup instead of hardcoding 3000 ms.
- **OS pin interrupt** — wire the LM75 OS (overtemperature) pin to an ATmega interrupt and trigger a UART alert when the threshold is crossed, without polling.
