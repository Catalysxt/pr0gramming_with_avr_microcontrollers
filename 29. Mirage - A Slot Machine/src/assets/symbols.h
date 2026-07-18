#ifndef SLOT_MACHINE_SYMBOLS_H_
#define SLOT_MACHINE_SYMBOLS_H_

#include <stdint.h>
#include <avr/pgmspace.h>

/* 8x8 reel icons, hand-drawn as bitmaps and kept in flash (PROGMEM). Bit
 * convention matches dotmatrix.c: row 0 = top, bit 7 = leftmost column. The
 * preview in each row's comment is the literal pixel pattern of that byte.
 *
 * This header is an asset module -- include it only from the app layer (game.c).
 * It also defines the symbol id enum and a lookup table; game.c owns the reel
 * weighting and payout tuning that reference these ids.
 *
 * Generated/verified so previews always match the bytes (see scratchpad
 * gen_symbols.py in the session that authored this). */

/* Cherry             */
static const uint8_t SYM_CHERRY[8] PROGMEM = {
    0b00000010,   /* |      # | */
    0b00000100,   /* |     #  | */
    0b00001000,   /* |    #   | */
    0b00101000,   /* |  # #   | */
    0b01110110,   /* | ### ## | */
    0b11111101,   /* |###### #| */
    0b11111110,   /* |####### | */
    0b01111100,   /* | #####  | */
};

/* Lemon              */
static const uint8_t SYM_LEMON[8] PROGMEM = {
    0b00000000,   /* |        | */
    0b00011000,   /* |   ##   | */
    0b00111100,   /* |  ####  | */
    0b01111110,   /* | ###### | */
    0b01111110,   /* | ###### | */
    0b01111110,   /* | ###### | */
    0b00111100,   /* |  ####  | */
    0b00011000,   /* |   ##   | */
};

/* Melon              */
static const uint8_t SYM_MELON[8] PROGMEM = {
    0b00111100,   /* |  ####  | */
    0b01000010,   /* | #    # | */
    0b10011001,   /* |#  ##  #| */
    0b10111101,   /* |# #### #| */
    0b10111101,   /* |# #### #| */
    0b10011001,   /* |#  ##  #| */
    0b01000010,   /* | #    # | */
    0b00111100,   /* |  ####  | */
};

/* Bell               */
static const uint8_t SYM_BELL[8] PROGMEM = {
    0b00011000,   /* |   ##   | */
    0b00111100,   /* |  ####  | */
    0b00111100,   /* |  ####  | */
    0b01111110,   /* | ###### | */
    0b01111110,   /* | ###### | */
    0b11111111,   /* |########| */
    0b00011000,   /* |   ##   | */
    0b00011000,   /* |   ##   | */
};

/* Star               */
static const uint8_t SYM_STAR[8] PROGMEM = {
    0b00011000,   /* |   ##   | */
    0b00011000,   /* |   ##   | */
    0b01111110,   /* | ###### | */
    0b11111111,   /* |########| */
    0b01111110,   /* | ###### | */
    0b00111100,   /* |  ####  | */
    0b01100110,   /* | ##  ## | */
    0b01000010,   /* | #    # | */
};

/* Seven              */
static const uint8_t SYM_SEVEN[8] PROGMEM = {
    0b01111110,   /* | ###### | */
    0b01000010,   /* | #    # | */
    0b00000110,   /* |     ## | */
    0b00001100,   /* |    ##  | */
    0b00011000,   /* |   ##   | */
    0b00011000,   /* |   ##   | */
    0b00011000,   /* |   ##   | */
    0b00011000,   /* |   ##   | */
};

/* Diamond            */
static const uint8_t SYM_DIAMOND[8] PROGMEM = {
    0b00011000,   /* |   ##   | */
    0b00111100,   /* |  ####  | */
    0b01111110,   /* | ###### | */
    0b11111111,   /* |########| */
    0b01111110,   /* | ###### | */
    0b00111100,   /* |  ####  | */
    0b00011000,   /* |   ##   | */
    0b00000000,   /* |        | */
};

/* Bar                */
static const uint8_t SYM_BAR[8] PROGMEM = {
    0b00000000,   /* |        | */
    0b11111111,   /* |########| */
    0b10000001,   /* |#      #| */
    0b10111101,   /* |# #### #| */
    0b10111101,   /* |# #### #| */
    0b10000001,   /* |#      #| */
    0b11111111,   /* |########| */
    0b00000000,   /* |        | */
};

/* Symbol ids. Order is low->high value; game.c's payout/weight tables are keyed
 * by this enum, and the reel PRNG returns one of these. */
typedef enum {
    SYM_ID_CHERRY = 0,
    SYM_ID_LEMON,
    SYM_ID_MELON,
    SYM_ID_BELL,
    SYM_ID_BAR,
    SYM_ID_STAR,
    SYM_ID_DIAMOND,
    SYM_ID_SEVEN,
    SYMBOL_COUNT
} symbol_id_t;

/* id -> 8-byte PROGMEM bitmap. RAM cost is one 16-byte pointer table (in game.c,
 * the only includer). Read an entry then pass it to the dotmatrix blit. */
static const uint8_t *const SYMBOL_BITMAPS[SYMBOL_COUNT] = {
    SYM_CHERRY, SYM_LEMON, SYM_MELON, SYM_BELL,
    SYM_BAR,    SYM_STAR,  SYM_DIAMOND, SYM_SEVEN,
};

#endif /* SLOT_MACHINE_SYMBOLS_H_ */
