// variants.h - the poker games: how each one is dealt, bet and ranked (no raylib dependency)
// Rules follow Wikipedia's articles on each game (see README).
#pragma once
#include <stdbool.h>

enum { V_HOLDEM, V_OMAHA, V_OMAHA8, V_OMAHA5, V_SHORTDECK, V_STUD, V_STUD8, V_RAZZ, V_DRAW5, V_TD27, V_SD27, V_BADUGI, V_COUNT };
enum { FAM_BOARD, FAM_STUD, FAM_DRAW };
// how hands are ranked: high, ace-to-five low, deuce-to-seven low, badugi, or split high / 8-or-better low
enum { RK_HIGH, RK_LOW_A5, RK_LOW_27, RK_BADUGI, RK_HILO8 };
enum { BL_NOLIMIT, BL_POTLIMIT, BL_FIXED, BL_COUNT };

typedef struct {
    const char *name;        // "Seven-Card Stud"
    const char *shortName;   // "Stud" (table title, buttons)
    const char *blurb;       // one line for the menu
    int family;              // FAM_*
    int nhole;               // cards each player ends up with (stud: 7)
    int deck;                // 52, or 36 for short deck (6 to ace)
    int rank;                // RK_*
    bool omaha;              // must use exactly 2 hole cards + 3 board cards
    int draws;               // draw games: number of draws
    int rounds;              // betting rounds
    int bigFrom;             // fixed limit: first betting round that uses the big bet
    int limits;              // bit per BL_* this game is played with
    int defLimit;            // usual structure
    const char *streets[5];  // name of each betting round
} VariantInfo;

const VariantInfo *varInfo(int v);
const char *limitName(int limit);      // "No-Limit", "Pot-Limit", "Fixed-Limit"
const char *limitShort(int limit);     // "NL", "PL", "FL"
bool varHasBoard(int v);
bool varIsLow(int v);                  // the low hand wins the whole pot (Razz, 2-7, Badugi)
bool varIsSplit(int v);                // hi-lo: high and low split the pot

// mixed games: one orbit of each game, in order
enum { MIX_NONE, MIX_HORSE, MIX_8GAME, MIX_COUNT };
typedef struct { int variant, limit; } MixGame;
int mixGames(int mix, const MixGame **out);
const char *mixName(int mix);          // "HORSE"
const char *mixBlurb(int mix);
