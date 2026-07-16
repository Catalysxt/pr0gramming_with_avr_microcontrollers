# Theory — Logging Thermometer

This project builds on the I2C and LM75 theory covered in `hello_world/docs/Theory.md`. Read that first. This document covers the new concepts introduced here: EEPROM storage physics, the 24LC256 protocol, and the temperature compression scheme.

---

## 1. EEPROM — How bits are stored

EEPROM (Electrically Erasable Programmable Read-Only Memory) stores each bit in a **floating-gate transistor**. The floating gate is surrounded by oxide insulation and has no physical connection to anything. Charge is injected onto it using **Fowler-Nordheim tunneling** (a quantum mechanical effect where electrons pass through a thin oxide layer under a high electric field). Once charged, the gate alters the transistor's threshold voltage, representing a stored '0' or '1'. The charge persists for decades because the oxide blocks leakage.

To erase, the electric field is reversed and charge is pulled back off the floating gate.

### Write endurance

Every write/erase cycle degrades the oxide slightly. The 24LC256 is rated for **1,000,000 write cycles per byte**. For a logging application writing every 60 seconds, that is:

```
1,000,000 cycles / (1 write/60 s) = 60,000,000 s ≈ 1.9 years
```

If a single address is written every 60 seconds continuously, it wears out in under 2 years. The current project writes `CURRENT_LOCATION_POINTER` (address 0x0000) on **every logging cycle** in addition to writing the temperature byte. That address takes double wear. For long deployments, consider updating the pointer less frequently (e.g. every 10 readings).

### Write cycle time (Twc)

After a write command the 24LC256 enters an internal self-timed write cycle. During this period it ignores all I2C traffic. The datasheet specifies a maximum of **5 ms** (Twc). The driver calls `_delay_ms(5)` after every byte or word write. Attempting a second write before Twc expires causes the transaction to be silently ignored.

---

## 2. 24LC256 Protocol

The 24LC256 is a 256 kbit (32 KB, addresses 0x0000–0x7FFF) I2C EEPROM. It shares the SDA/SCL bus with the LM75. Because both devices have different I2C addresses (LM75=0x48, 24LC256=0x50) they coexist transparently.

### I2C address

The 24LC256 control byte follows the same format as other I2C devices:

```
0b 1010 A2 A1 A0 R/W
```

With A2=A1=A0=GND: 7-bit address = **0x50**, write byte = **0xA0**, read byte = **0xA1**.

### 16-bit address

The 24LC256 has 32 KB of address space, requiring 15 address bits. The master sends the address as **two bytes** after the control byte: high address byte (A15..A8) then low address byte (A7..A0).

### Byte write sequence

```
Master: START
Master: 0xA0 (SLA+W)
Master: addr_high (A15..A8)
Master: addr_low  (A7..A0)
Master: data byte
Master: STOP
[24LC256 internal write cycle: up to 5 ms]
```

### Random read sequence

A "dummy write" first loads the internal address counter:

```
Master: START
Master: 0xA0 (SLA+W)
Master: addr_high
Master: addr_low
Master: repeated START
Master: 0xA1 (SLA+R)
Slave:  data byte
Master: NACK + STOP
```

### Sequential read (log dump optimisation)

The current `[p]` command calls `EEPROM_read_byte()` for each address, which is a complete I2C transaction per byte (~45 us at 100 kHz). For 32764 readings that is ~1.5 seconds of bus traffic — acceptable but slow. A future optimisation is to open a single transaction with a repeated START and ACK each byte until the last, letting the EEPROM auto-increment its address counter. The driver would need a new `EEPROM_read_sequential()` function.

### Page write

The 24LC256 can write up to **64 bytes** in a single transaction (one internal write cycle instead of 64). All bytes must fall within the same 64-byte aligned page (address bits A5..A0 are an internal page counter that wraps at 64). The `EEPROM_clear_all()` function in the driver uses page writes to zero the entire chip in ~2.56 seconds (512 pages × 5 ms).

---

## 3. Temperature Compression

The LM75 produces a 9-bit result: 8-bit signed integer °C and a 0.5 °C flag bit. Stored naively as two bytes, 32 KB holds 16,382 readings. The code instead packs the 9 bits into **one byte**:

```c
uint8_t temp_byte = (temp_high_byte << 1) | (temp_low_byte >> 7);
```

- `temp_high_byte << 1` shifts the integer °C into bits[7:1]
- `temp_low_byte >> 7` extracts the 0.5 °C flag (bit 7 of the LSB) into bit[0]

Decoding:
```c
integer_part = temp_byte >> 1;      // bits[7:1] back to bits[6:0]
half_degree  = temp_byte & 1;       // bit[0]
```

This doubles capacity to **32,764 readings** per chip. The trade-off is that negative temperatures are stored incorrectly — the sign bit of `temp_high_byte` occupies bit 7 of `temp_byte` after the shift, which `print_uint8` then treats as unsigned. For a room-temperature logger this is fine.

---

## 4. EEPROM Address Layout

```
Address   Content
0x0000    CURRENT_LOCATION_POINTER high byte  (uint16 — next write address)
0x0001    CURRENT_LOCATION_POINTER low byte
0x0002    SECONDS_POINTER high byte           (uint16 — sample interval)
0x0003    SECONDS_POINTER low byte
0x0004    Temperature reading #1              (uint8, compressed)
0x0005    Temperature reading #2
  ...
0x7FFF    Temperature reading #32764 (max)
```

On a **fresh or erased chip**, addresses 0x0000–0x0003 return 0xFF, so `CURRENT_LOCATION_POINTER` = 0xFFFF and `SECONDS_POINTER` = 65535 (~18 hours). The `[e]` menu command clears the chip and writes `MEMORY_START` (4) and the current `seconds_delay` to the config words. **Always run `[e]` on first use.**

---

## 5. Two I2C Devices on One Bus

The LM75 and 24LC256 share SDA and SCL. The master addresses each by its unique 7-bit address (0x48 and 0x50). Only one device responds to each transaction — the other ignores it. Pull-up resistors are shared between both devices; one pair of 4.7 kΩ resistors is sufficient for the entire bus.
