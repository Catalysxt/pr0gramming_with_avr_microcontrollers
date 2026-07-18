#include <avr/io.h>          /* ADC + TCNT1 for PRNG entropy */
#include <avr/pgmspace.h>    /* attract banner + symbol bitmaps live in flash */
#include <stdbool.h>
#include "game.h"
#include "prng.h"
#include "seg7.h"
#include "dotmatrix.h"
#include "buzzer.h"
#include "symbols.h"
#include "melodies.h"

/* ======================================================================== *
 *  TUNING -- all game balance lives here so it is easy to retune in one place
 * ======================================================================== */

#define REEL_COUNT     3u
#define ACCENT_PANEL   3u        /* panel 3: activity / payline accent */
#define START_BALANCE  100u
#define SPIN_COST      1u
#define CREDIT_MAX     9999u     /* 4-digit display ceiling */

/* ---- PAYOUT TABLE --------------------------------------------------------
 * 3-of-a-kind pays PAYOUT_THREE[symbol], scaled by rarity: 7 > Diamond > Star >
 * Bar > Bell > fruit. Two-of-a-kind pays a fraction of that symbol's 3-kind
 * value (floored). A lone cherry pays a tiny consolation. 3x Seven is the
 * jackpot. Keyed by symbol_id_t so the values are order-independent. */
static const uint16_t PAYOUT_THREE[SYMBOL_COUNT] = {
    [SYM_ID_CHERRY]  = 40,
    [SYM_ID_LEMON]   = 50,
    [SYM_ID_MELON]   = 70,
    [SYM_ID_BELL]    = 100,
    [SYM_ID_BAR]     = 150,
    [SYM_ID_STAR]    = 240,
    [SYM_ID_DIAMOND] = 400,
    [SYM_ID_SEVEN]   = 777,
};
#define PAYOUT_TWO_NUM     1u    /* pair pays PAYOUT_THREE * 1/8 ... */
#define PAYOUT_TWO_DEN     8u
#define PAYOUT_TWO_MIN     2u    /* ... but at least this */
#define PAYOUT_CHERRY_ONE  5u    /* single-cherry consolation */
#define WIN_BIG_THRESHOLD  120u  /* >= this uses the longer count-up arpeggio */

/* Reel symbol distribution -- higher weight = more common, so 7/Diamond stay
 * rare and the jackpot is genuinely hard. */
static const uint8_t REEL_WEIGHT[SYMBOL_COUNT] = {
    [SYM_ID_CHERRY]  = 6,
    [SYM_ID_LEMON]   = 8,
    [SYM_ID_MELON]   = 7,
    [SYM_ID_BELL]    = 5,
    [SYM_ID_BAR]     = 5,
    [SYM_ID_STAR]    = 4,
    [SYM_ID_DIAMOND] = 3,
    [SYM_ID_SEVEN]   = 2,
};

/* ---- Reel animation timing (easing: fast -> slow -> snap) ---- */
#define SPIN_STEP_FAST_MS  25u   /* ms per pixel row while spinning fast */
#define SPIN_STEP_SLOW_MS  110u  /* ms per row once decelerating */
#define SPIN_EASE_MS       700u  /* window over which a reel eases to its stop */
#define SPIN_BASE_MS       700u  /* reel 0 spins at least this long */
#define SPIN_STAGGER_MS    450u  /* each later reel spins this much longer */
#define SPIN_JITTER_MS     350u  /* random extra so stops feel unpredictable */

/* ---- Win count-up ---- */
#define COUNTUP_STEPS      30u   /* whole count-up takes ~this many increments */
#define COUNTUP_STEP_MS    40u

/* ---- Jackpot / feedback animations ---- */
#define JACKPOT_FLASH_MS   120u
#define JACKPOT_MIN_MS     1500u /* jackpot celebration holds at least this long */
#define SHAKE_MS           450u  /* "insufficient credits" reel shake duration */
#define SHAKE_HALF_MS      60u

/* ---- Attract mode ---- */
#define ATTRACT_IDLE_MS    12000u  /* READY -> ATTRACT after this much inactivity */
#define BANNER_STEP_MS     70u     /* scroll speed of the SLOTS marquee */
#define PARADE_STEP_MS     220u    /* symbol-parade advance */
#define ATTRACT_PHASE_MS   4000u   /* time on each attract sub-mode */
#define ATTRACT_JINGLE_MS  8000u   /* spacing of the occasional attract jingle */

/* ---- Cash out (coin ticks accelerate then slow for a big finish) ---- */
#define CASHOUT_FAST_MS    25u
#define CASHOUT_SLOW_MS    120u
#define CASHOUT_TICKS      64u    /* ~this many coin ticks regardless of balance */

/* ======================================================================== *
 *  Attract banner data (generated; column-major, bit0 = top row)
 * ======================================================================== */
#define ATTRACT_STRIP_COLS 69u
static const uint8_t ATTRACT_STRIP[ATTRACT_STRIP_COLS] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x49, 0x49, 0x49,
    0x79, 0x00, 0x7F, 0x40, 0x40, 0x40, 0x40, 0x00, 0x3E, 0x41, 0x41, 0x41,
    0x3E, 0x00, 0x01, 0x01, 0x7F, 0x01, 0x01, 0x00, 0x4F, 0x49, 0x49, 0x49,
    0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* ======================================================================== *
 *  State
 * ======================================================================== */
typedef enum {
    STATE_ATTRACT = 0,
    STATE_READY,
    STATE_SPINNING,
    STATE_EVALUATE,
    STATE_JACKPOT,
    STATE_SETTINGS
} game_state_t;

typedef enum {
    MENU_PLAYCOUNT = 0,
    MENU_CASHOUT,
    MENU_EXIT,
    MENU_COUNT
} menu_item_t;

typedef struct {
    symbol_id_t current;      /* symbol currently centered / landed */
    symbol_id_t incoming;     /* next symbol scrolling up from the bottom */
    uint8_t     offset;       /* 0..8 scroll position within the pair */
    bool        spinning;
    uint32_t    next_step_ms; /* when to advance offset */
    uint32_t    stop_ms;      /* when this reel is due to stop (staggered) */
} reel_t;

static game_state_t s_state;
static uint32_t     s_last_input;      /* for the READY idle timeout */

static uint16_t s_balance;
static uint16_t s_last_win;
static uint16_t s_play_count;          /* spins since power-up (RAM only) */

static reel_t   s_reels[REEL_COUNT];
static bool     s_seeded;              /* PRNG mixed with TCNT1 yet? */
static uint16_t s_adc_seed;            /* boot ADC noise */

/* Win count-up */
static uint16_t s_win_target;
static uint16_t s_win_shown;
static uint16_t s_count_inc;
static uint32_t s_next_count_ms;

/* Jackpot flash */
static uint32_t s_jackpot_until;
static uint32_t s_next_flash_ms;
static bool     s_flash_on;

/* Insufficient-credit shake */
static uint32_t s_shake_until;

/* Attract animation */
static uint16_t s_scroll_col;
static uint32_t s_next_banner_ms;
static uint32_t s_next_parade_ms;
static uint8_t  s_attract_phase;       /* 0 = banner, 1 = symbol parade */
static uint32_t s_attract_phase_until;
static uint32_t s_next_attract_jingle;

/* Settings / cash-out */
static uint8_t  s_menu_index;
static bool     s_cashout_active;
static uint16_t s_cashout_initial;
static uint16_t s_cashout_paid;
static uint16_t s_cashout_chunk;
static uint32_t s_next_coin_ms;

/* ======================================================================== *
 *  Small helpers
 * ======================================================================== */

/* FSM transition seam. No-op today; left as the single choke point where a
 * future build can log (from -> to) over UART for debugging. */
static void state_transition(game_state_t from, game_state_t to) {
    (void)from;
    (void)to;
}

static void enter_state(game_state_t to, uint32_t now) {
    state_transition(s_state, to);
    s_state = to;
    (void)now;
}

/* Read a floating ADC pin (ADC0/PC0) and fold several conversions' noisy low
 * bits together for a boot entropy word. The pin is left as a plain input (no
 * pull-up), so it floats and its LSBs are genuinely noisy. */
static uint16_t adc_sample_noise(void) {
    ADMUX  = (1 << REFS0);                               /* AVcc ref, channel 0 */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); /* /128 */
    uint16_t acc = 0;
    for (uint8_t i = 0; i < 8; i++) {
        ADCSRA |= (1 << ADSC);
        loop_until_bit_is_clear(ADCSRA, ADSC);
        acc = (uint16_t)((acc << 1) ^ ADC);             /* stir in the noisy read */
    }
    ADCSRA &= (uint8_t)~(1 << ADEN);                    /* release the ADC/pin */
    return acc;
}

/* Weighted random reel symbol (rarer symbols weighted lower). */
static symbol_id_t random_symbol(void) {
    uint16_t total = 0;
    for (uint8_t i = 0; i < SYMBOL_COUNT; i++) {
        total += REEL_WEIGHT[i];
    }
    uint16_t r = prng_below(total);
    for (uint8_t i = 0; i < SYMBOL_COUNT; i++) {
        if (r < REEL_WEIGHT[i]) {
            return (symbol_id_t)i;
        }
        r = (uint16_t)(r - REEL_WEIGHT[i]);
    }
    return SYM_ID_CHERRY;   /* unreachable */
}

/* ======================================================================== *
 *  Rendering
 * ======================================================================== */

/* Blit one reel symbol into its panel, optionally shifted horizontally by
 * `shift` columns (used for the insufficient-credits shake). */
static void blit_reel(uint8_t panel, symbol_id_t sym, int8_t shift) {
    const uint8_t *bmp = SYMBOL_BITMAPS[sym];
    for (uint8_t r = 0; r < DOT_ROWS; r++) {
        uint8_t b = pgm_read_byte(&bmp[r]);
        if (shift > 0) {
            b = (uint8_t)(b >> shift);
        } else if (shift < 0) {
            b = (uint8_t)(b << (-shift));
        }
        dotmatrix_set_row(panel, r, b);
    }
}

static void render_reels_static(uint32_t now) {
    int8_t shift = 0;
    if ((int32_t)(now - s_shake_until) < 0) {          /* still shaking */
        shift = ((now / SHAKE_HALF_MS) & 1u) ? 1 : -1;
    }
    for (uint8_t i = 0; i < REEL_COUNT; i++) {
        blit_reel(i, s_reels[i].current, shift);
    }
}

/* Panel-3 accent: a moving sparkle while spinning, a striped flash on a win,
 * and a static centered payline marker otherwise. */
static void render_accent(uint32_t now) {
    for (uint8_t r = 0; r < DOT_ROWS; r++) {
        dotmatrix_set_row(ACCENT_PANEL, r, 0x00);
    }
    if (s_state == STATE_SPINNING) {
        uint8_t row = (uint8_t)((now / 60u) % DOT_ROWS);
        dotmatrix_set_row(ACCENT_PANEL, row, 0b00111100);
    } else if (s_state == STATE_EVALUATE || s_state == STATE_JACKPOT) {
        uint8_t phase = (uint8_t)((now / 100u) & 1u);
        for (uint8_t r = 0; r < DOT_ROWS; r++) {
            dotmatrix_set_row(ACCENT_PANEL, r, ((r + phase) & 1u) ? 0xFF : 0x00);
        }
    } else {
        dotmatrix_set_row(ACCENT_PANEL, 3, 0b00011000);
        dotmatrix_set_row(ACCENT_PANEL, 4, 0b00011000);
    }
}

/* Transpose a 32-column window of the column-major banner onto the four panels
 * to scroll "SLOTS" horizontally. bit(row) of each strip column maps to a row;
 * bit7 of a panel byte is the leftmost column. */
static void render_banner(uint16_t scroll) {
    for (uint8_t panel = 0; panel < DOT_PANELS; panel++) {
        for (uint8_t row = 0; row < DOT_ROWS; row++) {
            uint8_t bits = 0;
            for (uint8_t col = 0; col < 8; col++) {
                uint16_t idx = (uint16_t)((scroll + panel * 8u + col) % ATTRACT_STRIP_COLS);
                uint8_t colbyte = pgm_read_byte(&ATTRACT_STRIP[idx]);
                if ((colbyte >> row) & 1u) {
                    bits |= (uint8_t)(1u << (7u - col));
                }
            }
            dotmatrix_set_row(panel, row, bits);
        }
    }
}

static void render_parade(uint32_t now) {
    uint8_t base = (uint8_t)((now / 250u) % SYMBOL_COUNT);
    for (uint8_t p = 0; p < DOT_PANELS; p++) {
        dotmatrix_set_symbol(p, SYMBOL_BITMAPS[(base + p) % SYMBOL_COUNT]);
    }
}

/* Settings visuals: menu-index pips on the dot-matrix. */
static void render_settings_matrix(void) {
    dotmatrix_clear();
    for (uint8_t p = 0; p <= s_menu_index && p < DOT_PANELS; p++) {
        dotmatrix_set_row(p, 3, 0b00111100);
        dotmatrix_set_row(p, 4, 0b00111100);
    }
}

/* ======================================================================== *
 *  State entry helpers
 * ======================================================================== */
static void enter_ready(uint32_t now) {
    s_last_input = now;
    enter_state(STATE_READY, now);
}

static void enter_attract(uint32_t now) {
    s_scroll_col          = 0;
    s_next_banner_ms      = now;
    s_next_parade_ms      = now;
    s_attract_phase       = 0;
    s_attract_phase_until = now + ATTRACT_PHASE_MS;
    s_next_attract_jingle = now + ATTRACT_JINGLE_MS;
    enter_state(STATE_ATTRACT, now);
}

static void enter_settings(uint32_t now) {
    buzzer_play(MELODY_BUTTON, MELODY_BUTTON_LEN);
    s_menu_index     = MENU_PLAYCOUNT;
    s_cashout_active = false;
    dotmatrix_clear();
    enter_state(STATE_SETTINGS, now);
}

static void start_spin(uint32_t now) {
    /* First spin: fold TCNT1 (a live, user-timed value) into the ADC boot noise
     * so the whole session's sequence depends on when the player first pressed. */
    if (!s_seeded) {
        uint32_t seed = ((uint32_t)s_adc_seed << 16)
                      ^ ((uint32_t)TCNT1 << 3)
                      ^ (uint32_t)TCNT1
                      ^ (uint32_t)s_adc_seed;
        prng_seed(seed);
        s_seeded = true;
    }

    s_balance = (uint16_t)(s_balance - SPIN_COST);
    s_play_count++;
    s_last_win = 0;
    seg7_show_balance(s_balance);
    seg7_show_win(0);

    for (uint8_t i = 0; i < REEL_COUNT; i++) {
        s_reels[i].current      = random_symbol();
        s_reels[i].incoming     = random_symbol();
        s_reels[i].offset       = 0;
        s_reels[i].spinning     = true;
        s_reels[i].next_step_ms = now;
        s_reels[i].stop_ms      = now + SPIN_BASE_MS + (uint32_t)i * SPIN_STAGGER_MS
                                + prng_below(SPIN_JITTER_MS);
    }
    enter_state(STATE_SPINNING, now);
}

/* ======================================================================== *
 *  SPINNING
 * ======================================================================== */

/* Step period for a reel given how close it is to its stop time: full speed
 * until SPIN_EASE_MS remain, then linearly ramp to the slow period (easing). */
static uint16_t spin_period(uint32_t now, uint32_t stop_ms) {
    if ((int32_t)(now - stop_ms) >= 0) {
        return SPIN_STEP_SLOW_MS;
    }
    uint32_t remaining = stop_ms - now;
    if (remaining >= SPIN_EASE_MS) {
        return SPIN_STEP_FAST_MS;
    }
    uint32_t t = SPIN_EASE_MS - remaining;   /* 0 .. SPIN_EASE_MS */
    return (uint16_t)(SPIN_STEP_FAST_MS
        + (uint32_t)(SPIN_STEP_SLOW_MS - SPIN_STEP_FAST_MS) * t / SPIN_EASE_MS);
}

static void evaluate_enter(uint32_t now);   /* fwd */

static void spinning_step(uint32_t now) {
    bool any_spinning = false;
    bool stepped      = false;

    for (uint8_t i = 0; i < REEL_COUNT; i++) {
        reel_t *rl = &s_reels[i];
        if (!rl->spinning) {
            dotmatrix_set_symbol(i, SYMBOL_BITMAPS[rl->current]);
            continue;
        }
        any_spinning = true;

        if ((int32_t)(now - rl->next_step_ms) >= 0) {
            stepped = true;
            rl->offset++;
            if (rl->offset >= DOT_ROWS) {               /* completed one symbol */
                rl->offset  = 0;
                rl->current = rl->incoming;
                rl->incoming = random_symbol();
            }
            rl->next_step_ms = now + spin_period(now, rl->stop_ms);

            /* Stop only when past the due time AND aligned on a whole symbol. */
            if ((int32_t)(now - rl->stop_ms) >= 0 && rl->offset == 0) {
                rl->spinning = false;
                buzzer_play(MELODY_REEL_STOP, MELODY_REEL_STOP_LEN);  /* thunk */
            }
        }
        dotmatrix_scroll_panel(i, SYMBOL_BITMAPS[rl->current],
                               SYMBOL_BITMAPS[rl->incoming], rl->offset);
    }

    /* Spin-loop ticks fill the gaps between thunks; since step periods grow as
     * reels slow, the click rate slows with them. Don't clobber a thunk/jingle. */
    if (stepped && !buzzer_busy()) {
        buzzer_play(MELODY_CLICK, MELODY_CLICK_LEN);
    }

    render_accent(now);

    if (!any_spinning) {
        evaluate_enter(now);
    }
}

/* ======================================================================== *
 *  EVALUATE / JACKPOT
 * ======================================================================== */
static void evaluate_enter(uint32_t now) {
    symbol_id_t a = s_reels[0].current;
    symbol_id_t b = s_reels[1].current;
    symbol_id_t c = s_reels[2].current;

    uint16_t win  = 0;
    bool     jack = false;

    if (a == b && b == c) {                     /* three of a kind */
        win = PAYOUT_THREE[a];
        jack = (a == SYM_ID_SEVEN);
    } else if (a == b || b == c || a == c) {    /* two of a kind */
        symbol_id_t pair = (a == b || a == c) ? a : b;
        uint32_t w = (uint32_t)PAYOUT_THREE[pair] * PAYOUT_TWO_NUM / PAYOUT_TWO_DEN;
        if (w < PAYOUT_TWO_MIN) {
            w = PAYOUT_TWO_MIN;
        }
        win = (uint16_t)w;
    } else if (a == SYM_ID_CHERRY || b == SYM_ID_CHERRY || c == SYM_ID_CHERRY) {
        win = PAYOUT_CHERRY_ONE;                /* lone-cherry consolation */
    }

    uint32_t nb = (uint32_t)s_balance + win;
    s_balance  = (nb > CREDIT_MAX) ? CREDIT_MAX : (uint16_t)nb;
    s_last_win = win;
    seg7_show_balance(s_balance);

    /* Set up the last-win count-up (digit-by-digit). */
    s_win_target    = win;
    s_win_shown     = 0;
    s_count_inc     = (win > 0) ? (uint16_t)((win + COUNTUP_STEPS - 1u) / COUNTUP_STEPS) : 0;
    if (win > 0 && s_count_inc == 0) {
        s_count_inc = 1;
    }
    s_next_count_ms = now;
    seg7_show_win(0);

    if (jack) {
        buzzer_play(MELODY_JACKPOT, MELODY_JACKPOT_LEN);
        s_jackpot_until = now + JACKPOT_MIN_MS;
        s_next_flash_ms = now;
        s_flash_on      = false;
        enter_state(STATE_JACKPOT, now);
    } else if (win > 0) {
        if (win >= WIN_BIG_THRESHOLD) {
            buzzer_play(MELODY_ARP_UP, MELODY_ARP_UP_LEN);
        } else {
            buzzer_play(MELODY_SMALL_WIN, MELODY_SMALL_WIN_LEN);
        }
        enter_state(STATE_EVALUATE, now);
    } else {
        enter_ready(now);                       /* no win: straight back to READY */
    }
}

/* Advance the last-win count-up. Returns true once it reaches the target. */
static bool update_countup(uint32_t now) {
    if (s_win_shown >= s_win_target) {
        return true;
    }
    if ((int32_t)(now - s_next_count_ms) >= 0) {
        s_next_count_ms = now + COUNTUP_STEP_MS;
        uint32_t v = (uint32_t)s_win_shown + s_count_inc;
        if (v >= s_win_target) {
            v = s_win_target;
        }
        s_win_shown = (uint16_t)v;
        seg7_show_win(s_win_shown);
    }
    return (s_win_shown >= s_win_target);
}

static void evaluate_step(uint32_t now) {
    bool done = update_countup(now);
    render_reels_static(now);
    render_accent(now);
    if (done && !buzzer_busy()) {               /* let the arpeggio finish too */
        enter_ready(now);
    }
}

static void jackpot_step(uint32_t now) {
    update_countup(now);

    if ((int32_t)(now - s_next_flash_ms) >= 0) {
        s_next_flash_ms = now + JACKPOT_FLASH_MS;
        s_flash_on = !s_flash_on;
    }

    render_reels_static(now);                   /* the three lucky 7s */
    render_accent(now);
    if (s_flash_on) {
        dotmatrix_invert();                     /* one flash frame */
    }

    /* Blocks input for its duration: hold until the min time, the jingle, and
     * the count-up have all finished. */
    if ((int32_t)(now - s_jackpot_until) >= 0 && !buzzer_busy()
        && s_win_shown >= s_win_target) {
        enter_ready(now);
    }
}

/* ======================================================================== *
 *  ATTRACT / READY
 * ======================================================================== */
static void attract_step(uint32_t now, button_event_t ev_a, button_event_t ev_b) {
    if (ev_b == BUTTON_LONG) {
        enter_settings(now);
        return;
    }
    if (ev_a == BUTTON_SHORT || ev_b == BUTTON_SHORT) {
        buzzer_play(MELODY_BUTTON, MELODY_BUTTON_LEN);
        enter_ready(now);
        return;
    }

    seg7_show_balance(s_balance);
    seg7_show_win(0);

    if ((int32_t)(now - s_next_attract_jingle) >= 0) {
        if (!buzzer_busy()) {
            buzzer_play(MELODY_ATTRACT, MELODY_ATTRACT_LEN);
        }
        s_next_attract_jingle = now + ATTRACT_JINGLE_MS;
    }

    if ((int32_t)(now - s_attract_phase_until) >= 0) {
        s_attract_phase ^= 1u;                  /* toggle banner <-> parade */
        s_attract_phase_until = now + ATTRACT_PHASE_MS;
    }

    if (s_attract_phase == 0) {                 /* scrolling SLOTS banner */
        if ((int32_t)(now - s_next_banner_ms) >= 0) {
            s_next_banner_ms = now + BANNER_STEP_MS;
            s_scroll_col = (uint16_t)((s_scroll_col + 1u) % ATTRACT_STRIP_COLS);
            render_banner(s_scroll_col);
            render_accent(now);
        }
    } else {                                    /* cycling symbol parade */
        if ((int32_t)(now - s_next_parade_ms) >= 0) {
            s_next_parade_ms = now + PARADE_STEP_MS;
            render_parade(now);
            render_accent(now);
        }
    }
}

static void ready_step(uint32_t now, button_event_t ev_a, button_event_t ev_b) {
    if (ev_b == BUTTON_LONG) {
        enter_settings(now);
        return;
    }
    if (ev_a == BUTTON_SHORT) {
        if (s_balance >= SPIN_COST) {
            buzzer_play(MELODY_BUTTON, MELODY_BUTTON_LEN);
            start_spin(now);
            return;
        }
        buzzer_play(MELODY_INSUFFICIENT, MELODY_INSUFFICIENT_LEN);
        s_shake_until = now + SHAKE_MS;
        s_last_input  = now;
    }
    if (ev_b == BUTTON_SHORT) {
        buzzer_play(MELODY_BUTTON, MELODY_BUTTON_LEN);
        s_last_input = now;
    }

    if ((int32_t)(now - (s_last_input + ATTRACT_IDLE_MS)) >= 0) {
        enter_attract(now);
        return;
    }

    seg7_show_balance(s_balance);
    seg7_show_win(s_last_win);
    render_reels_static(now);
    render_accent(now);
}

/* ======================================================================== *
 *  SETTINGS (+ cash out)
 * ======================================================================== */
static void start_cashout(uint32_t now) {
    if (s_balance == 0) {
        buzzer_play(MELODY_INSUFFICIENT, MELODY_INSUFFICIENT_LEN);
        return;
    }
    s_cashout_initial = s_balance;
    s_cashout_paid    = 0;
    s_cashout_chunk   = (uint16_t)(s_balance / CASHOUT_TICKS);
    if (s_cashout_chunk == 0) {
        s_cashout_chunk = 1;
    }
    s_next_coin_ms   = now;
    s_cashout_active = true;
    seg7_show_win(0);
}

/* Coin-tick period: slow at the start, fastest in the middle, slowing again for
 * a big finish -- a triangle in |2*paid - initial|. */
static uint16_t cashout_period(void) {
    uint32_t total = s_cashout_initial;
    uint32_t done  = s_cashout_paid;
    uint32_t dist  = (2u * done > total) ? (2u * done - total) : (total - 2u * done);
    return (uint16_t)(CASHOUT_FAST_MS
        + (uint32_t)(CASHOUT_SLOW_MS - CASHOUT_FAST_MS) * dist / (total ? total : 1u));
}

static void cashout_step(uint32_t now) {
    if (s_balance == 0) {                        /* finished draining */
        s_cashout_active = false;
        buzzer_play(MELODY_ARP_UP, MELODY_ARP_UP_LEN);   /* big finish */
        s_menu_index = MENU_EXIT;                /* park on Exit */
        return;
    }
    if ((int32_t)(now - s_next_coin_ms) >= 0) {
        uint16_t chunk = (s_cashout_chunk > s_balance) ? s_balance : s_cashout_chunk;
        s_balance     = (uint16_t)(s_balance - chunk);
        s_cashout_paid = (uint16_t)(s_cashout_paid + chunk);
        seg7_show_balance(s_balance);
        seg7_show_win(s_cashout_paid);
        buzzer_play(MELODY_COIN, MELODY_COIN_LEN);
        s_next_coin_ms = now + cashout_period();
    }
    render_settings_matrix();
}

static void render_menu(void) {
    switch ((menu_item_t)s_menu_index) {
        case MENU_PLAYCOUNT:
            seg7_show_text(SEG7_DISPLAY_A, "PLAY");
            seg7_show_win(s_play_count);
            break;
        case MENU_CASHOUT:
            seg7_show_text(SEG7_DISPLAY_A, "CASH");
            seg7_show_win(s_balance);
            break;
        case MENU_EXIT:
        default:
            seg7_show_text(SEG7_DISPLAY_A, "EXIT");
            seg7_show_text(SEG7_DISPLAY_B, "");
            break;
    }
}

static void settings_step(uint32_t now, button_event_t ev_a, button_event_t ev_b) {
    if (s_cashout_active) {                       /* no exit mid-cash-out */
        cashout_step(now);
        return;
    }

    if (ev_b == BUTTON_SHORT) {                   /* next menu item */
        s_menu_index = (uint8_t)((s_menu_index + 1u) % MENU_COUNT);
        buzzer_play(MELODY_BUTTON, MELODY_BUTTON_LEN);
    }
    if (ev_a == BUTTON_SHORT) {                    /* select / confirm */
        buzzer_play(MELODY_BUTTON, MELODY_BUTTON_LEN);
        switch ((menu_item_t)s_menu_index) {
            case MENU_PLAYCOUNT:
                break;                             /* display-only */
            case MENU_CASHOUT:
                start_cashout(now);
                break;
            case MENU_EXIT:
            default:
                enter_ready(now);
                return;
        }
    }

    render_menu();
    render_settings_matrix();
}

/* ======================================================================== *
 *  Public API
 * ======================================================================== */
void game_init(void) {
    s_balance     = START_BALANCE;
    s_last_win    = 0;
    s_play_count  = 0;
    s_seeded      = false;
    s_shake_until = 0;

    s_adc_seed = adc_sample_noise();
    prng_seed(s_adc_seed);                        /* provisional; remixed on spin 1 */

    /* A pleasant idle line-up before the first spin. */
    s_reels[0].current = SYM_ID_CHERRY;
    s_reels[1].current = SYM_ID_SEVEN;
    s_reels[2].current = SYM_ID_BAR;
    for (uint8_t i = 0; i < REEL_COUNT; i++) {
        s_reels[i].incoming = s_reels[i].current;
        s_reels[i].offset   = 0;
        s_reels[i].spinning = false;
    }

    seg7_show_balance(s_balance);
    seg7_show_win(0);
    dotmatrix_clear();

    enter_attract(0);
}

void game_update(uint32_t now_ms, button_event_t ev_a, button_event_t ev_b) {
    switch (s_state) {
        case STATE_ATTRACT:  attract_step(now_ms, ev_a, ev_b); break;
        case STATE_READY:    ready_step(now_ms, ev_a, ev_b);   break;
        case STATE_SPINNING: spinning_step(now_ms);            break;  /* input locked */
        case STATE_EVALUATE: evaluate_step(now_ms);            break;
        case STATE_JACKPOT:  jackpot_step(now_ms);             break;  /* input locked */
        case STATE_SETTINGS: settings_step(now_ms, ev_a, ev_b); break;
        default:             enter_attract(now_ms);            break;
    }
}
