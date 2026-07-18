# Host-side unit tests -- native gcc, no AVR toolchain.
#
# Only the pure application logic is under test: mood.c (valence/arousal model),
# sensors.c (sensor -> nudge interpretation) and fsm.c (transition table). These
# files include no AVR headers. Hardware modules (spi, max7219, dotmatrix,
# buttons, adc, hcsr04, animator, timers/ISRs) are NOT built here -- see README.
#
# Usage:  make            # from tests/, builds + runs both suites
#         make -C tests   # from the project root (what `make host-test` does)

CC       = gcc
APPDIR   = ../src/app
CPPFLAGS = -I$(APPDIR)
CFLAGS   = -std=gnu99 -Wall -Wextra -Werror -g

TESTS = test_mood test_fsm

.PHONY: all run clean
all: run

test_mood: test_mood.c $(APPDIR)/mood.c $(APPDIR)/sensors.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@

test_fsm: test_fsm.c $(APPDIR)/mood.c $(APPDIR)/fsm.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@

run: $(TESTS)
	@echo "== running host tests =="
	./test_mood
	./test_fsm

clean:
	rm -f $(TESTS) *.o
