# Host-side tests

These tests compile with **native `gcc`** (no `avr-gcc`, no hardware) and run on
your development machine. They exercise the parts of the firmware that are pure
logic, so a regression in the emotion model is caught in milliseconds instead of
on a breadboard.

## Run

```sh
make host-test        # from the project root
# or
make -C tests         # equivalently
```

`make host-test` returns non-zero if any check fails, so it drops straight into
CI.

## Under test

| Suite         | Module(s)              | What it checks                                            |
| :------------ | :--------------------- | :-------------------------------------------------------- |
| `test_mood`   | `mood.c`, `sensors.c`  | (v, a) trajectory over scripted pets / proximity events matches a golden table; pet-gesture bonus; linear decay; out-of-range trend reset. |
| `test_fsm`    | `mood.c`, `fsm.c`      | region selection + ±8 hysteresis band; the illegal `HAPPY -> NERVOUS` jump routes through `HESITANT`; `SLEEPY -> SURPRISED` is abrupt. |

## Explicitly NOT under test (stubbed / excluded)

SPI, the MAX7221 register writes, `dotmatrix`, `buttons`, `adc`, `hcsr04`, the
`animator`'s PROGMEM/pgmspace access, and every timer/ISR are **not** built here
— they depend on `<avr/io.h>` and real silicon timing. The application logic was
deliberately split so `mood.c`, `sensors.c` and `fsm.c` pull in no AVR headers
and can be linked straight into a host binary. Behaviour of the excluded modules
is verified instead in the Wokwi simulation (see the top-level `README.md`).
