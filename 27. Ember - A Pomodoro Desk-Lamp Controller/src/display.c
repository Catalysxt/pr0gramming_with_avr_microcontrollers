/*
 * display.c
 *
 * Purpose : Multiplex a 4-digit common-cathode 7-segment display, flicker-free,
 *           entirely from a Timer2 CTC ISR. Main context only writes a small
 *           framebuffer; the ISR does all the pin toggling.
 * Hardware: SEG[8] + DIGIT[4] from hardware_connections.h. Digit commons are
 *           switched by NPN low-side transistors (active-HIGH select); segment
 *           anodes are sourced through series resistors (active-HIGH = lit).
 *           Polarity is set purely by EMBER_*_ACTIVE_LOW there, not here.
 * Datasheet: ATmega328P §17.11 (Timer2 regs), Table 17-8 (CTC mode),
 *            Table 17-9 (CS2x prescaler). Display: Kingbright CC56-12SRWA.
 *
 * Refresh math: 1 ms/digit -> 4 ms/frame -> 250 Hz, well above the ~60 Hz
 * flicker-fusion threshold.
 *   f_isr = F_CPU / (prescaler * (OCR2A + 1))
 *   1 kHz = 16e6 / (64 * (249 + 1))
 */

#include "display.h"
#include "hardware_connections.h"   /* SEG, DIGIT, EMBER_*_ACTIVE_LOW */
#include "pin.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define NUM_DIGITS   4u
#define SEG_DP_BIT   7u    /* bit index of the decimal point in a font byte */
#define BLANK        0u    /* a framebuffer byte with no segments lit       */

/*
 * 7-segment font, common bit order bit0=a .. bit6=g, bit7=DP.
 * A set bit means "this segment is lit". PROGMEM: the table lives in flash,
 * not SRAM (we have only 2 KB of RAM). Read with pgm_read_byte().
 *
 *      aaa
 *     f   b
 *     f   b
 *      ggg
 *     e   c
 *     e   c
 *      ddd
 */
static const uint8_t k_font[10] PROGMEM = {
    0x3Fu,  /* 0: a b c d e f   */
    0x06u,  /* 1: b c           */
    0x5Bu,  /* 2: a b g e d     */
    0x4Fu,  /* 3: a b g c d     */
    0x66u,  /* 4: f g b c       */
    0x6Du,  /* 5: a f g c d     */
    0x7Du,  /* 6: a f g e d c   */
    0x07u,  /* 7: a b c         */
    0x7Fu,  /* 8: all           */
    0x6Fu,  /* 9: a b c d f g   */
};

/* Framebuffer: one font byte per digit (leftmost = index 0). volatile —
 * written by main context, read by the ISR. Each element is a single byte,
 * so writes are atomic on the 8-bit core; no ATOMIC_BLOCK needed. */
static volatile uint8_t s_fb[NUM_DIGITS] = { BLANK, BLANK, BLANK, BLANK };

/* Colon = the DP on digit index 1 (between the MM and SS pairs). */
static volatile bool s_colon = false;

/* ---- low-level segment/digit helpers (respect configured polarity) ---- */

static void seg_write(uint8_t bit_index, bool lit)
{
#if EMBER_SEG_ACTIVE_LOW
    if (lit) { pin_low(SEG[bit_index]); }  else { pin_high(SEG[bit_index]); }
#else
    if (lit) { pin_high(SEG[bit_index]); } else { pin_low(SEG[bit_index]); }
#endif
}

static void digit_select(uint8_t idx, bool on)
{
#if EMBER_DIGIT_ACTIVE_LOW
    if (on) { pin_low(DIGIT[idx]); }  else { pin_high(DIGIT[idx]); }
#else
    if (on) { pin_high(DIGIT[idx]); } else { pin_low(DIGIT[idx]); }
#endif
}

void display_init(void)
{
    for (uint8_t i = 0; i < 8u; i++) {
        pin_set_output(SEG[i]);
        seg_write(i, false);            /* segments off */
    }
    for (uint8_t d = 0; d < NUM_DIGITS; d++) {
        pin_set_output(DIGIT[d]);
        digit_select(d, false);         /* all digits deselected */
    }
    display_resume();
}

void display_resume(void)
{
    /* Timer2 CTC, prescaler /64, 1 kHz compare interrupt (see header math).
     * WGM21=1 -> CTC (Table 17-8). CS22=1 -> /64 (Table 17-9). */
    TCCR2A = (1u << WGM21);
    TCCR2B = (1u << CS22);
    OCR2A  = 249u;
    TIMSK2 = (1u << OCIE2A);
}

void display_stop(void)
{
    TIMSK2 = 0u;                        /* silence the refresh interrupt */
    for (uint8_t d = 0; d < NUM_DIGITS; d++) {
        digit_select(d, false);         /* park with nothing lit         */
    }
}

void display_blank(void)
{
    for (uint8_t d = 0; d < NUM_DIGITS; d++) {
        s_fb[d] = BLANK;
    }
    s_colon = false;
}

void display_set_colon(bool on)
{
    s_colon = on;
}

void display_show_uint(uint16_t value)
{
    /* Right-aligned, leading digits blanked (not zero-padded). */
    uint8_t buf[NUM_DIGITS] = { BLANK, BLANK, BLANK, BLANK };
    uint8_t i = NUM_DIGITS;
    do {
        i--;
        buf[i] = pgm_read_byte(&k_font[value % 10u]);
        value /= 10u;
    } while ((value != 0u) && (i != 0u));

    for (uint8_t d = 0; d < NUM_DIGITS; d++) {
        s_fb[d] = buf[d];
    }
    s_colon = false;
}

void display_show_mmss(uint8_t min, uint8_t sec)
{
    if (min > 99u) { min = 99u; }
    if (sec > 59u) { sec = 59u; }

    s_fb[0] = pgm_read_byte(&k_font[(min / 10u) % 10u]);
    s_fb[1] = pgm_read_byte(&k_font[min % 10u]);
    s_fb[2] = pgm_read_byte(&k_font[(sec / 10u) % 10u]);
    s_fb[3] = pgm_read_byte(&k_font[sec % 10u]);
}

/*
 * Fires every 1 ms. Lights exactly one digit per call, cycling 0..3. Order is
 * critical to avoid ghosting: blank the previously-lit digit, load this
 * digit's segments, only then enable this digit's anode.
 */
ISR(TIMER2_COMPA_vect)
{
    static uint8_t cur = 0;

    digit_select(cur, false);                 /* 1. blank the old digit    */

    cur = (uint8_t)((cur + 1u) & 0x03u);       /* advance 0->1->2->3->0     */

    uint8_t mask = s_fb[cur];
    if ((cur == 1u) && s_colon) {              /* colon rides digit 1's DP  */
        mask |= (uint8_t)(1u << SEG_DP_BIT);
    }

    for (uint8_t b = 0; b < 8u; b++) {         /* 2. paint the segments     */
        seg_write(b, (mask & (uint8_t)(1u << b)) != 0u);
    }

    digit_select(cur, true);                   /* 3. light the new digit    */
}
