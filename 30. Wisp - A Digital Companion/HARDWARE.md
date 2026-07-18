# HARDWARE — Digital Companion

ATmega328P @ 16 MHz. One MAX7221-driven 8×8 dot-matrix face, one HC-SR04
ultrasonic ranger, one pet button. Full net list: `docs/hardware_connections.md`.

## Pin table

| Signal              | MCU pin | Port/bit     | Direction | Notes                                       |
| :------------------ | :------ | :----------- | :-------- | :------------------------------------------ |
| MAX7221 DIN (MOSI)  | 17      | PB3          | out       | hardware SPI                                 |
| MAX7221 CLK (SCK)   | 19      | PB5          | out       | hardware SPI                                 |
| MAX7221 /LOAD (CS)  | 16      | PB2          | out       | latch; idles high; forced output (MSTR guard)|
| (MISO, unused)      | 18      | PB4          | in        | leave floating/input                         |
| HC-SR04 ECHO        | 14      | PB0 / ICP1   | in        | Timer1 input capture                         |
| HC-SR04 TRIG        | 6       | PD4          | out       | 10 µs ping pulse                             |
| Pet button          | 15      | PB1 / PCINT1 | in, pull-up | active-low; pin-change seeds PRNG          |
| PRNG entropy        | 23      | PC0 / ADC0   | in (float)| leave unconnected                            |
| Mic (Tier 3)        | 24      | PC1 / ADC1   | in        | scaffold only (`ENABLE_AUDIO`)               |
| Loop-scope point    | 5       | PD3          | out       | oscilloscope timing proof                    |
| Debug TXD           | 3       | PD1          | out       | only with `DEBUG_UART`                        |

**Timers:** Timer0 → 1 ms system tick; Timer1 → HC-SR04 ICP1 capture (/64,
4 µs/tick); Timer2 → free.

## SPI wiring (ASCII)

```
 ATmega328P                         MAX7221 + 8x8 matrix
 ┌──────────┐                       ┌───────────────────┐
 │      PB3 ├──────── DIN ─────────►│ DIN               │
 │      PB5 ├──────── CLK ─────────►│ CLK          SEG/ │──► 8x8 LED
 │      PB2 ├──────── LOAD/CS ─────►│ LOAD (CS)    DIG  │──► matrix
 │          │                       │ ISET ─[28k]─ VCC  │
 │          │                    ┌──┤ GND   VCC ├──┐    │
 └──────────┘                    │  └───────────────────┘
                                GND               +5V (+0.1µF, +10µF)
```

CS idles HIGH; a full frame is 8 × 16-bit writes (one MAX7221 DIGIT register per
matrix row), latched by CS's rising edge. SPI is mode 0, MSB-first, fosc/16
(1 MHz) — far under the MAX7221's 10 MHz ceiling.

## HC-SR04 ICP1 wiring note

ECHO goes to **PB0/ICP1** so Timer1's input-capture unit timestamps both echo
edges in hardware (no `pulseIn` busy loop). TRIG is any GPIO (**PD4**), pulsed
HIGH for 10 µs to start a ping. The capture ISR records the rising edge, flips
the edge-select to catch the falling edge, and stores the width; `width_µs / 58`
is the distance in cm. Prescaler /64 gives 4 µs/tick, so the 16-bit counter wraps
every 262 ms — longer than the sensor's ~38 ms "no-object" pulse, keeping the
unsigned edge subtraction valid.

```
 ATmega328P            HC-SR04
 ┌────────┐            ┌────────┐
 │    PD4 ├─── TRIG ──►│ TRIG   │
 │    PB0 │◄── ECHO ───┤ ECHO   │   (5 V logic; divide for 3.3 V MCU)
 │ (ICP1) │            │ VCC/GND│── +5V / GND  (+100µF local)
 └────────┘            └────────┘
```

## ADC / entropy

No analog divider is required (the LDR from the original spec was removed). ADC0
is left **floating** and sampled at boot for PRNG entropy; ADC1 is reserved for
the Tier-3 electret mic (unused unless built with `ENABLE_AUDIO`).

## Wiring diff vs. the `stopwatch` project

Only the **new / changed** connections (SPI DIN/CLK/LOAD on PB3/PB5/PB2 are
identical and reused):

| Change | stopwatch                     | digital_companion                         |
| :----- | :---------------------------- | :---------------------------------------- |
| removed| BTN_STARTPAUSE (PD6), BTN_CLEAR (PD7) | — (no dual buttons)                |
| added  | —                             | Pet button on **PB1** (PCINT1)            |
| added  | —                             | HC-SR04 **ECHO→PB0/ICP1**, **TRIG→PD4**   |
| added  | —                             | Loop-scope test point on **PD3**          |
| added  | —                             | ADC0 (PC0) floating entropy; ADC1 mic seam |
| display| single 8×8 (7-seg font)       | single 8×8 (raw no-decode face)           |
