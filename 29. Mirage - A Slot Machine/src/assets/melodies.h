#ifndef SLOT_MACHINE_MELODIES_H_
#define SLOT_MACHINE_MELODIES_H_

#include <avr/pgmspace.h>
#include "buzzer.h"   /* note_t { freq_hz, dur_ms } */

/* Casino sound effects as PROGMEM note sequences fed to the buzzer engine.
 * Include only from the app layer (game.c). Each melody has a matching _LEN via
 * sizeof so callers pass buzzer_play(MELODY_X, MELODY_X_LEN). freq 0 = rest. */

#define REST 0u

/* Equal-tempered pitches (Hz, rounded). Named so the sequences read musically
 * and carry no magic numbers. */
#define NOTE_G3  196u
#define NOTE_A3  220u
#define NOTE_C4  262u
#define NOTE_E4  330u
#define NOTE_G4  392u
#define NOTE_A4  440u
#define NOTE_C5  523u
#define NOTE_D5  587u
#define NOTE_E5  659u
#define NOTE_G5  784u
#define NOTE_A5  880u
#define NOTE_C6 1047u
#define NOTE_E6 1319u
#define NOTE_G6 1568u

/* Low, un-pitched-ish thunks for mechanical feedback. */
#define THUNK_HI 150u
#define THUNK_LO 110u

#define MELODY_LEN(m) ((uint8_t)(sizeof(m) / sizeof((m)[0])))

/* Subtle click on every button press. */
static const note_t MELODY_BUTTON[] PROGMEM = {
    { NOTE_A5, 12 },
};

/* Spin-loop tick -- one very short blip per reel step; the game slows the rate
 * as reels decelerate, so the perceived click rate slows with them. */
static const note_t MELODY_CLICK[] PROGMEM = {
    { NOTE_C6, 6 },
};

/* Distinct low "thunk" when a reel snaps to its stop. */
static const note_t MELODY_REEL_STOP[] PROGMEM = {
    { THUNK_HI, 25 },
    { THUNK_LO, 45 },
};

/* Small-win ascending arpeggio (3-5 notes). */
static const note_t MELODY_SMALL_WIN[] PROGMEM = {
    { NOTE_C5, 70 }, { NOTE_E5, 70 }, { NOTE_G5, 70 }, { NOTE_C6, 130 },
};

/* Count-up arpeggio -- a longer ascending run that plays under the last-win
 * digit count-up on a bigger (non-jackpot) win. */
static const note_t MELODY_ARP_UP[] PROGMEM = {
    { NOTE_C5, 60 }, { NOTE_E5, 60 }, { NOTE_G5, 60 },
    { NOTE_C6, 60 }, { NOTE_E6, 60 }, { NOTE_G6, 120 },
};

/* Big win / jackpot jingle (~1.7 s). */
static const note_t MELODY_JACKPOT[] PROGMEM = {
    { NOTE_G4, 120 }, { NOTE_C5, 120 }, { NOTE_E5, 120 }, { NOTE_G5, 200 },
    { REST,     40 },
    { NOTE_E5, 120 }, { NOTE_G5, 120 }, { NOTE_C6, 300 },
    { REST,     40 },
    { NOTE_G5, 100 }, { NOTE_C6, 100 }, { NOTE_E6, 100 }, { NOTE_G6, 360 },
};

/* Cash-out coin tick -- one coin drop; the game repeats it, accelerating then
 * slowing, as the balance decrements. */
static const note_t MELODY_COIN[] PROGMEM = {
    { NOTE_E6, 10 }, { NOTE_C6, 14 },
};

/* Insufficient-credits descending "raspberry" buzz. */
static const note_t MELODY_INSUFFICIENT[] PROGMEM = {
    { NOTE_A3, 90 }, { NOTE_G3, 90 }, { THUNK_HI, 120 }, { THUNK_LO, 180 },
};

/* Occasional attract jingle in idle mode. */
static const note_t MELODY_ATTRACT[] PROGMEM = {
    { NOTE_C5, 90 }, { NOTE_G4, 90 }, { NOTE_C5, 90 }, { NOTE_E5, 160 },
};

#define MELODY_BUTTON_LEN        MELODY_LEN(MELODY_BUTTON)
#define MELODY_CLICK_LEN         MELODY_LEN(MELODY_CLICK)
#define MELODY_REEL_STOP_LEN     MELODY_LEN(MELODY_REEL_STOP)
#define MELODY_SMALL_WIN_LEN     MELODY_LEN(MELODY_SMALL_WIN)
#define MELODY_ARP_UP_LEN        MELODY_LEN(MELODY_ARP_UP)
#define MELODY_JACKPOT_LEN       MELODY_LEN(MELODY_JACKPOT)
#define MELODY_COIN_LEN          MELODY_LEN(MELODY_COIN)
#define MELODY_INSUFFICIENT_LEN  MELODY_LEN(MELODY_INSUFFICIENT)
#define MELODY_ATTRACT_LEN       MELODY_LEN(MELODY_ATTRACT)

#endif /* SLOT_MACHINE_MELODIES_H_ */
