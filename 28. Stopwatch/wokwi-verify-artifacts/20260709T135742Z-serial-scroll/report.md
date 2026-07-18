# Wokwi verification — serial-driven scrolling text (MAX7219)

**Scenario:** Type a line over UART (9600 8N1); ≤8 chars render statically on the
8 digits, >8 chars scroll right-to-left once. Verified at the SPI-bus level by
decoding a logic-analyzer VCD of DIN/CLK/CS.

**Circuit:** Arduino Uno (ATmega328P) + `wokwi-max7219-matrix` + `wokwi-logic-analyzer`.
Wokwi has no MAX7219-driven 7-seg part, so the matrix is a **protocol stand-in** —
its DIN/CLK/CS interface is byte-for-byte identical to the real two 4-digit
displays, so the firmware path exercised is exactly the one that runs on hardware.
Segment→letter *shapes* are therefore validated by the decoded register bytes, not
by the matrix's visual (which lights cells, not 7-seg strokes).

## Verdict: PASS

### Init sequence (`trace.vcd`) — every frame exactly 16 bits
DISPTEST=0x00, DECODE=0x00 (no-decode), SCANLIMIT=0x07 (all 8 digits),
INTENSITY=0x07 (=0x0F/2), DIGIT0..7=0x00, SHUTDOWN=0x01 (run). Matches
`max7219_init()` exactly.

### Static render of "HELLO" (`trace.vcd`)
Digit registers received the exact font bytes, left-aligned + blank-padded:
DIGIT0=0x37(H) 0x4F(E) 0x0E(L) 0x0E(L) 0x7E(O) 0x00 0x00 0x00. ✓

### Scroll of "ABCDEFGHI" (`trace-scroll.vcd`) — 19 windows
Text enters from the right edge, advances one digit per frame, exits left, then
blanks. Each character's byte is correct (A=0x77 B=0x1F C=0x4E D=0x3D E=0x4F
F=0x47 G=0x5E H=0x37 I=0x30). ✓

## Bug found & fixed during verification
`readString` (shared driver) terminates only on CR; the Wokwi monitor sends LF,
so the first build hung with nothing rendered. Replaced with a local `read_line()`
in `main.c` that accepts CR **or** LF (and skips leftover enders for CRLF). Real
PuTTY (CR) and LF-only monitors now both work.

## Files
- [trace.vcd](trace.vcd) — init + static "HELLO" (791 samples)
- [trace-scroll.vcd](trace-scroll.vcd) — scroll of "ABCDEFGHI" (5767 samples)
- [inputs.log](inputs.log) · [serial.log](serial.log)
- [firmware.hex](firmware.hex) · [firmware.elf](firmware.elf)

*Simulation is a pre-flash sanity check, not a substitute for real-hardware testing.*
