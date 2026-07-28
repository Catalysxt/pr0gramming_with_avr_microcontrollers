# Digital Dash — A Bare-Metal AVR Codebase Walkthrough

## What This Project Is

Reading the code end-to-end, Digital Dash is a side-scrolling obstacle-dodge game — spiritually a Chrome "Dino Run" clone — running on an ATmega328P clocked at 16 MHz from an external crystal, displayed on a 16×2 HD44780-compatible character LCD, with two tactile pushbuttons (jump and duck), a passive piezo buzzer for sound effects, a floating ADC pin for random-number seeding, and internal EEPROM for persisting the high score across power cycles. There is no USART, no SPI, and no I²C in this project; the peripherals in play are **Timer0** (system millisecond tick), **Timer2** (buzzer tone generation), the **ADC** (one-shot noise seed at boot), **pin-change interrupts** (button edge detection), **EEPROM** (high score storage), and **GPIO** on three ports (LCD data/control bus, button inputs, buzzer output). The entire game runs in a cooperative super-loop; the only two interrupt service routines are the Timer0 compare-match (increments a millisecond counter) and the PCINT1 handler (debounces buttons and fires sound effects).

## Project File Structure

```
digital_dash/
├── Makefile                        # Build rules, fuse definitions, flash targets
├── README.md
├── digital_dash.elf                # Linked ELF (avr-gcc output)
├── digital_dash.hex                # Intel HEX for flash programming
├── digital_dash.map                # Linker map (symbol addresses, section sizes)
├── docs/
│   ├── CHANGELOG.md
│   ├── Theory.md
│   ├── digital_dash_amega328p_port_prompt.md
│   └── hardware_connections.md     # Pin table, ASCII schematic, BOM
├── src/
│   ├── main.c                      # Top-level state machine (splash → play → gameover)
│   ├── hardware_connections.h      # Pin definitions — header-only, no .c companion
│   ├── sys_tick.c / .h             # Timer0 CTC → 1 ms system tick + sys_millis()
│   ├── buttons.c / .h             # PCINT1 ISR debounce for jump (PC0) and duck (PC1)
│   ├── buzzer.c / .h              # Timer2 CTC + OC2A toggle → non-blocking piezo tones
│   ├── game.c / .h                # Core game logic: obstacles, collision, scoring
│   ├── sprites.c / .h            # PROGMEM glyph data → LCD CGRAM loader
│   ├── rng.c / .h                 # ADC2 noise seed + 16-bit xorshift PRNG
│   └── high_score.c / .h         # EEPROM read/write with magic-byte sentinel
└── build/                          # (empty — object files currently in src/)

../lcd_driver/                      # External shared library (sibling directory)
├── lcd_driver.c                    # HD44780 4-bit HAL: init, busy-flag polling, R/W
└── lcd_driver.h                    # GPIO macros, command opcodes, public API
```

---

## The Build System and Fuse Configuration

The [Makefile](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/Makefile) sets `MCU = atmega328p` and `F_CPU = 16000000UL`, meaning every `_delay_ms()` / `_delay_us()` call and every timer-period calculation the code depends on is compiled against a 16 MHz assumption. The compiler flags are worth a close look: `-Os` asks for optimisations biased toward small code size (critical when you have 32 KB of flash), `-std=gnu99` enables GNU extensions to C99 (necessary for inline asm and `__attribute__` syntax that avr-libc uses), and `-funsigned-char` makes plain `char` unsigned, which matters because LCD character codes and register bytes should never be sign-extended. `-ffunction-sections` and `-fdata-sections` paired with the linker flag `-Wl,--gc-sections` let the linker discard any function or constant that is defined but never referenced — without this pair, the entire `.text` of every compiled object file would be linked in, even dead code. The Makefile also pulls in an external LCD driver from `../extern_libraries/lcd_driver/lcd_driver.c`; all project-local source lives under `src/`.

The fuse bytes are defined explicitly:

| Fuse  | Value  | Meaning |
|-------|--------|---------|
| LFUSE | `0xF7` | External full-swing crystal oscillator, slow start-up (SUT1:0 = 11). Full-swing mode (CKSEL3:0 = 0111) drives the crystal harder than low-power mode and is recommended when the crystal is physically close to noisy digital I/O — robustness at the cost of ~1 mA extra current. |
| HFUSE | `0xD9` | SPI programming enabled (SPIEN = 0), EESAVE **disabled** (bit 3 = 1, meaning EEPROM *is* erased on a chip-erase). A separate make target `set_eeprom_save_fuse` flips this to `0xD7` (EESAVE = 0) so that high scores survive re-flashing. |
| EFUSE | `0x07` | Brown-out detection disabled. This means the MCU has no voltage supervisor — if VCC droops slowly (e.g. dying battery), the CPU may execute garbage instructions before resetting. For a bench-powered game this is acceptable; for battery operation you would want BOD at 2.7 V or 4.3 V. |

> **Explore this yourself.** Open the ATmega328P datasheet Table 27-7 (Low Fuse Byte) and Table 9-1 (Clock Source Selection). Starting from `LFUSE = 0xF7`, decode each bit field and verify that CKSEL3:0 = 0111 indeed selects "Full Swing Crystal Oscillator". Then look at what would happen if you changed CKSEL to 0110 — would the prescaler bits (CKDIV8, the top bit of LFUSE) still give you 16 MHz at the CPU?

---

## Hardware Peripherals in Detail

### GPIO and Pin Assignments

All pin definitions live in [hardware_connections.h](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/hardware_connections.h), a header-only file with no `.c` companion. The project uses three ports:

- **PORTB**: PB0–PB2 are the LCD control lines (RS, R/W, Enable), defined inside the LCD driver header rather than `hardware_connections.h` — the comment notes this is "for cross-reference" only. PB3 is `BUZZER_BIT`, chosen specifically because PB3 is the hardware output-compare pin `OC2A` for Timer2 — the timer's waveform generator can toggle this pin automatically without any CPU intervention once configured.
- **PORTC**: PC0 is the jump button (PCINT8), PC1 is the duck button (PCINT9), and PC2 is the ADC seed pin left intentionally floating. The code explicitly notes that PC0 and PC1 cannot serve as noise sources because their internal pull-ups clamp the voltage to near-VCC.
- **PORTD**: PD4–PD7 are the LCD 4-bit data bus. PD0 and PD1 (the USART pins) are left untouched by the LCD driver's masked-write macros — a deliberate design choice visible in `LCD_DATA_WRITE()` in [lcd_driver.h](file:///c:/Users/User/Coding/my_avr_programming/lcd/lcd_driver/lcd_driver.h#L169), which reads-modifies-writes only the upper nibble.

### Timer0 — The 1 ms System Tick

[sys_tick.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/sys_tick.c) configures Timer0 in CTC (Clear Timer on Compare) mode to fire an interrupt every 1 millisecond. The register writes in [sys_tick_init()](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/sys_tick.c#L19-L35) are:

```c
TCCR0A = (1u << WGM01);                  /* CTC mode: WGM01=1, WGM00=0 */
TCCR0B = (1u << CS01) | (1u << CS00);    /* prescaler /64               */
OCR0A  = 249u;                           /* compare-match value         */
TIMSK0 = (1u << OCIE0A);                 /* enable compare-match A IRQ  */
```

The reasoning chain: with F_CPU = 16 MHz and a /64 prescaler, the timer's input clock is 16,000,000 / 64 = 250,000 Hz. In CTC mode the counter counts from 0 up to `OCR0A` and then resets, so the overflow period is `(OCR0A + 1)` ticks. Setting OCR0A = 249 gives 250,000 / 250 = 1,000 Hz — exactly one interrupt per millisecond. The `+1` is because the counter counts the zero: it visits values 0, 1, 2, … 249, which is 250 distinct states. Forgetting the `−1` when computing OCR0A is one of the most common AVR timer bugs; the period would be 251 µs × 1000 = 251 ms per second instead of 250, and your millisecond clock would drift by 0.4%.

The ISR itself is minimal — `s_ms++` — keeping interrupt latency low. The counter `s_ms` is declared `static volatile uint32_t`. The `volatile` qualifier is essential: without it, the compiler is entitled to assume that `s_ms` never changes between two reads in the main loop (because no visible call modifies it), and may cache a stale value in a register, causing `sys_millis()` to return a constant. The `static` means it has file scope only; external code accesses it through `sys_millis()`, which wraps the read in `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`. This atomic wrapper is necessary because on an 8-bit CPU a 32-bit read is four separate load instructions; if the Timer0 ISR fires between loading the second and third byte, the caller gets a torn value — for example, the counter might be transitioning from `0x0000FFFF` to `0x00010000`, and a torn read could return `0x0001FFFF` or `0x000000FF`. `ATOMIC_BLOCK` emits `cli` / `sei` around the read, and `ATOMIC_RESTORESTATE` saves and restores the SREG so the function works correctly whether called with interrupts enabled or disabled.

The helper [sys_elapsed()](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/sys_tick.c#L56-L63) deserves attention. Rather than stamping `*anchor = sys_millis()` on each trigger (which would accumulate jitter), it does `*anchor += period_ms`. This anchored-advance pattern means the interval is measured from when it *should* have fired, not when the main loop got around to checking — so over many cycles, the tick rate stays accurate even if individual polls are late by a few milliseconds.

> **Explore this yourself.** Before looking at the code, work out: if you changed the Timer0 prescaler from /64 to /256, what value of OCR0A would you need for a 1 ms period? Is that value representable in an 8-bit register (0–255)? What happens if the required OCR0A value exceeds 255?

| Function | Description | Peripheral / Registers | Interrupt Context? |
|---|---|---|---|
| `void sys_tick_init(void)` | Configures Timer0 CTC mode, /64 prescaler, OCR0A=249, enables OCIE0A | Timer0: TCCR0A, TCCR0B, OCR0A, TIMSK0 | No |
| `ISR(TIMER0_COMPA_vect)` | Increments `s_ms` counter by 1 each millisecond | None (reads/writes SRAM only) | **Yes** |
| `uint32_t sys_millis(void)` | Returns current ms count with atomic 4-byte read | None (SREG for cli/sei) | No |
| `bool sys_elapsed(uint32_t*, uint32_t)` | Non-blocking interval check; advances anchor on trigger | None (calls sys_millis) | No |

---

### Timer2 — The Buzzer Tone Generator

[buzzer.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/buzzer.c) uses Timer2 in a completely different mode from Timer0. Where Timer0 fires an ISR, Timer2 never interrupts the CPU at all — instead it uses CTC mode with hardware output-compare toggle on `OC2A` (PB3). Setting `COM2A0 = 1` with `COM2A1 = 0` tells the timer's waveform generator to flip the OC2A pin every time the counter matches OCR2A. Each full cycle of the output waveform is therefore two match periods, so the output frequency is:

```
f_out = F_CPU / (2 × prescaler × (OCR2A + 1))
```

The [buzzer_tone_nb()](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/buzzer.c#L44-L82) function solves this equation in reverse: given a desired `freq_hz`, it iterates through all seven of Timer2's available prescaler values (1, 8, 32, 64, 128, 256, 1024 — note that Timer2 has a different prescaler table from Timer0/Timer1) and picks the smallest prescaler for which `OCR2A = F_CPU / (2 × prescaler × freq) − 1` fits in a `uint8_t` (0–255). Choosing the smallest prescaler maximises frequency resolution because each OCR2A step represents a smaller change in the output period.

The function then writes `TCCR2B = 0` to stop the timer before reconfiguring (avoiding glitches from a partially-updated configuration), sets `TCCR2A` with CTC mode + OC2A toggle, loads `OCR2A`, and finally writes the prescaler bits into `TCCR2B` — this last write starts the timer. The stop-before-reconfigure pattern is important because the ATmega328P's timer update logic is not atomic across multiple registers; if you changed OCR2A while the timer was running with the old prescaler, you could get one cycle at a wildly wrong frequency.

The "non-blocking" aspect is that `buzzer_tone_nb()` records a stop-time (`s_stop_at = sys_millis() + duration_ms`) and returns immediately. The main loop calls `buzzer_service()` every iteration, which checks whether the deadline has passed and calls `buzzer_off()` to stop the timer and tristate PB3. Tristating (setting DDR to input with no pull-up) is a deliberate choice for a passive piezo: it ensures no DC bias is applied to the element when idle, preventing steady-state current draw and mechanical stress.

There is a subtlety here worth flagging: `buzzer_tone_nb()` is called from inside the PCINT1 ISR (in [buttons.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/buttons.c#L74)), and it calls `sys_millis()` internally (line 80). `sys_millis()` uses `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`, which saves SREG, executes `cli`, restores SREG. Inside the ISR, the I flag is already cleared, so `cli` is a no-op and the SREG restore puts it back to cleared — this is safe. But note that `s_stop_at` is read by `buzzer_service()` in the main loop and written by `buzzer_tone_nb()` from the ISR. The variable is `volatile`, which prevents the compiler from caching it, but `s_stop_at` is a `uint32_t` — a 4-byte value on an 8-bit CPU. A write from the ISR that interrupts a 4-byte read in `buzzer_service()` could produce a torn value. In practice, the only consequence would be stopping the tone one poll iteration too early or too late (a few hundred microseconds), so the race is benign for audio — but it is technically a data race and worth being aware of.

> **Explore this yourself.** For the jump sound effect (`JUMP_PITCH = 2700` Hz), work through the prescaler-selection loop by hand. At prescaler = 1, what is OCR2A? Does it fit in 0–255? What about prescaler = 8? Find the first prescaler that works and check it against what the code would compute. Then calculate the actual output frequency for that OCR2A value — how far off is it from the requested 2700 Hz?

| Function | Description | Peripheral / Registers | Interrupt Context? |
|---|---|---|---|
| `void buzzer_tone_nb(uint16_t freq_hz, uint16_t duration_ms)` | Configures Timer2 CTC + OC2A toggle for a given frequency; sets PB3 as output; records stop time | Timer2: TCCR2A, TCCR2B, OCR2A; DDRB | Called from ISR (PCINT1) and main |
| `void buzzer_service(void)` | Polls: if tone duration expired, calls buzzer_off() | None (calls sys_millis) | No |
| `void buzzer_off(void)` | Stops Timer2, disconnects OC2A, tristates PB3 | Timer2: TCCR2A, TCCR2B; DDRB | No |

---

### ADC — One-Shot Random Seed

[rng.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/rng.c) uses the ADC exactly once, at boot, to seed a pseudo-random number generator. The [rng_init()](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/rng.c#L23-L53) function sets `ADMUX` to select ADC channel 2 (PC2) with AVCC as the voltage reference (`REFS0 = 1, REFS1 = 0`). The AVCC reference is the right choice here because it tracks the supply rail and the 100 nF capacitor on AREF (shown in the [hardware documentation](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/docs/hardware_connections.md)) provides the decoupling the datasheet requires.

The prescaler is set to /128 (`ADPS2:0 = 111`), giving an ADC clock of 16 MHz / 128 = 125 kHz. The datasheet specifies 50–200 kHz for full 10-bit accuracy; 125 kHz sits comfortably in range. A single conversion takes 13 ADC clock cycles (25 for the first conversion after enabling), so the busy-wait `while (ADCSRA & (1u << ADSC))` blocks for at most 25 / 125 kHz = 200 µs — trivial during boot.

After reading the 10-bit result from the `ADC` register, the code disables the ADC entirely (`ADCSRA = 0`) to save the ~350 µA quiescent current draw. This is good practice: the ADC's successive-approximation circuit consumes power even when idle if `ADEN` remains set.

The seed is fed into a 16-bit xorshift PRNG with shift constants 7, 9, 8. These constants are from George Marsaglia's 2003 paper; the period is 2^16 − 1 = 65,535, which is more than enough randomness for obstacle respawn positions. A guard ensures the state is never zero (the absorbing state of xorshift).

One point worth noting: the quality of the seed depends entirely on the noise present on the floating PC2 pin. In a noisy breadboard environment with a running MCU toggling GPIO nearby, you'll get reasonable entropy in the low bits. On a clean PCB with good ground planes, a floating pin might settle to a nearly repeatable voltage determined by leakage currents, and you'd get the same seed every time. A more robust approach would be to XOR multiple ADC readings, or to incorporate the Timer0 counter value at the moment of the read.

> **Explore this yourself.** The `rng_range()` function uses the modulo operator (`%`) to map a 16-bit random value into a range. This introduces modulo bias when the range does not evenly divide 65,535. For the hill respawn (`rng_range(0, 8)`), how severe is the bias? What about for the crow (`rng_range(0, 16)`)? For a game like this, does it matter?

| Function | Description | Peripheral / Registers | Interrupt Context? |
|---|---|---|---|
| `void rng_init(void)` | Performs one ADC conversion on floating PC2, seeds xorshift state, disables ADC | ADC: ADMUX, ADCSRA, ADC (result) | No |
| `uint16_t rng_u16(void)` | Returns next 16-bit xorshift pseudo-random value | None (pure computation) | No |
| `uint16_t rng_range(uint16_t lo, uint16_t hi)` | Maps rng_u16() into [lo, hi) via modulo | None (calls rng_u16) | No |

---

### Pin-Change Interrupts — Button Debouncing

[buttons.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/buttons.c) is where the interrupt architecture gets interesting. The ATmega328P's pin-change interrupt system groups pins into three vectors: PCINT0 (PORTB), PCINT1 (PORTC), PCINT2 (PORTD). Both buttons are on PORTC, so they share `PCINT1_vect`. Critically, a pin-change interrupt fires on *any* edge — rising or falling — and it fires once for the *group*, not per pin. The ISR must read the current pin states, XOR them against the previous snapshot, and figure out which pin(s) changed and in which direction.

The [buttons_init()](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/buttons.c#L36-L56) function:

1. Clears the DDR bits for PC0 and PC1, making them inputs.
2. Sets the PORT bits while DDR is zero, which enables the internal pull-ups. This is one of the ATmega328P's GPIO modes that trips people up: writing 1 to PORTxn when DDRxn is 0 doesn't drive the pin high — it activates a ~20–50 kΩ pull-up resistor to VCC. The buttons connect the pins to ground when pressed, so the resting state is high (logic 1) and a press reads as low (logic 0) — this is the "active-low" convention.
3. Snapshots the initial pin state into `s_pinc_prev` so the first ISR invocation has a valid baseline.
4. Enables the PCINT1 group by setting `PCIE1` in `PCICR`, and unmasks the specific pins PC0 and PC1 by setting bits 0 and 1 in `PCMSK1`.

The [ISR(PCINT1_vect)](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/buttons.c#L58-L92) reads the current PINC value, XORs it against the snapshot to find changed bits, and then handles each button independently. Debouncing is done with a simple time-window approach: if fewer than `DEBOUNCE_MS` (5 ms) have passed since the last transition on a given pin, the edge is ignored. This is a reasonable debounce strategy for tactile switches, which typically bounce for 1–5 ms. The timestamp comparison uses `sys_millis()`, which (as discussed above) is safe to call from ISR context because `ATOMIC_RESTORESTATE` is a no-op when interrupts are already disabled.

For the jump button, a falling edge (press) sets the flag `g_jump_edge = true` and immediately calls `buzzer_tone_nb(JUMP_PITCH, JUMP_MS)`. The flag is a one-shot: main-loop code calls `buttons_consume_jump_edge()` to atomically read and clear it. The "consume" pattern prevents a single press from being seen as multiple jumps. The consume function uses inline assembly `cli` with a `"memory"` clobber rather than avr-libc's `ATOMIC_BLOCK` — the effect is identical, but marginally smaller code.

For the duck button, there is no edge flag — instead `g_ducking` tracks the current *level*. A press sets it true, a release sets it false. This makes sense: ducking is a hold-to-sustain action, not a one-shot trigger.

A design observation: firing the buzzer SFX directly from the ISR means the player hears a beep the instant they press the button, regardless of whether the game logic has run yet. This gives a more responsive feel than waiting for the next game tick (which could be up to 90 ms away). The trade-off is that you hear the jump sound even if the game-state machine discards the jump (e.g. the player is already airborne). For a casual game, the immediacy is worth the occasional phantom beep.

The debounce timestamps `s_last_jump_ms` and `s_last_duck_ms` are `volatile uint32_t`, written only from the ISR and never read from main context, so there is no torn-read risk on them. However, `g_jump_edge` and `g_ducking` are `volatile bool` (single-byte), which on AVR is atomically readable without special protection — the comment in `buttons_ducking()` correctly notes this.

> **Explore this yourself.** Open [buttons.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/buttons.c) and trace what happens if both buttons are pressed at exactly the same time (i.e. both PC0 and PC1 change in the same PINC sample). Does the ISR handle both, or only the first? What if one button is released while the other is pressed simultaneously?

| Function | Description | Peripheral / Registers | Interrupt Context? |
|---|---|---|---|
| `void buttons_init(void)` | Configures PC0/PC1 as inputs with pull-ups; enables PCINT1 group and unmasks PCINT8/PCINT9 | GPIO: DDRC, PORTC, PINC; PCICR, PCMSK1 | No |
| `ISR(PCINT1_vect)` | Debounces both buttons, sets g_jump_edge / g_ducking, fires buzzer SFX on press | GPIO: PINC; calls buzzer_tone_nb, sys_millis | **Yes** |
| `bool buttons_consume_jump_edge(void)` | Atomically reads and clears the jump one-shot flag | SREG (inline cli/sei) | No |
| `bool buttons_ducking(void)` | Returns current debounced duck level (single-byte, no atomic needed) | None | No |

---

### EEPROM — High Score Persistence

[high_score.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/high_score.c) uses the ATmega328P's 1024-byte internal EEPROM to store the high score across power cycles. The EEPROM layout is explicit:

- Address 0x00–0x01: `uint16_t` high score (little-endian, as AVR is little-endian).
- Address 0x02: `uint8_t` magic byte (`0xA5`).

The magic byte solves a real problem: a freshly-erased EEPROM cell reads as `0xFF`. Without a sentinel, there's no way to tell whether the stored high score is a legitimate value of 65,535 or just uninitialised memory. On first boot, [high_score_load()](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/high_score.c#L32-L46) checks the magic byte, finds it's not `0xA5`, stamps it, writes a score of 0, and returns 0. On subsequent boots, the magic byte validates and the stored score is returned.

The code exclusively uses `eeprom_update_*` rather than `eeprom_write_*`. The `update` variants read the cell first and only perform the erase-write cycle if the new value differs. This is significant because EEPROM cells have a rated endurance of ~100,000 erase-write cycles. In a game where the player dies every 30 seconds and the high score doesn't change most of the time, the `update` optimisation prevents burning a write cycle on every death.

The `EEMEM` attribute on the variable declarations tells the linker to place them in the `.eeprom` section. The Makefile has a rule to extract this section into a `.eeprom` hex file and a target (`flash_eeprom`) to program it — but for normal operation, the code initialises EEPROM at runtime via the magic-byte check, so pre-programming the EEPROM is not necessary.

Note that `high_score_save()` does its own comparison (`score > stored`) before calling `eeprom_update_word`. This is redundant with `eeprom_update_word`'s own read-before-write behaviour, but it has a different purpose: it enforces the game rule that only a new record is written, whereas `eeprom_update_word` would happily write a *lower* score if asked to.

| Function | Description | Peripheral / Registers | Interrupt Context? |
|---|---|---|---|
| `void high_score_load(void)` → `uint16_t` | Reads magic byte; initialises EEPROM on first boot; returns stored high score | EEPROM (via avr-libc eeprom_read/update) | No |
| `void high_score_save(uint16_t score)` | Writes score to EEPROM only if it beats the current stored value | EEPROM (via avr-libc eeprom_read/update) | No |

---

### LCD Driver — 4-Bit Parallel HD44780 Interface

The external [lcd_driver.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/lcd_driver/lcd_driver.c) / [lcd_driver.h](file:///c:/Users/User/Coding/my_avr_programming/lcd/lcd_driver/lcd_driver.h) is a full HAL for a 16×2 HD44780-compatible display in 4-bit mode. Every byte transfer requires two nibble writes (high nibble first, then low nibble), each latched by toggling the Enable pin. The driver uses **busy-flag polling** after initialisation rather than fixed delays — this is the right approach because it adapts to the controller's actual execution speed rather than assuming worst-case timing, which both reduces wasted CPU time and is more reliable across different HD44780 clones.

The [Lcd_Init()](file:///c:/Users/User/Coding/my_avr_programming/lcd/lcd_driver/lcd_driver.c#L145-L230) sequence follows the HD44780's mandatory power-on procedure precisely: three 0x3 nibbles to force the controller into a known 8-bit state (regardless of what mode it was in from a previous warm boot), then a 0x2 nibble to switch to 4-bit mode, followed by the standard Function Set / Display Off / Clear / Entry Mode / Display On command sequence. The delays between the early nibbles (5 ms, 200 µs, 200 µs) come directly from the datasheet and are necessary because the busy flag cannot be read until 4-bit mode is established.

The custom-character facility is used by [sprites.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/sprites.c) to load 7 game glyphs (hill, two walking frames, jump pose, duck pose, two crow animation frames) into CGRAM slots 0–6. The HD44780 has 8 custom-character slots, each 5 pixels wide by 8 pixels tall. Writing character codes 0–7 to DDRAM causes the display to render the corresponding CGRAM glyph instead of a ROM character.

> **Explore this yourself.** The [sprites.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/sprites.c) glyph data uses the `PROGMEM` attribute. This means the byte arrays live in flash (program memory) rather than SRAM. On the ATmega328P you have 32 KB of flash but only 2 KB of SRAM. Each glyph is 8 bytes, so 7 glyphs = 56 bytes — not huge, but the principle matters. Look at `sprites_load_all()` and find where `pgm_read_byte()` is called. Why can't the code just write `buf[row] = k_glyph_table[slot][row]` directly? What would happen if you removed `PROGMEM` and `pgm_read_byte()` — would the code still work? (Hint: think about the Harvard architecture and which address space a pointer dereferences by default.)

| Function | Description | Peripheral / Registers | Interrupt Context? |
|---|---|---|---|
| `void sprites_load_all(void)` | Copies 7 glyph patterns from flash into LCD CGRAM slots 0–6; resets cursor to (0,0) | LCD CGRAM via Lcd_WriteCustomChar / Lcd_SetCursor | No |

---

## Initialisation, Main Loop, and ISR Responsibilities

### The Boot Sequence

[main.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/main.c) calls initialisations in a deliberate order. First, `sys_tick_init()` configures Timer0 and enables its compare-match interrupt in `TIMSK0` — but this does not start the ISR firing yet, because global interrupts are still disabled (the I flag in SREG defaults to 0 after reset). Next, a 50 ms `_delay_ms()` gives the LCD controller time to complete its internal reset (the HD44780 datasheet mandates ≥40 ms after VCC reaches 2.7 V). Then `Lcd_Init()` runs the full power-on sequence. Then `sprites_load_all()` writes custom characters into CGRAM. Then `rng_init()` runs a single blocking ADC conversion. Then the buzzer pin is explicitly tristated (redundant with reset defaults, but defensive). Then `buttons_init()` configures the pin-change interrupt — again, the interrupt is armed in the peripheral but won't fire until the I flag is set.

Finally, `high_score_load()` reads the EEPROM, and then `sei()` enables global interrupts. From this point forward, both `TIMER0_COMPA_vect` and `PCINT1_vect` can fire. The ordering matters: all peripherals are fully configured before any ISR can run, avoiding the scenario where an ISR accesses a half-initialised peripheral.

### The Super-Loop State Machine

The main `for (;;)` loop implements a three-state machine:

**STATE_SPLASH**: The LCD shows the game title and the stored high score. The loop polls `buttons_consume_jump_edge()` every iteration. When the player presses jump, the game transitions to `STATE_PLAYING`, the screen is cleared, `game_init()` resets all game-state fields, and `tick_anchor` is stamped with the current millisecond count.

**STATE_PLAYING**: On every loop iteration, `sys_elapsed(&tick_anchor, TICKSPEED_MS)` checks whether 90 ms have passed since the last game tick. If not, the loop falls through (doing nothing but servicing the buzzer) — this is the non-blocking cooperative scheduling pattern. When 90 ms have elapsed, `game_tick_update()` runs one frame of game logic: it updates the player's vertical position based on the jump phase, draws the player sprite, advances the hill and crow obstacles leftward, checks for collisions, and returns `true` if the player is hit. On collision, the state transitions to `STATE_GAMEOVER` via `game_end()`, which fires the death sound and conditionally updates the EEPROM high score.

**STATE_GAMEOVER**: The score and high score are displayed on row 0, and "Jump to Continue" blinks on row 1 with a 500 ms period. Another jump press restarts the game by transitioning back to `STATE_PLAYING`, reloading sprites (CGRAM can be corrupted by certain LCD timing glitches, so the reload is defensive), and reinitialising the game state.

The crucial point about `buzzer_service()` being called unconditionally at the top of every loop iteration: this is what makes the buzzer truly non-blocking. The ISR starts the tone, and the main loop stops it when the duration expires. Without this poll, tones would play forever.

> **Explore this yourself.** The game-tick period `TICKSPEED_MS = 90` directly controls game speed. Open [game.h](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/game.h) and find `JUMP_LENGTH = 3`. The player is airborne for 3 ticks × 90 ms = 270 ms. The hill scrolls left one column per tick, so it crosses the 16-column screen in 16 × 90 ms = 1.44 seconds. At the player's column (0), the hill arrives while the player has been in the air for at most 270 ms — is that enough to clear the hill? Think about where the hill actually is when jump is first detected.

---

## The Game Logic

[game.c](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/game.c) is the heart of the project, and it is entirely pure logic with no direct hardware register access — all display output goes through the LCD driver API, and all input comes from the buttons module. The game state is encapsulated in a [game_t](file:///c:/Users/User/Coding/my_avr_programming/lcd/digital_dash/src/game.h#L29-L37) struct:

```c
typedef struct {
    uint8_t  jump_phase;   /* 0..(JUMP_LENGTH-1) = airborne; >JUMP_LENGTH = grounded */
    uint16_t game_tick;    /* total ticks since game start                            */
    int8_t   crow_x;       /* column position (-128..127, signed for off-screen left) */
    int8_t   hill_x;       /* column position                                         */
    bool     player_y;     /* false = ground (row 1), true = air (row 0)              */
    bool     crow_go;      /* flip-flop: crow moves only every other tick              */
    uint16_t score;        /* obstacles dodged                                         */
} game_t;
```

The `int8_t` type for `crow_x` and `hill_x` is a deliberate choice: obstacles start off-screen right (column 25 or 40), scroll left into the visible range (0–15), and eventually reach column 0 and below. A signed type lets the position go negative, which is how the code detects that an obstacle has reached the player column. An unsigned type would wrap around to 255, which wouldn't work as a "past the player" test.

One curiosity: `HILL_INIT_X = 25` and `CROW_INIT_X = 40` are both larger than 15, which means they start beyond the right edge of the 16-column display. The `if (g->hill_x < 16)` guard prevents writing to an out-of-bounds column on the LCD. The hill is drawn only when it enters the visible portion of the screen.

The crow has a half-speed mechanism: `crow_go` toggles every tick, and the crow's x-position is only decremented when `crow_go` is true. This means the crow moves one column every two ticks (180 ms), while the hill moves one column every tick (90 ms). The hill is a ground obstacle you jump over; the crow is an aerial obstacle you duck under. The different speeds give the player different timing windows.

The collision detection in `draw_player()` and the subsequent position checks is positional: if `hill_x < 1` and the player is not jumping (`!player_y`), it's a hit. If `crow_x < 1` and the player is not ducking (`!buttons_ducking()`), it's a hit. The check at `< 1` (not `== 0`) ensures that obstacles that skip position 0 (which can't happen at one column per tick, but is defensive) are still caught.

The rendering approach is brute-force but effective for a 16×2 display: every tick, the player is drawn at column 0, then the entire rest of both rows (columns 1–15) is blanked with spaces, then the obstacles are drawn at their current positions. This avoids tracking which column an obstacle was at last frame and selectively erasing it — a worthwhile simplification when the display is only 32 characters total.

| Function | Description | Peripheral / Registers | Interrupt Context? |
|---|---|---|---|
| `static void draw_player(const game_t *g)` | Draws player sprite at col 0 based on state (walking/jumping/ducking); blanks cols 1–15 | LCD (via Lcd_SetCursor, Lcd_WriteChar) | No |
| `void game_init(game_t *g)` | Resets all game state fields to starting values | None (pure SRAM writes) | No |
| `bool game_tick_update(game_t *g)` | One frame of game logic: move player, obstacles, check collisions, draw | LCD (via Lcd_SetCursor, Lcd_WriteChar); calls buttons_*, rng_range | No |
| `void game_end(game_t *g, uint16_t hi, uint16_t *new_hi_out)` | Fires death buzzer SFX; conditionally saves new high score to EEPROM | Buzzer (via buzzer_tone_nb); EEPROM (via high_score_save) | No |

---

## Things Worth Questioning

**The `s_stop_at` / `s_active` race in buzzer.c.** As noted above, these are `volatile uint32_t` / `volatile uint8_t` variables written from ISR context (via `buzzer_tone_nb` called from `PCINT1_vect`) and read from main context (in `buzzer_service`). For `s_active` (a single byte), the read is atomic on AVR. But `s_stop_at` is 4 bytes. If the ISR fires between two bytes of a `sys_millis() >= s_stop_at` comparison in `buzzer_service()`, the comparison could see a partially-updated deadline. The practical impact is negligible (a tone might be cut short or extended by one poll cycle), but for code that controls something safety-critical, this would need an `ATOMIC_BLOCK` around the read of `s_stop_at`.

**EEPROM writes with interrupts enabled.** The `eeprom_update_word` function internally busy-waits for the EEPROM write to complete (~3.4 ms per byte). During this time, interrupts are still enabled, so the Timer0 ISR and PCINT1 ISR continue to fire. This is fine — avr-libc's EEPROM routines are interrupt-safe — but it means a button press during EEPROM write will be handled, and the resulting `buzzer_tone_nb` will reconfigure Timer2 mid-game-over. That's intentional (you want the button beep to be responsive), but worth knowing.

**`itoa` with a signed cast.** In `lcd_write_u16()`, the call is `itoa((int)val, buf, 10)`. On AVR, `int` is 16-bit signed, so values above 32,767 would print as negative numbers. Since game scores are `uint16_t`, a score above 32,767 would display incorrectly. In practice, no one is dodging 32,768 obstacles in a sitting, but `utoa()` would be the pedantically correct choice.

---

## Concepts Most Worth Understanding Deeply

### 1. Volatile Semantics and Atomic Access on an 8-Bit CPU

This project is an excellent case study because it uses `volatile` correctly throughout, and it demonstrates *why* you need atomic access for multi-byte shared variables but *not* for single-byte ones on AVR. The contrast between `sys_millis()` (which needs `ATOMIC_BLOCK` for its 4-byte read) and `buttons_ducking()` (which reads a single `volatile bool` without protection) makes the concept concrete. Understanding when a torn read can happen — and when it can't — is foundational for any bare-metal work.

**Suggested modification:** Remove the `ATOMIC_BLOCK` from `sys_millis()` and observe the effect. Add a diagnostic: read `sys_millis()` twice in quick succession in the main loop and check if the second reading is ever *less than* the first. On a real device, you should eventually catch a torn read where the millisecond counter appears to jump backward.

### 2. Timer Modes and Hardware-Generated Waveforms

The project uses the same CTC mode for two completely different purposes: Timer0 generates a periodic interrupt (software observes the event), while Timer2 generates a physical waveform on a pin (hardware does all the work after configuration). Understanding the difference between "timer as interrupt source" and "timer as waveform generator" is the key to unlocking PWM, frequency generation, input capture, and motor control in future projects.

**Suggested modification:** Add a visible speed-up to the game: every 10 points scored, decrease `TICKSPEED_MS` by 5 ms (clamping at a minimum, say 40 ms). This forces you to understand how `sys_elapsed()` works, how the tick period drives game feel, and how the jump timing (`JUMP_LENGTH × TICKSPEED_MS`) must remain playable as the tick rate changes. You might need to adjust `JUMP_LENGTH` dynamically to keep jumps viable.

### 3. Non-Blocking Cooperative Scheduling

The entire project avoids `_delay_ms()` in the game loop. All timing is done by polling `sys_elapsed()` against a millisecond counter. This is the cooperative-scheduling pattern that scales from simple games up to industrial RTOS-free firmware. The critical insight is that the main loop must *never block* — if any state in the state machine calls a blocking function, every other state starves (buttons stop responding, the buzzer never shuts off, LCD updates freeze). Once you internalise this pattern, you can extend the project with arbitrary new features (LED animations, serial telemetry, additional sound effects) without any of them interfering with each other.

**Suggested modification:** Add a "countdown" sequence before gameplay starts: after the jump press on the splash screen, display "3", "2", "1", "GO!" with 500 ms between each, all without using `_delay_ms()`. You must implement this as additional states in the state machine, driven by `sys_elapsed()`. If you find yourself reaching for `_delay_ms()`, you haven't internalised the pattern yet.
