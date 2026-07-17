# Sentinel — FSM-driven relay controller

## Overview

An **extension** of `relay/hello_world` (which just blinks a relay with blocking
`_delay_ms`). Here the relay is owned by a four-state **finite state machine** —
`AUTO`, `MANUAL_ON`, `MANUAL_OFF`, `LOCKOUT` — driven by a non-blocking 1 ms
Timer0 timebase and a debounced push button, with the current mode reported over
UART. The switched load is a **12 V LED** switched by a relay module driven
directly from PB0. What's
interesting: there is no `_delay_ms` in the run loop at all — timing is
cooperative, so the button is serviced every pass no matter what the relay is
doing, and `LOCKOUT` acts as a deliberate safety freeze that only a reset clears.

## Hardware

- ATmega328P @ 16 MHz external crystal
- Active-HIGH relay module (VCC/GND/IN; on-board driver + flyback), driven directly by PB0
- 12 V LED load + 12 V supply on the relay's switched contact
- 1× tactile push button (uses the MCU's internal pull-up — no external resistor)
- USB-serial adapter for the UART terminal (9600 8N1)

Full wiring and why the module is driven directly (no ULN2803): **[docs/hardware_connections.md](docs/hardware_connections.md)**.

| Signal      | MCU pin |
| :---------- | :------ |
| Relay drive | PB0     |
| Button      | PD2     |
| UART TX/RX  | PD1/PD0 |

## Theory

Relay electromechanics (coil, back-EMF, why the flyback diode), the FSM +
non-blocking timing architecture, the sampling-timer debounce, and EEPROM wear are
all explained in **[docs/Theory.md](docs/Theory.md)**.

## Build & Flash

```sh
make            # -> build/sentinel.hex
make size       # flash / SRAM usage
make flash      # program via USBasp
```

## How to use

1. Wire per `docs/hardware_connections.md`; connect the USB-serial adapter and open
   a terminal at **9600 8N1**.
2. On power-up the firmware prints the restored state (or `AUTO` on a fresh chip).
3. **Short press** the button to cycle: `AUTO → MANUAL_ON → MANUAL_OFF → AUTO`.
   Each transition prints its name over UART.
   - `AUTO` — relay/LED toggles once per second.
   - `MANUAL_ON` — LED frozen on.
   - `MANUAL_OFF` — LED frozen off.
4. **Hold ≥ 2 s** to enter `LOCKOUT`: LED off, short presses ignored.
5. **Hold ≥ 2 s again** to leave `LOCKOUT` — it toggles — and the board resumes the
   operational mode it was in before locking out. (A reset also clears LOCKOUT.)

## Practical checklist

1. Open a serial terminal (PuTTY/screen/minicom) at 9600 8N1 on the AVR's COM port.
2. Power on — terminal prints `AUTO` and the LED begins blinking at ~1 Hz.
3. Short-press the button — terminal prints `MANUAL_ON` and the LED stays lit.
4. Short-press again — terminal prints `MANUAL_OFF` and the LED goes dark.
5. Short-press again — terminal prints `AUTO` and blinking resumes.
6. From `MANUAL_ON`, press and hold ~2 s — terminal prints `LOCKOUT`, LED off; tap
   a few times and confirm nothing changes and nothing is printed.
7. Hold ~2 s again — terminal prints `MANUAL_ON` (resumed from EEPROM), LED lit,
   proving the long-press LOCKOUT toggle and the resume.
8. While in `MANUAL_ON`, press reset — terminal prints `MANUAL_ON`, proving EEPROM
   persistence across a power-cycle.
9. Enter `LOCKOUT`, then press reset — terminal prints the last operational state
   (not `LOCKOUT`), proving lockout is never persisted.

## Memory footprint

Latest build: `.text` **1792 B** (5.5% of 32 KB flash), `.data + .bss` **38 B**
(1.9% of 2 KB SRAM).

## Stretch goals

- **Done:** EEPROM persistence of the operational state.
- **Done:** long-press LOCKOUT is a toggle (hold to arm, hold again to resume).
- **Next:** watchdog timer that resets on lockup (`<avr/wdt.h>`).
- Report state changes with a timestamp (`sys_millis()`), or add a heartbeat print.
