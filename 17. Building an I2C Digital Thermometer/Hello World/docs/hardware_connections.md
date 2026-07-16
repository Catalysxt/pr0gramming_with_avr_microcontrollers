# Hardware Connections — I2C Hello World Thermometer

## Bill of Materials

| RefDes | Part | Notes |
|--------|------|-------|
| U1 | ATmega328P (on breadboard or Arduino Uno board) | 16 MHz external crystal |
| U2 | LM75 breakout board | Address pins A0/A1/A2 not exposed; hardwired to GND → I2C address 0x48 |
| J1 | USB-to-UART adapter (e.g. CH340, CP2102, FT232) | 3.3 V or 5 V logic |
| — | USBasp programmer | For flashing only |

---

## I2C Bus Notes

SDA and SCL are **open-drain lines**. They require pull-up resistors to VCC to function. Most LM75 breakout boards include 4.7 kΩ pull-ups on-board — check your specific board's schematic. If not present, add 4.7 kΩ resistors from SDA→VCC and SCL→VCC externally.

---

## Connection Table

| Net       | From (RefDes, Pin) | To (RefDes, Pin) | MCU Pin | Notes |
|:----------|:-------------------|:-----------------|:--------|:------|
| VCC_5V    | Supply, 5 V        | U1, VCC          | VCC     | — |
| VCC_5V    | Supply, 5 V        | U2, VCC          | —       | LM75 accepts 2.7–5.5 V |
| GND       | Supply, GND        | U1, GND          | GND     | — |
| GND       | Supply, GND        | U2, GND          | —       | — |
| GND       | Supply, GND        | J1, GND          | —       | Common ground required |
| I2C_SDA   | U1, PC4            | U2, SDA          | PC4     | Open-drain; pull-up to VCC required |
| I2C_SCL   | U1, PC5            | U2, SCL          | PC5     | Open-drain; pull-up to VCC required |
| LM75_A0   | U2, A0             | GND              | —       | Address bit 0 = 0 |
| LM75_A1   | U2, A1             | GND              | —       | Address bit 1 = 0 |
| LM75_A2   | U2, A2             | GND              | —       | Address bit 2 = 0 → 7-bit addr = 0x48 |
| UART_TX   | U1, PD1            | J1, RX           | PD1     | 19200 8N1 |

> **Note:** A2/A1/A0 are not exposed by the breakout board and are hardwired to GND internally. No external connections required for the address pins.

---

## ASCII Wiring Diagram

```
                    ATmega328P
                  .-----------.
            VCC --| VCC   PD1 |-- UART_TX ----> USB-UART (RX)
            GND --| GND       |
                  |           |
                  |       PC4 |-- SDA --+----> LM75 breakout (SDA)
                  |       PC5 |-- SCL --+----> LM75 breakout (SCL)
                  |           |         |
                  |    USBasp |     [4.7kΩ pull-ups to VCC]
                  |  (MOSI,   |    (may be on breakout board)
                  |   MISO,   |
                  |   SCK,    |         LM75 Breakout
                  |   RESET)  |       .-----------.
                  '-----------'  VCC--| VCC   SDA |-- SDA
                                 GND--| GND   SCL |-- SCL
                                      | A0=GND    |
                                      | A1=GND    |
                                      | A2=GND    |
                                      '-----------'
                                      (addr pins hardwired
                                       to GND internally)
```
