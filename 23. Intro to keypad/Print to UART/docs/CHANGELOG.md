# Changelog

## [2026-06-28] - Session: initial project creation

### Added

- `main.c`: New project derived from `hello_world_keypad`. Keypad scanning
  logic (rows PB0-PB3, columns PC0-PC3, release-wait, two-sample debounce,
  row-settle delay, row scan) carried over unchanged.
- `main.c`: USART output — `initUSART()` configures USART0 at 9600 8N1.
  Each keypress transmits `"You pressed key: X\r\n"` via `printString()` and
  `transmitByte()` from `extern_libraries/USART.h`.
- `main.c`: Startup banner `"Keypad ready\r\n"` so the terminal confirms the
  MCU is live before the first keypress.

### Changed

- PORTD LED output removed entirely (was `DDRD = 0xFF` + `PORTD = ...` in
  `hello_world_keypad`). USART0 TX is on PD1 — writing to PORTD while UART is
  active would corrupt serial data.
- `reverse_byte()` helper removed — not needed without LED output.

### Files touched

- `main.c`

### Why

- New project goal: confirm keypad identity over a serial terminal rather than
  reading an 8-bit LED pattern.
- PD1 conflict forced removal of PORTD LED output; UART is the sole output
  channel in this variant.
