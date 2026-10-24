// basic.h - equity and simple advice for every game (stud, draw, hi-lo, short deck ...).
// The Hold'em / Omaha coach in strategy.c goes much deeper; this one gives sound basics:
// your share of the pot against the hands opponents could hold, the price, and what to draw.
#pragma once
#include "game.h"

#define NTIPS 7

typedef struct {
    int act, amount;        // recommended action (A_DRAW: amount = mask of cards to throw away)
    int alt;                // reasonable alternative (-1 none)
    float equity;           // expected share of the pot
    float need;             // equity a call needs (0 when free)
    int nopp;
    bool draw;              // a draw decision
    char head[96];          // "Call $10", "Draw 2 (throw away K, Q)"
    char hand[120];         // what you hold under this game's ranking
    char tips[NTIPS][260];
    int ntips;
} Tips;

// Expected share of the pot for p, simulating the rest of the hand. discard >= 0: p throws away
// these cards now (draw decisions). weighted: opponents who bet / raised (or stood pat) hold stronger hands.
float basicEquity(const Game *g, int p, int discard, bool weighted, int trials, Rng *rng);
// Which cards to throw away (bit mask over hole[0..n-1]); drawsLeft counts this draw.
int basicDrawChoice(int variant, const int *hole, int n, int drawsLeft);
void adviseBasic(const Game *g, int p, Tips *t, int trials, Rng *rng);
Action basicBot(const Game *g, int p, int level, Rng *rng);
void describeHandV(const Game *g, int p, char *buf, int n);   // "8-6-5-3-2 low", "Pair of Kings (one showing)"
int drawsLeft(const Game *g, int p);   // draws p still makes this hand (the current one included)
