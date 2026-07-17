# Hardware Connections — Sentinel (relay/sentinel)

MCU: ATmega328P @ 16 MHz external crystal. Load: a 12 V LED switched by a
**self-contained relay module** (an active-HIGH module with `VCC` / `GND` / `IN`
control pins). PB0 drives the module's `IN` pin **directly** — no external driver.

> **Why no ULN2803 (unlike `relay/hello_world`):** a relay *module* already carries
> its own coil driver (transistor + base resistor + flyback diode, often an
> opto-isolator on the input). It is designed to be driven straight from a logic
> pin. Stacking a ULN2803 in front of it added a *second* driver — and because the
> ULN2803 can only **sink** (pull its output LOW) while this module is **active-HIGH**
> (energises when `IN` is HIGH), the two fought and the relay never actuated. The
> ULN2803 belongs on a **bare** coil, not a module. See `docs/Theory.md`.

## Connection table

| Net         | From (RefDes, Pin) | To (RefDes, Pin)      | MCU pin | Notes                                        |
| :---------- | :----------------- | :-------------------- | :------ | :------------------------------------------- |
| RELAY_DRIVE | U1 (MCU), PB0       | M1 (relay module), IN | PB0     | **Active-HIGH: logic HIGH = relay energised** |
| MOD_VCC     | +5 V supply, +     | M1 (relay module), VCC| -       | Powers the module's logic + coil (5 V module) |
| MOD_GND     | GND                | M1 (relay module), GND| -       | **Must share ground with the MCU**            |
| LED_HIGH    | +12 V supply, +    | M1 (relay module), COM| -       | 12 V rail into the switched contact           |
| LED_ANODE   | M1 (relay module), NO | LED (12 V), +      | -       | Relay closed → LED sees 12 V                  |
| LED_LOW     | LED (12 V), −      | +12 V supply, GND     | -       | Series resistor per LED module rating         |
| BTN_IN      | U1 (MCU), PD2       | SW1 (button), pin A   | PD2     | Internal pull-up enabled; idle HIGH           |
| BTN_GND     | SW1 (button), pin B | GND                   | -       | Press pulls PD2 LOW (active-low)              |
| UART_TX     | U1 (MCU), PD1 (TXD) | USB-serial, RX        | PD1     | 9600 8N1 state reporting                      |
| UART_RX     | U1 (MCU), PD0 (RXD) | USB-serial, TX        | PD0     | Unused by firmware, wired for symmetry        |
| GND_COMMON  | U1 (MCU), GND       | All GND rails         | -       | 5 V logic, 12 V load, module, serial share GND |

## ASCII wiring (signal path)

```
             +5 V          +12 V
              |              |
        ┌─────┴─────┐    [ COM ]
        │  RELAY    │     M1  \  (NO)
  PB0 ──►IN  MODULE │      contact \___► 12 V LED (+) ─►[R]─► 12 V GND
        │ (M1)  GND ├──GND        (closes when IN is driven HIGH:
        └───────────┘             active-HIGH module, internal driver
                                  + flyback handled on-board)

  MCU PD2 ──┬── SW1 ── GND        (internal pull-up: idle HIGH, press = LOW)
            └── (internal pull-up)

  MCU PD1 (TXD) ──► USB-serial RX   @ 9600 8N1
```

## Why drive the module directly (and not through a ULN2803)

An ATmega pin sources ~20 mA — more than enough for a relay module's `IN`, which
only feeds an opto-LED or a transistor base through a resistor. The module's own
on-board transistor sinks the coil current and its on-board flyback diode clamps
the coil's back-EMF, so **all** the heavy lifting the ULN2803 would do for a bare
coil is already inside the module.

Key gotcha this project hit: **match the module's trigger polarity to the pin
level that means "on."** This is an **active-HIGH** module (`IN` HIGH → energised),
and the firmware drives PB0 HIGH for "on" (`relay_drive(true) → pin_high`), so they
agree with no inversion. An **active-LOW** module would need either its high-level
trigger jumper set, or `relay_drive()` inverted in `src/relay_fsm.c`.

> **If you ever revert to a bare relay coil:** *then* you need a ULN2803 (or a
> discrete transistor) plus a flyback diode. On a real **ULN2803A the pinout is
> pin 9 = GND, pin 10 = COM** (COM ties to the coil supply rail to clamp back-EMF).
> The Darlington is an inverting low-side sink, so a bare coil goes high-side to
> +Vcoil and low-side to the ULN output.
