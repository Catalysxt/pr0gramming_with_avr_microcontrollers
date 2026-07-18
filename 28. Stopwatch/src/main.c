/* Two-button stopwatch on eight 7-segment digits driven by a single MAX7219.
 *
 * Display layout (7 of 8 digits; DIG7 is left blank):
 *
 *     DIG0 DIG1 . DIG2 DIG3 . DIG4 DIG5 DIG6
 *      \minutes/    \seconds/   \milliseconds/
 *         00     .    00     .      000
 *
 * The decimal points on DIG1 and DIG3 act as field separators, so the display
 * reads "00.00.000".
 *
 * Controls (active-low buttons, internal pull-ups -- see hardware_connections.h):
 *   PD6  START/PAUSE : toggles the running state
 *   PD7  CLEAR       : resets the count to zero at any time (running or paused)
 *
 * Time base: Timer1 in CTC mode raises a compare interrupt every 1 ms. That tick
 * drives two counters -- a free-running clock used to sample the buttons at a
 * fixed rate, and the stopwatch's elapsed time (advanced only while running).
 * The main loop never blocks, so no milliseconds are lost to display or debounce
 * work. */

#include <avr/io.h>
#include <avr/interrupt.h>   /* ISR(), sei() */
#include <util/atomic.h>     /* ATOMIC_BLOCK -- consistent multi-byte reads of ISR globals */
#include <stdbool.h>

#include "pin.h"                    /* pin_set_input(), pin_read() */
#include "max7219.h"               /* the LED-driver front end */
#include "font7seg.h"              /* char_to_segments() */
#include "hardware_connections.h"  /* MAX7219_CS, BTN_STARTPAUSE, BTN_CLEAR */

/* ---- Timer1 time base ---- */
#define TIMER1_PRESCALE 8u                                     /* CS11 -> clk/8 */
#define TICK_HZ         1000u                                  /* one tick per millisecond */
/* CTC top: with clk/8 at 16 MHz, (16e6/8/1000)-1 = 1999 counts -> exactly 1 ms.
 * Exact integer division, so the time base carries no rounding drift. */
#define TIMER1_TOP      ((F_CPU / TIMER1_PRESCALE / TICK_HZ) - 1u)

/* ---- Time decomposition ---- */
#define MS_PER_SEC   1000u
#define SEC_PER_MIN  60u
#define MIN_WRAP     100u   /* minutes field is two digits: wrap 99.59.999 -> 00.00.000 */
#define MS_ROLLOVER  ((uint32_t)MIN_WRAP * SEC_PER_MIN * MS_PER_SEC)   /* 6,000,000 ms */

/* ---- Button debounce ---- */
#define DEBOUNCE_MS  15u    /* a new level must hold this long (ms) to be accepted */

/* Time base, shared with the Timer1 ISR (all volatile: ISR writes, main reads). */
static volatile uint32_t s_ticks_ms   = 0;   /* free-running 1 ms clock (for button sampling) */
static volatile uint32_t s_elapsed_ms = 0;   /* stopwatch value; advances only while running */
static volatile bool     s_running    = false;

/* The stopwatch is a single MAX7219: a one-device daisy chain. The shared driver
 * models N cascaded chips, so we describe our lone chip as count = 1. */
static const max7219_chain_t s_disp = { MAX7219_CS, 1 };

/* One segment byte per physical digit; index 0 is the leftmost digit. */
static uint8_t s_frame[MAX7219_DIGITS];

/* Logical position (0 = leftmost) -> MAX7219 digit register index. Wire DIG0 as
 * the leftmost digit to keep this identity map; reverse the entries if a module
 * numbers its digits right-to-left. */
static const uint8_t s_digit_map[MAX7219_DIGITS] = { 0, 1, 2, 3, 4, 5, 6, 7 };

/* Timer1 CTC @ 1 ms. WGM12 selects CTC with OCR1A as top; CS11 selects clk/8. */
static void timer1_init(void) {
    TCCR1B = (1 << WGM12) | (1 << CS11);
    OCR1A  = TIMER1_TOP;
    TIMSK1 = (1 << OCIE1A);              /* enable compare-match A interrupt */
}

ISR(TIMER1_COMPA_vect) {
    s_ticks_ms++;                        /* free-running: keeps ticking even when paused */
    if (s_running) {
        if (++s_elapsed_ms >= MS_ROLLOVER) {
            s_elapsed_ms = 0;
        }
    }
}

/* Blit the framebuffer to the chip. */
static void push_frame(void) {
    for (uint8_t i = 0; i < MAX7219_DIGITS; i++) {
        max7219_chain_set_digit(s_disp, 0, s_digit_map[i], s_frame[i]);
    }
}

/* Decompose elapsed milliseconds into MM.SS.mmm and paint the framebuffer.
 * Digit glyphs come straight from the shared font -- char_to_segments('0'+d) --
 * and char_to_segments('.') supplies the bare decimal-point segment we OR in as
 * the minute/second field separators. Leading zeros are intentional. */
static void render_time(uint32_t ms) {
    uint16_t millis  = (uint16_t)(ms % MS_PER_SEC);
    uint32_t total_s = ms / MS_PER_SEC;
    uint8_t  sec     = (uint8_t)(total_s % SEC_PER_MIN);
    uint8_t  minute  = (uint8_t)((total_s / SEC_PER_MIN) % MIN_WRAP);

    s_frame[0] = char_to_segments((char)('0' + minute / 10u));
    s_frame[1] = char_to_segments((char)('0' + minute % 10u)) | char_to_segments('.');
    s_frame[2] = char_to_segments((char)('0' + sec / 10u));
    s_frame[3] = char_to_segments((char)('0' + sec % 10u))    | char_to_segments('.');
    s_frame[4] = char_to_segments((char)('0' + millis / 100u));
    s_frame[5] = char_to_segments((char)('0' + (millis / 10u) % 10u));
    s_frame[6] = char_to_segments((char)('0' + millis % 10u));
    s_frame[7] = 0x00u;

    push_frame();
}

/* Per-button debounce state; each button remembers its own history across calls.
 * `stable` is the last confirmed level (true = released, since buttons are
 * active-low); `settling_ms` counts how long the raw reading has disagreed. */
typedef struct {
    bool    stable;
    uint8_t settling_ms;
} button_t;

/* Debounced press-edge detector. Call once per millisecond. Returns true exactly
 * once per physical press -- on the released->pressed transition.
 *
 * A mechanical contact bounces for a few ms around each press, flipping the raw
 * level many times; acting on a single sample would register one press as
 * several. So we require a *changed* level to hold steady for DEBOUNCE_MS
 * consecutive samples before trusting it. The buttons are active-low, so
 * pin_read() == false means "pressed": we report the edge only when the newly
 * committed level is the pressed one. */
static bool button_pressed(pin_t btn, button_t *st) {
    bool level = pin_read(btn);

    if (level == st->stable) {           /* matches the confirmed state -> steady */
        st->settling_ms = 0;
        return false;
    }

    if (++st->settling_ms >= DEBOUNCE_MS) {   /* the new level held long enough */
        st->stable      = level;
        st->settling_ms = 0;
        return (level == false);         /* committed to pressed -> this is the edge */
    }

    return false;                        /* still settling */
}

int main(void) {
    max7219_chain_init(s_disp);

    pin_set_input(BTN_STARTPAUSE, true);   /* input + internal pull-up (idles HIGH) */
    pin_set_input(BTN_CLEAR, true);

    /* Both buttons start released (HIGH) thanks to the pull-ups. */
    button_t startpause_st = { true, 0 };
    button_t clear_st       = { true, 0 };
    uint32_t last_sample_ms = 0;

    timer1_init();
    sei();                                  /* let the 1 ms tick run */

    for (;;) {
        uint32_t ticks;
        uint32_t snapshot;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { /* consistent reads of the 32-bit ISR globals */
            ticks    = s_ticks_ms;
            snapshot = s_elapsed_ms;
        }

        /* Sample the buttons on a fixed 1 ms cadence so debounce timing is
         * independent of how fast the render loop spins. */
        if (ticks != last_sample_ms) {
            last_sample_ms = ticks;

            if (button_pressed(BTN_STARTPAUSE, &startpause_st)) {
                s_running = !s_running;     /* single-byte write: atomic on AVR */
            }
            if (button_pressed(BTN_CLEAR, &clear_st)) {
                ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                    s_elapsed_ms = 0;       /* 32-bit write: guard against a mid-tick ISR */
                }
            }
        }

        render_time(snapshot);
    }

    return 0;
}
