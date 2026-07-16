# Working with the 24LC256 EEPROM IC

## Overview

An interactive bare-metal AVR program that demonstrates reading from and writing to a Microchip 24LC256 EEPROM over I2C (TWI). A custom TWI driver handles the two-wire bus at 100 kHz. A serial terminal (9600 baud) lets you inspect the first 10 bytes of EEPROM, write an arbitrary byte to any address, or erase the entire 32 KB array using 64-byte page writes. The project started as an SPI-based 25LC256 demo and was fully refactored to I2C to illustrate the two protocols side-by-side.

---

## Hardware

**Bill of materials:** ATmega328P, Microchip 24LC256 (8-pin DIP), 16 MHz crystal, two 4.7 kΩ pull-up resistors (SDA/SCL), USB-UART adapter.

Full wiring table and ASCII schematic: [docs/hardware_connections.md](docs/hardware_connections.md)

Key connections at a glance:

| Signal | MCU pin | EEPROM pin |
|:-------|:--------|:-----------|
| SDA | PC4 | pin 5 |
| SCL | PC5 | pin 6 |
| A0/A1/A2 | — | pins 1-3 → GND |
| WP | — | pin 7 → GND |

---

## Theory

I2C is a two-wire open-drain bus (SDA + SCL) with pull-up resistors. The ATmega328P calls it TWI. The 24LC256 is addressed via a control byte (`1010 A2 A1 A0 R/W`); with all address pins grounded the write address is 0xA0 and read address is 0xA1. Reads require a "dummy write" to set the internal address counter followed by a Repeated START to switch bus direction. Writes trigger a 5 ms internal write cycle (Twc); the bus must stay idle until it completes.

---

## Build & Flash

```bash
# Build
make

# Flash via USBasp
make flash

# Check memory usage
make size
```

Requires `avr-gcc`, `avrdude`, and a USBasp programmer on the ISP header.

---

## How to Use

1. Open a serial terminal at **9600 8N1** on the USB-UART adapter's COM port.
2. The terminal displays a memory table (addresses 0–9 and their current values) plus a menu.
3. Press **`w`** to write: enter a decimal address (0–255), press Enter; enter a decimal value (0–255), press Enter. The new value appears in the table on the next loop.
4. Press **`e`** to erase: all 32 KB is zeroed using page writes (~2.6 s).
5. Any other key prints `You say what?` and loops back to the display.

---

## Practical Checklist

Work through these steps in order to confirm the hardware and firmware are both working.

1. Wire up the circuit.
2. Flash the firmware: `make flash`. Avrdude should report `avrdude done. Thank you.`
3. Open a serial terminal (PuTTY / minicom / screen) at **9600 8N1** on the adapter's COM port.
4. The terminal should display:
   ```
   ==== EEPROM Memory Playground ====
   Address Value
    0:   ...
    1:   ...
    ...
    9:   ...
    [e] to erase all memory
    [w] to write byte to memory
   ```
5. Press **`e`** and wait ~3 seconds. The display should refresh with all values showing `0`.
6. Press **`w`**. Enter address `5`, then value `42`. The next display should show `5: 42`.
7. Power-cycle the board (unplug/replug). Re-open the terminal. Address 5 should still read `42` — confirming data persistence across power cycles.
8. Press **`e`** again to restore a clean state. Confirm all values return to `0`.

---

## Memory Footprint

Measured with `avr-size` on ATmega328P (32 KB flash, 2 KB SRAM):

| Section | Bytes | % of limit |
|:--------|------:|----------:|
| `.text` (flash) | 1,158 | 3.5% |
| `.data + .bss` (SRAM) | 0 | 0% |

SRAM usage at runtime is stack-only (local variables and function call frames).

---

## Stretch Goals

- **ACK polling instead of fixed delay**: after a write STOP, poll the 24LC256 by repeatedly sending a START + write address until it ACKs — this is faster than the worst-case 5 ms delay and is the technique the datasheet calls "acknowledge polling" (p. 6).
- **EEPROM hex dump**: add an `[h]` menu option that prints all 32 KB in classic hex-dump format (address | hex bytes | ASCII), reading 64 bytes at a time with sequential reads.
- **Multi-byte write via UART**: accept a string over UART and store it starting at a user-specified address, using page writes to minimise write cycles.
- **I2C scanner**: scan all 127 I2C addresses and report which ones ACK — useful for verifying wiring and debugging address conflicts on a shared bus.
- **TWI status code checking**: add error handling that reads `TWSR` after each bus operation and returns an error code if the status is not what was expected (e.g., 0x18 = SLA+W ACK, 0x28 = data ACK). Currently the driver proceeds blindly if the EEPROM does not respond.
