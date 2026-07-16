# Hardware Connections — Logging Thermometer + DS3231

## Bill of Materials

| RefDes | Part | Notes |
|--------|------|-------|
| U1 | ATmega328P | 16 MHz external crystal |
| U2 | LM75 breakout | A0/A1/A2 hardwired to GND internally -> I2C address 0x48 |
| U3 | 24LC256 (DIP-8 or breakout) | A0/A1/A2 to GND -> I2C address 0x50 |
| U4 | DS3231 RTC module | No address pins -- fixed I2C address 0x68; 32.768 kHz crystal + CR2032 backup cell already populated on the module |
| D1 | LED (any colour) | Heartbeat indicator on PB0 |
| R1 | 330 Ohm resistor | LED current limiter |
| J1 | USB-to-UART adapter | CH340, CP2102, or FT232 |
| — | USBasp programmer | For flashing only |

---

## I2C Bus Notes

SDA and SCL are **open-drain**; all three devices share the same two wires. The single pair of pull-up resistors (4.7 kOhm to VCC) already on the bus for U2/U3 is sufficient for U4 as well -- **do not add a third pair**. Three distinct 7-bit addresses (0x48, 0x50, 0x68) coexist without conflict; if a second LM75 or DS3231 is ever added, address-strap or bus-scan collisions become a real risk (DS3231 has no address pins at all, so only one can ever be on a bus).

The 24LC256 WP (Write Protect) pin must be tied to **GND** to enable write operations. If left floating or pulled high, all writes are silently ignored.

The DS3231 module's **SQW** and **32K** output pins are left **unconnected**. This project polls the clock over I2C on a schedule the firmware already controls; it has no use for the DS3231's alarm interrupt (SQW) or auxiliary 32.768 kHz clock output (32K). Leaving them floating is the module manufacturer's documented default state.

---

## Connection Table

| Net | From (RefDes, Pin) | To (RefDes, Pin) | MCU Pin | Notes |
|:----|:-------------------|:-----------------|:--------|:------|
| VCC_5V | Supply, 5 V | U1, VCC | VCC | — |
| VCC_5V | Supply, 5 V | U2, VCC | — | LM75 accepts 2.7-5.5 V |
| VCC_5V | Supply, 5 V | U3, VCC (pin 8) | — | 24LC256 accepts 2.5-5.5 V |
| VCC_5V | Supply, 5 V | U4, VCC | — | DS3231 module accepts 2.3-5.5 V |
| GND | Supply, GND | U1, GND | GND | — |
| GND | Supply, GND | U2, GND | — | — |
| GND | Supply, GND | U3, GND (pin 4) | — | — |
| GND | Supply, GND | U4, GND | — | — |
| GND | Supply, GND | J1, GND | — | Common ground required |
| I2C_SDA | U1, PC4 | U2, SDA | PC4 | Open-drain; shared bus |
| I2C_SDA | U1, PC4 | U3, SDA (pin 5) | PC4 | Same wire as U2 SDA |
| I2C_SDA | U1, PC4 | U4, SDA | PC4 | Same wire, third device on the bus |
| I2C_SCL | U1, PC5 | U2, SCL | PC5 | Open-drain; shared bus |
| I2C_SCL | U1, PC5 | U3, SCL (pin 6) | PC5 | Same wire as U2 SCL |
| I2C_SCL | U1, PC5 | U4, SCL | PC5 | Same wire, third device on the bus |
| DS3231_SQW | U4, SQW | — | — | Not connected -- alarm interrupt unused |
| DS3231_32K | U4, 32K | — | — | Not connected -- auxiliary clock output unused |
| LM75_A0 | U2, A0 | GND | — | Addr bit 0 = 0 (internal to breakout) |
| LM75_A1 | U2, A1 | GND | — | Addr bit 1 = 0 (internal to breakout) |
| LM75_A2 | U2, A2 | GND | — | Addr bit 2 = 0 -> 7-bit addr = 0x48 |
| EEPROM_A0 | U3, A0 (pin 1) | GND | — | Addr bit 0 = 0 |
| EEPROM_A1 | U3, A1 (pin 2) | GND | — | Addr bit 1 = 0 |
| EEPROM_A2 | U3, A2 (pin 3) | GND | — | Addr bit 2 = 0 -> 7-bit addr = 0x50 |
| EEPROM_WP | U3, WP (pin 7) | GND | — | WP=GND enables writes; WP=VCC locks chip |
| LED_ANODE | D1, anode | R1, pin 1 | — | — |
| LED_K | D1, cathode | U1, PB0 | PB0 | Sinks current; PB0 low = LED on |
| LED_R | R1, pin 2 | VCC | — | 330 Ohm; ~13 mA at 5 V |
| UART_TX | U1, PD1 | J1, RX | PD1 | 9600 8N1 |
| UART_GND | GND | J1, GND | GND | Common ground |

---

## ASCII Wiring Diagram

```
              ATmega328P
            .-------------.
      VCC --| VCC    PD1  |-- UART_TX ---------> USB-UART (RX)
      GND --| GND         |
            |             |
            |        PC4  |-- SDA ---+--+--+----> LM75 breakout (SDA)
            |        PC5  |-- SCL ---+--+--+----> LM75 breakout (SCL)
            |             |    |     |  |
            |        PB0  |-- LED0   |  |  [4.7k pull-ups to VCC]
            |             |    |     |  |  (one pair for whole bus)
            |  (USBasp)   |  [330R]  |  |
            '-------------'    |     |  |
                              GND    |  |
                                     |  |
                         24LC256 (DIP-8)|
                        .-----------.  |
                  GND --| A0 (1) VCC|--+-- VCC
                  GND --| A1 (2) WP |-----GND   (WP=GND enables writes)
                  GND --| A2 (3)SCL |-----SCL
                  GND --| GND   SDA |-----SDA
                        '-----------'
                                        |
                              DS3231 module
                        .-----------------.
                  VCC --| VCC     SQW  NC |
                  GND --| GND     32K  NC |
                        | SDA  -----------+-- SDA
                        | SCL  -----------+-- SCL
                        '-----------------'
```

> **24LC256 DIP-8 pinout:** 1=A0, 2=A1, 3=A2, 4=GND, 5=SDA, 6=SCL, 7=WP, 8=VCC
>
> **DS3231 module pinout:** 32K, SQW, SCL, SDA, VCC, GND (order as printed on most breakout silkscreens)
