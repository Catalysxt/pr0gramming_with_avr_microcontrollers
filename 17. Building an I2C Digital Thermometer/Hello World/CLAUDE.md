# My CLAUDE.md — A Reusable template for AVR projects

Drop this into the root of any AVR bare-metal project as `CLAUDE.md`. Loaded automatically every session.

### CLAUDE.md

Reusable context file for bare-metal AVR-C projects. This file is loaded automatically every session — keep it accurate.

### Project identity — confirm before writing a single line

```c
Target MCU:    ATmega328P
Clock:         16 MHz external crystal (F_CPU=16000000UL)
Programmer:    USBasp
Toolchain:     avr-gcc + avrdude (Makefile-based)
Language:      C (bare-metal, no Arduino framework)
```

If any of the above differ for the current project, **stop and confirm with the user before proceeding**.

### Project Structure

```
workspace_root/
├── extern_libraries/              # shared across ALL projects
│   ├── i2c.h / i2c.c
│   ├── USART.h / USART.c
│   └── lcd_driver/
├── chapter_N_topic/               # chapter folder — NO Makefile here
│   ├── CLAUDE.md
│   ├── hello_world/               # each sub-project is fully self-contained
│   │   ├── Makefile               # LIBDIR = ../../extern_libraries
│   │   ├── src/                   # main.c + project-specific .c/.h
│   │   ├── docs/                  # CHANGELOG.md, Theory.md, hardware_connections.md
│   │   └── build/                 # generated, gitignored — ALL artifacts land here
│   └── next_project/
│       └── ...
└── standalone_project/            # simple one-off projects keep the flat layout
    ├── Makefile                   # LIBDIR = ../extern_libraries
    ├── src/
    ├── docs/
    └── build/
```

- IMPORTANT: `extern_libraries/` is a **sibling** of every project, never a child. Drivers are shared, not duplicated.
- All drivers used by more than one project live in `extern_libraries/`.
- `extern_libraries` must not contain pin definitions as this can vary from project to project.
- Each sub-project owns its own `Makefile`, `README.md`, `src/`, `docs/`, and `build/`.
- **`LIBDIR` depth depends on nesting.** A sub-project inside a chapter folder uses `../../extern_libraries`; a standalone project uses `../extern_libraries`.

### `extern_libraries/` discipline

- Drivers here are shared across every project. A careless edit can break unrelated code.
- **Never duplicate** a driver into a project's `src/`. If the driver doesn't do what you need, *improve the driver* — don't fork it.
- **Never edit a driver silently.** Before any change, tell the user in chat what you intend to change and why. Wait for confirmation. This is crucial — a change may break another project.
- **Justify the change in `docs/CHANGELOG.md`** of the project that triggered the edit.
- **Breaking changes require versioning.** Bump the driver's version macro (e.g. `LCD_DRIVER_VERSION`) and list every project that calls it.
- Each driver folder has its own mini `README.md` documenting its API.

### Required project documentation (`docs/`)

Every project's `docs/` folder must contain:

1. CHANGELOG.md`** — see format below.
2. **`Theory.md`** — explains the physics/electronics theory behind every non-trivial component in the project. Examples: Solenoids → electromagnetism, back-EMF, why a flyback diode is needed. IR remote → 38 kHz carrier, NEC protocol framing, photodiode demodulation. HD44780 LCD → DDRAM vs CGRAM, busy flag timing.
3. `hardware_connections.md`** — the hardware connection table (format below).
4. Schematic wire list** and/or ASCII wiring diagram.
5. Any datasheet PDFs referenced in code comments.

Hardware connection table format

```c
| Net             | From (RefDes, Pin) | To (RefDes, Pin) | MCU pin | Notes        |
| :-------------- | :----------------- | :--------------- | :------ | :----------- |
| MP3_VCC         | U1, 3V3            | MP3_1, VCC       | 3V3     | -            |
| MP3_RX          | U1, D7             | MP3_1, RX        | D7      | -            |
| MP3_TX          | U1, D6             | MP3_1, TX        | D6      | -            |
| MP3_SPK1        | MP3_1, SPK1        | SPK1, +          | -       | Audio output |
```

### docs/CHANGELOG.md` format

Use [Keep a Changelog](https://keepachangelog.com/) style so future-you can skim it fast. Abide my the structure /template below

```c
## [2026-06-27] - Session: keypad debounce

### Added

- pin_t struct in extern_libraries/pin/

### Changed
- lcd_driver: replaced delay_ms(2) with busy-flag polling (datasheet p.24)

### Fixed
- Overflow in chained multiply (Ascent calc)

### Files touched
- src/main.c, extern_libraries/lcd_driver/lcd_driver.c

### Why
- ...
```

### Coding Rules

1. **Bare-metal only.** No Arduino libraries or APIs — no `LiquidCrystal.h`, `tone()`, `millis()`, `delay()`, `Serial.*`, etc.
2. **Prefer avr-libc.** Use `<avr/io.h>`, `<avr/interrupt.h>`, `<avr/eeprom.h>`, `<avr/pgmspace.h>`, `<util/atomic.h>`, `<util/delay.h>`, `<avr/sleep.h>`, `<avr/power.h>` wherever possible. When importing one, add a comment naming the specific APIs being used and why — this elevates the reader's understanding.
3. **Reference the datasheet** in comments whenever a register, bit, or timing decision comes from one (e.g. `// HD44780 datasheet p.24 — busy flag`).
4. **`volatile`** for any ISR-shared global. 
5. **Educational comments.** Convey the *why*, not just the *what*. A line that sets a bit should explain what that bit *does in this context*.
6. **No magic numbers.** Every literal that isn't `0` or `1` must be a `#define` or `static const`.
7. **EEPROM is wear-aware.** Use `eeprom_update_*`, never `eeprom_write_*`, and only write when the value has actually changed.
8. **Don't guess.** If a pin, register, timing, or part number isn't specified, ask the user. Do not assume.
9. **Never silently rewrite the Makefile.** If it needs changes, list them in the CHANGELOG entry first and surface the change in chat.
13. **Build artifacts in `build/`.** The Makefile must route `.elf`, `.hex`, `.map`, `.eeprom`, and `.lst` into the `build/` subdirectory via a `BUILDDIR` variable. Never let generated files land in the project root.
10. **ISRs stay short.** Set a `volatile` flag and handle work in `main()`.
11. **`static` for file-local helpers** — anything not declared in a `.h` should be `static`.
12. **Memory budget check.** At the end of each session, report `.text` (flash) and `.data + .bss` (SRAM) sizes from `avr-size`. Warn if usage exceeds 75% of 32 KB flash or 2 KB SRAM.

### Naming Conventions

- **snake_case** for all variables, functions, and file names
- UPPER_SNAKE_CASE for #define macros and EEMEM constants
- g_ prefix for globals (you have this)
- s_ prefix for static file-local globals
- ee_ prefix (or an eemem struct) for EEPROM-backed variables
- Structs: snake_case_t suffix (e.g. pin_t)
- Header guards: PROJECT_MODULE_H_

### Pin Struct

Use this format:

```c
typedef struct {
    volatile uint8_t *ddr;     // data direction register
    volatile uint8_t *port;    // output / pullup register
    volatile uint8_t *pin_reg; // input register
    uint8_t           bit;     // bit index 0..7
} pin_t;

// canonical helpers in extern_libraries/pin/
void pin_set_output(pin_t p);
void pin_set_input(pin_t p, bool pullup);
void pin_high(pin_t p);
void pin_low(pin_t p);
bool pin_read(pin_t p);

// All pin_t instances live in hardware_connections.h as static const.
```

### hardware_connections.h` rules

- All `pin_t` instances live in `src/hardware_connections.h` as `static const`. Drivers receive `pin_t` values as arguments — they never reference port/pin macros directly
- Header only.** No `.c` file — only `#define`s and `static const pin_t`s.
- Single source of truth.** If a new pin is needed, edit this file, *not* the driver.
- **Group by subsystem** with banner comments, for example:

```c
// ---- LCD (HD44780, 4-bit) ----

static const pin_t LCD_RS = { &DDRB, &PORTB, &PINB, PB0 };

static const pin_t LCD_E  = { &DDRB, &PORTB, &PINB, PB1 };

// ---- Keypad (4x4 matrix) ----

static const pin_t KEYPAD_ROW_0 = { &DDRD, &PORTD, &PIND, PD2 };
```

### README.md required sections

Each project's top-level `README.md` must contain:

1. **Overview** — one paragraph: what the project does and what's interesting about it.
2. **Hardware** — bill of materials + link to `docs/hardware_connections.md`.
3. **Theory** — short summary, link to `docs/Theory.md`.
4. **Build & Flash** — exact commands.
5. **How to use** — runtime walkthrough (buttons, menus, expected output).
6. **Practical checklist** - A list of steps the user can perform to validate the program
7. **Memory footprint** — latest flash/SRAM numbers.
8. **Stretch goals** — what to add next.

#### Example of practical checklist

Use this as a template/guide. Steps should be as small and digestible. 

#### Example for scrolling_text-uart

1. Open a serial terminal (PuTTY, screen, minicom) at 9600 8N1 on the AVR's COM port.
2. Serial terminal should display: `Enter text:` 
3. Type `The world is not round, but flat!` and press Enter. The text should scroll right-to-left once across row 0 while row 1 shows << scrolling >>.
4. After the scroll finishes the LCD goes blank and serial terminal displays `Enter text:`  on a newline.

### Extensions
This section is dedicated to brainstorming ideas for how this project can be expanded on. The user is always seeking to grow his competency. This section is dedicated to achieving this aim.