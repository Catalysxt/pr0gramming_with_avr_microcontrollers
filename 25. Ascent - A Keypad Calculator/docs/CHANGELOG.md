# Changelog

## [2026-07-02] - Session: initial ASCENT build-out

### Added

- `src/hardware.h` — pin macros for buzzer, keypad, and the unused-in-v1
  PB4 status LED.
- `src/sys_tick.h/.c` — ported from `anvil/src/sys_tick.*` unmodified
  (Timer0 1 kHz CTC tick).
- `src/keypad.h/.c` — ported from `anvil/src/keypad.*`; buzzer dependency
  removed (see Fixed below); added `keypad_rows_sleep()`/
  `keypad_rows_wake()` for `sleep.c`.
- `src/buzzer.h/.c` — Timer2 CTC tone engine ported from
  `anvil/src/buzzer.*`; new `buzzer_init()`; new 5-class SFX table
  (`SFX_DIGIT`, `SFX_OPERATOR`, `SFX_EQUALS`, `SFX_CLEAR`, `SFX_ERROR`)
  replacing Anvil's PIN-safe set.
- `src/calc.h/.c` — new. int32 overflow-safe chained-arithmetic core,
  `operand_buf_t` digit accumulator, `calc_format()`.
- `src/ui.h/.c` — new. Splash/expression-and-operand/expression-and-result
  /error/blank LCD rendering, with row-0 scrolling and a reserved-column
  cursor.
- `src/sleep.h/.c` — new. `SLEEP_MODE_PWR_DOWN` + PCINT8-11 wake, fully
  state-preserving.
- `src/ascent.h/.c` — new. Top-level FSM (`ST_SPLASH`/`ST_FRESH`/
  `ST_OPERAND`/`ST_OPERATOR`/`ST_RESULT`/`ST_ERROR`).
- `src/main.c` — new. Init sequence + non-blocking superloop.
- `README.md`, `docs/hardware_connections.md`, `docs/Theory.md`.

### Fixed — spec corrections / gaps filled

`docs/prompt.md` is otherwise treated as authoritative; these are the
points where implementing it literally was impossible, self-contradictory,
or unsafe, and what was done instead:

- **Idle-sleep timer ownership**: `docs/prompt.md`'s `g_last_key_ms` is
  listed as private `ascent.c` state but main()'s pseudocode references
  it directly, which main() can't do (it never sees consumed key events
  or `g_state`). Kept it private to `ascent.c`; added
  `ascent_check_idle_sleep()`/`ascent_redraw()` accessors instead.
- **Multiply-overflow UB**: the spec's suggested `abs(a) > INT32_MAX /
  abs(b)` check is undefined behaviour whenever `a` or `b` is
  `INT32_MIN` outside the `* -1` case. Fixed via an `abs_u32()` helper
  that converts to `uint32_t` before negating (see `docs/Theory.md` §4).
- **`operand_to_int` 10-digit overflow**: a full 10-digit entry (e.g.
  `9999999999`) doesn't fit `int32_t` *or* `uint32_t`. Accumulates in
  `uint64_t` and saturates to `INT32_MAX`, which is safe because the
  saturated value immediately trips the ordinary overflow path on any
  subsequent operation.
- **`calc_format` and `INT32_MIN`**: same UB family as the multiply fix;
  reuses `abs_u32()`.
- **Cursor column math**: "right-aligned in columns 0-15" plus "cursor
  one position past the last digit" is geometrically impossible (off-
  screen). Resolved by permanently reserving column 15 for the cursor
  and right-aligning digits against column 14.
- **`ui_show_expr_and_result` cursor mode**: the spec text says "solid
  cursor for ST_RESULT," contradicting two other spec locations that
  both say ST_RESULT has no cursor. Trusted the 2-vs-1 majority: added a
  `show_cursor` parameter — `true` for `ST_OPERATOR` (solid, "armed"),
  `false` for `ST_RESULT` (none).
- **`ST_SLEEP_PEND`**: kept in the `state_t` enum for spec-shape parity
  but never entered — sleep is state-preserving via the accessors above,
  not a real FSM state.
- **Operator-replace worked example typo**: the deliverables checklist's
  `5 + − 3 = -2` doesn't check out mathematically — replacing `+` with
  `−` before typing `3` gives `5 − 3 = 2`, not `-2` (v1 has no
  signed-operand entry; that's an explicit stretch goal). Implemented
  the mathematically correct `2`; the checklist and README worked
  example both use `2`.
- **"Press \* to reset" text**: kept verbatim even though `*` is actually
  ignored during the 1.5 s error freeze (it auto-resets regardless, so
  the text is only ever momentarily inaccurate).

### Files touched

`src/hardware.h`, `src/sys_tick.h`, `src/sys_tick.c`, `src/keypad.h`,
`src/keypad.c`, `src/buzzer.h`, `src/buzzer.c`, `src/calc.h`,
`src/calc.c`, `src/ui.h`, `src/ui.c`, `src/sleep.h`, `src/sleep.c`,
`src/ascent.h`, `src/ascent.c`, `src/main.c`, `README.md`,
`docs/hardware_connections.md`, `docs/Theory.md`, `docs/CHANGELOG.md`.

### Why

`docs/prompt.md` specified the full ASCENT firmware from scratch;
`src/` and most of `docs/` were empty before this session (only
`prompt.md` and `CLAUDE.md` existed). Built directly against the spec,
reusing the sibling `anvil/` project's proven `sys_tick`/`keypad`/
`buzzer` architecture wherever the spec didn't diverge.
