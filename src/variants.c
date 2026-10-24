// variants.c - the table of poker games
#include "variants.h"

#define NL (1 << BL_NOLIMIT)
#define PL (1 << BL_POTLIMIT)
#define FL (1 << BL_FIXED)

#define BOARD_ST { "Preflop", "Flop", "Turn", "River", "" }
#define STUD_ST { "3rd street", "4th street", "5th street", "6th street", "7th street" }
#define TRIPLE_ST { "Before the first draw", "After the 1st draw", "After the 2nd draw", "After the 3rd draw", "" }
#define SINGLE_ST { "Before the draw", "After the draw", "", "", "" }

static const VariantInfo V[V_COUNT] = {
    [V_HOLDEM] = { "Texas Hold'em", "Hold'em", "2 cards each + 5 shared cards. Best 5 of 7 wins.",
                   FAM_BOARD, 2, 52, RK_HIGH, false, 0, 4, 2, NL | PL | FL, BL_NOLIMIT,
                   BOARD_ST },
    [V_OMAHA] = { "Omaha", "Omaha", "4 cards each; use exactly 2 of them + 3 shared cards.",
                  FAM_BOARD, 4, 52, RK_HIGH, true, 0, 4, 2, NL | PL | FL, BL_POTLIMIT,
                  BOARD_ST },
    [V_OMAHA8] = { "Omaha Hi-Lo", "Omaha Hi-Lo", "Omaha where the best high and the best low (8 or better) split the pot.",
                   FAM_BOARD, 4, 52, RK_HILO8, true, 0, 4, 2, PL | FL, BL_FIXED,
                   BOARD_ST },
    [V_OMAHA5] = { "5-Card Omaha", "5-Card Omaha", "Omaha with 5 cards each - still exactly 2 of them + 3 shared.",
                   FAM_BOARD, 5, 52, RK_HIGH, true, 0, 4, 2, PL | NL, BL_POTLIMIT,
                   BOARD_ST },
    [V_SHORTDECK] = { "Short Deck Hold'em", "Short Deck", "Hold'em with the 2s-5s removed. A flush beats a full house.",
                      FAM_BOARD, 2, 36, RK_HIGH, false, 0, 4, 2, NL | PL | FL, BL_NOLIMIT,
                      BOARD_ST },
    [V_STUD] = { "Seven-Card Stud", "7-Card Stud", "No shared cards: 3 face down, 4 face up. Best 5 of your 7 wins.",
                 FAM_STUD, 7, 52, RK_HIGH, false, 0, 5, 2, FL, BL_FIXED,
                 STUD_ST },
    [V_STUD8] = { "Stud Hi-Lo", "Stud Hi-Lo", "Seven-card stud; the high and the low (8 or better) split the pot.",
                  FAM_STUD, 7, 52, RK_HILO8, false, 0, 5, 2, FL, BL_FIXED,
                  STUD_ST },
    [V_RAZZ] = { "Razz", "Razz", "Seven-card stud for the LOWEST hand. A-2-3-4-5 is the best.",
                 FAM_STUD, 7, 52, RK_LOW_A5, false, 0, 5, 2, FL, BL_FIXED,
                 STUD_ST },
    [V_DRAW5] = { "Five-Card Draw", "5-Card Draw", "5 cards each, one chance to swap cards, best high hand wins.",
                  FAM_DRAW, 5, 52, RK_HIGH, false, 1, 2, 1, FL | PL | NL, BL_FIXED,
                  SINGLE_ST },
    [V_TD27] = { "2-7 Triple Draw", "2-7 Triple Draw", "Lowball: three draws to the lowest hand. 7-5-4-3-2 is the best.",
                 FAM_DRAW, 5, 52, RK_LOW_27, false, 3, 4, 2, FL | PL, BL_FIXED,
                 TRIPLE_ST },
    [V_SD27] = { "2-7 Single Draw", "2-7 Single Draw", "Lowball with one draw, usually no-limit. 7-5-4-3-2 is the best.",
                 FAM_DRAW, 5, 52, RK_LOW_27, false, 1, 2, 1, NL | PL | FL, BL_NOLIMIT,
                 SINGLE_ST },
    [V_BADUGI] = { "Badugi", "Badugi", "4 cards, three draws: low cards of four different suits win.",
                   FAM_DRAW, 4, 52, RK_BADUGI, false, 3, 4, 2, FL | PL, BL_FIXED,
                   TRIPLE_ST },
};

const VariantInfo *varInfo(int v) { return &V[v >= 0 && v < V_COUNT ? v : 0]; }

const char *limitName(int limit) {
    static const char *N[BL_COUNT] = { "No-Limit", "Pot-Limit", "Fixed-Limit" };
    return N[limit >= 0 && limit < BL_COUNT ? limit : 0];
}

const char *limitShort(int limit) {
    static const char *N[BL_COUNT] = { "NL", "PL", "FL" };
    return N[limit >= 0 && limit < BL_COUNT ? limit : 0];
}

bool varHasBoard(int v) { return varInfo(v)->family == FAM_BOARD; }
bool varIsLow(int v) { int r = varInfo(v)->rank; return r == RK_LOW_A5 || r == RK_LOW_27 || r == RK_BADUGI; }
bool varIsSplit(int v) { return varInfo(v)->rank == RK_HILO8; }

// HORSE: Hold'em, Omaha hi-lo, Razz, Stud, stud hi-lo (Eight-or-better), all fixed limit.
// 8-Game: 2-7 triple draw, limit Hold'em, Omaha hi-lo, Razz, Stud, Stud hi-lo, no-limit Hold'em, pot-limit Omaha.
static const MixGame HORSE[] = { { V_HOLDEM, BL_FIXED }, { V_OMAHA8, BL_FIXED }, { V_RAZZ, BL_FIXED }, { V_STUD, BL_FIXED }, { V_STUD8, BL_FIXED } };
static const MixGame EIGHT[] = { { V_TD27, BL_FIXED }, { V_HOLDEM, BL_FIXED }, { V_OMAHA8, BL_FIXED }, { V_RAZZ, BL_FIXED },
                                 { V_STUD, BL_FIXED }, { V_STUD8, BL_FIXED }, { V_HOLDEM, BL_NOLIMIT }, { V_OMAHA, BL_POTLIMIT } };

int mixGames(int mix, const MixGame **out) {
    if (mix == MIX_HORSE) { *out = HORSE; return (int)(sizeof HORSE / sizeof HORSE[0]); }
    if (mix == MIX_8GAME) { *out = EIGHT; return (int)(sizeof EIGHT / sizeof EIGHT[0]); }
    *out = 0;
    return 0;
}

const char *mixName(int mix) { return mix == MIX_HORSE ? "HORSE" : mix == MIX_8GAME ? "8-Game" : ""; }

const char *mixBlurb(int mix) {
    return mix == MIX_HORSE ? "Hold'em, Omaha hi-lo, Razz, Stud, Stud hi-lo (Eight or better) - one orbit each."
         : mix == MIX_8GAME ? "2-7 Triple Draw, Hold'em (limit and no-limit), Omaha hi-lo, Razz, Stud, Stud hi-lo, PLO."
         : "";
}
