# Theory — I2C Hello World Thermometer

## 1. The I2C Bus

I2C (Inter-Integrated Circuit, also called TWI — Two-Wire Interface) is a synchronous serial bus invented by Philips. It uses exactly two wires shared by all devices on the bus:

- **SDA** — Serial Data
- **SCL** — Serial Clock

### Open-drain and pull-up resistors

Both lines are **open-drain**: every device on the bus can only pull a line *low* (to GND) — it can never drive it high. Instead, a pull-up resistor connected to VCC pulls the line high when nobody is driving it low.

This design allows multiple devices to share a single wire without causing a short circuit. If two devices simultaneously pull the line low, that is still a valid low. The bus is only high when *every* device releases it.

**Consequence:** if you forget the pull-up resistors, both SDA and SCL float. The ATmega will see noise on the lines and I2C communication will fail silently or randomly.

Typical pull-up value at 100 kHz (Standard Mode): **4.7 kΩ**. Many breakout boards include them on-board.

### Bus transactions

Every I2C transaction follows this skeleton:

```
Master: START | ADDRESS + R/W̄ | DATA bytes ... | STOP
```

**START condition** — SDA falls while SCL is high. This is the only time SDA is allowed to change while SCL is high. It grabs the bus.

**Address frame** — 7 bits of device address followed by a direction bit:
- `0` = Master is writing to the slave
- `1` = Master is reading from the slave

The 8th bit (ACK) is driven by the *slave*: it pulls SDA low to acknowledge it recognised its own address. If no device responds, SDA stays high — a NACK.

**Data bytes** — each byte is followed by an ACK from the receiver. During a read, the master drives the ACK; it sends NACK on the *last* byte to signal "I'm done, release the bus."

**STOP condition** — SDA rises while SCL is high. Releases the bus.

**Repeated START** — a new START condition issued *before* a STOP. Used when the master wants to switch direction (write then read) without releasing bus ownership to another master. This project uses it: write the register pointer, then repeated START, then read the data.

---

## 2. ATmega328P TWI Peripheral

The ATmega328P has a hardware TWI peripheral that handles the low-level bit-banging automatically. The CPU only needs to:
1. Write a command word to **TWCR** (TWI Control Register)
2. Wait for **TWINT** (TWI Interrupt Flag) to be set — hardware clears it when the operation starts, sets it when done
3. Read the result from **TWDR** (TWI Data Register) or **TWSR** (TWI Status Register)

### Key registers

| Register | Purpose |
|----------|---------|
| `TWBR` | Bit Rate — sets SCL frequency |
| `TWCR` | Control — send commands (START, STOP, ACK, enable) |
| `TWDR` | Data — byte to transmit or byte just received |
| `TWSR` | Status — result code of last operation (0x08 = START sent, 0x40 = SLA+R ACK, etc.) |

### SCL frequency formula

```
SCL = F_CPU / (16 + 2 × TWBR × Prescaler)
```

With prescaler bits TWPS[1:0] = 00 (scalar = 1) and F_CPU = 16 MHz targeting 100 kHz:

```
TWBR = (16 000 000 / 100 000 − 16) / 2 = 72
```

### Why TWINT must be cleared by assignment, not `|=`

The TWINT flag is cleared by writing a **1** to it (unusual but intentional — prevents accidental clears). However, TWINT also serves as a "go" signal: writing 1 starts the next hardware operation.

Using `TWCR |= _BV(TWINT)` is dangerous: the read-modify-write reads the current TWCR, which may have stale TWSTA or TWSTO bits set, and replays them unintentionally. The driver always uses direct assignment:

```c
TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWSTA));  // start condition
```

---

## 3. LM75 Temperature Sensor

### How it measures temperature

The LM75 contains an on-chip **bandgap voltage reference** (a circuit whose output voltage is nearly constant across temperature) and a **sigma-delta ADC** that compares the forward voltage of an internal diode — which varies predictably with temperature — against that reference.

The result is a 9-bit two's complement value representing temperature in increments of **0.5 °C**, covering −55 °C to +125 °C.

### Internal registers

The LM75 has four registers selected by a **pointer byte** written by the master:

| Pointer | Register | Width | Purpose |
|---------|----------|-------|---------|
| 0x00 | Temperature (Temp) | 16-bit, read-only | Current temperature |
| 0x01 | Configuration (Conf) | 8-bit, R/W | Shutdown, thermostat mode, fault queue |
| 0x02 | Hysteresis (Thyst) | 16-bit, R/W | Thermostat lower threshold |
| 0x03 | Overtemperature (Tos) | 16-bit, R/W | Thermostat upper threshold |

The pointer register retains its value between transactions. After power-on it points to 0x00 (Temp), so in practice you only need to set it once (or each time you want a different register).

### Temperature register format

The 16-bit temperature value is read as two bytes:

```
Byte 1 (MSB): [D7  D6  D5  D4  D3  D2  D1  D0]  ← signed integer °C
Byte 2 (LSB): [HF  0   0   0   0   0   0   0 ]  ← HF = 0.5 °C flag
```

- **Byte 1** is a signed 8-bit two's complement integer representing whole degrees Celsius.
  - `0x19` = +25 °C
  - `0xFF` = −1 °C (two's complement)
- **Byte 2 bit 7 (HF)** adds 0.5 °C when set. Bits 6:0 are always 0.

Examples:

| Byte 1 | Byte 2 | Temperature |
|--------|--------|-------------|
| 0x19   | 0x00   | +25.0 °C    |
| 0x19   | 0x80   | +25.5 °C    |
| 0xFF   | 0x00   | −1.0 °C     |
| 0x80   | 0x00   | −128.0 °C (min) |

> **Known limitation in this project:** `print_uint8()` interprets `temp_high_byte` as `uint8_t`. At sub-zero temperatures the byte is a negative two's complement value; it will print as a large positive number (e.g. −1 °C → "255"). This is acceptable for a room-temperature thermometer but would need fixing for outdoor/cold applications.

### I2C address

The LM75 7-bit base address is `0b1001xxx`. The lower three bits are set by the hardware pins A2, A1, A0:

```
7-bit address = 0b1001 | (A2<<2) | (A1<<1) | A0
```

With A2=A1=A0=GND (all zero): address = **0x48**.

The 8-bit wire format appends the R/W̄ bit:
- Write: `0x48 << 1 | 0` = **0x90**
- Read:  `0x48 << 1 | 1` = **0x91**

Up to 8 LM75 devices can share one I2C bus by wiring the address pins differently.

---

## 4. Read Transaction Sequence

To read the temperature register the master must:

1. **Write the pointer** — tell the LM75 which register to read next
2. **Repeated START** — restart without releasing the bus
3. **Read two bytes** — the LM75 streams MSB then LSB

```
Master: START
Master: 0x90 (SLA+W)      ← Slave ACKs
Master: 0x00 (pointer)    ← Temp register; Slave ACKs
Master: repeated START
Master: 0x91 (SLA+R)      ← Slave ACKs
Slave:  Byte 1 (integer)  ← Master ACKs (more data coming)
Slave:  Byte 2 (fraction) ← Master NACKs (done), then STOP
```

The repeated START is necessary because: after writing the pointer the master wants to switch to read mode. A STOP followed by a new START would release bus ownership, allowing another master to intervene. In a single-master system this is harmless, but the repeated START is the correct I2C idiom regardless.
