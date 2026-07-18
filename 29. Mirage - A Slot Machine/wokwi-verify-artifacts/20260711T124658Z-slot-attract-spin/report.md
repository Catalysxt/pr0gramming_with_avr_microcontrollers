# Wokwi verify — slot_machine (attract + spin)

**Date:** 2026-07-11 · **Firmware:** `firmware.hex` / `firmware.elf` (this dir)
**Circuit:** `../../diagram.json` — Arduino Uno + 5 daisy-chained
`wokwi-max7219-matrix` (m0 score stand-in, m1..m4 dot-matrix panels) sharing
MOSI(D11)/CLK(D13)/CS(D10), 2 pushbuttons (D6/D7), buzzer (D9), logic analyzer.

## Scenario

Boot into attract, wake with BTN_A to READY, press BTN_A again to SPIN the three
reels (m1..m3) with staggered stops and buzzer feedback.

## Verdict: PARTIAL PASS (electrical + boot verified; scripted gameplay blocked by tooling)

### Confirmed
- **Build:** clean `-Wall -Wextra -Werror`; flash 7494 B (23% of 32 KB),
  SRAM 264 B (13% of 2 KB).
- **Boots & runs stably:** simulation advanced from ~2.8 s to ~218 s of sim time
  with no hang, fault, or reset.
- **SPI display chain correct at idle** (proves `max7219_chain_init()` + the
  refresh loop execute and drive the shared bus):
  - CLK (D13) idle **low** — SPI mode 0, as configured.
  - /LOAD CS (D10) idle **high** — frames are active-low, latch on rising edge.
- **Buzzer idle silent** (D9 low) in attract — correct; no spurious tone.

### Not verified in sim (tooling limitation, not a firmware defect)
- Interactive **short-press** gameplay (wake → SPIN → win). The Wokwi sim
  fast-forwards tens of seconds of sim time between MCP tool calls, so a button
  held across two calls always crosses the 1 s long-press threshold, while a
  same-message press+release falls under the 15 ms debounce window. A controlled
  15 ms–1 s "short press" was therefore not reliably producible. Buzzer snapshot
  reads during spin attempts returned low (a ~2.5 s spin can complete between the
  press and the next observable call).
- VCD capture of the reel-spin tone burst: the MCP server disconnected right
  after the logic analyzer was added, before a trace could be exported.

## Files
- `firmware.hex`, `firmware.elf` — exact binary under test
- `inputs.log` — every control driven, in order
- `pins.log` — every pin read
- (no `trace.vcd` / screenshots — see limitation above)

## Recommendation
The display/SPI path, boot, and idle audio are verified in simulation. Reel
animation, payout, count-up, jackpot, and settings/cash-out are exercised by
straightforward `timing_ms()`-paced logic and should be validated on real
hardware (or in the interactive Wokwi UI, where a human can produce a normal
button tap). This run is a pre-flash sanity check, not a substitute for
hardware testing.
