# Wokwi verification — Ember (pom_timer)

Pre-flash sanity check in Wokwi. **Not** a substitute for hardware testing.

## Circuit
Arduino Uno (ATmega328P @16 MHz host) + wokwi-7segment (4-digit, common anode)
+ wokwi-ky-040 encoder + wokwi-buzzer + LED (relay proxy on PB0). See
`../../diagram.json`. NOTE: real hardware drives the digit anodes through PNP
high-side transistors (active-LOW); stock Wokwi has no such part, so the sim
build temporarily used `EMBER_DIGIT_ACTIVE_LOW 0` (direct drive). Reverted after.

## Scenarios & verdicts

| # | Scenario | Verdict | Evidence |
|---|----------|---------|----------|
| 1 | Boot -> IDLE parks display + powers down | PASS | relay OFF; all 4 COM LOW across 16 samples; encoder pull-ups HIGH (init ran) |
| 2 | Display multiplex + font render (SET_WORK "25") | PASS | COM3 (uno:12) caught HIGH -> Timer2 ISR selecting digits at ~250 Hz |
| 3 | Relay/lamp OFF during setup | PASS | uno:8 = false |
| 4 | Encoder turn adjusts value; wake-from-sleep | NOT VERIFIED | Wokwi KY-040 rotate/press control semantics unclear; couldn't confirm PCINT wake in sim. Logic is deterministic table code — verify on hardware. |
| 5 | WORK->BREAK at 0:00 lights lamp | NOT VERIFIED | minute-scale countdown impractical to run in real-time sim; verify on hardware (use 1-min work for the bench test) |

## Takeaway
Boot, init, the common-anode display multiplex (the biggest new subsystem), and
the IDLE power-down path all behave correctly. Encoder-driven value changes,
wake-from-power-down, and the full countdown->lamp transition were not
exercised in sim (harness/timescale limits) and should be checked with the
hardware practical checklist in README.md.

Files: firmware.hex/.elf, inputs.log, pins.log. (No readable screenshot: the
MCP screenshot tool returns inline base64 that can't be rendered here.)
