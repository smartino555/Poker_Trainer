// equity.h - Monte Carlo equity against estimated opponent ranges
#pragma once
#include "range.h"

typedef struct {
    float top;            // preflop: hand is within the top fraction of starting hands (1 = any two cards)
    float floor;          // mostly excludes hands stronger than this (players who would have re-raised them); 0 = none
    int aggMask, callMask; // bit per street (ST_FLOP..ST_RIVER) where they bet/raised or called a bet
    int raiseMask;         // ... where they raised someone else's bet (a much stronger range)
    int contMask;          // ... where they would continue against a raise (what-if: "if I raise, what's left?")
    const Range *set;      // Hold'em only: an exact class range (e.g. a solved push range) instead of 'top'
} OppRange;

float equityCalc(int variant, const int *hole, const int *board, int nb,
                 const OppRange *opp, int nopp, int trials, Rng *rng);
bool connected(int variant, const int *hole, const int *board, int nb);  // pair using a hole card, or a draw
int handTier(int variant, const int *hole, const int *board, int nb);  // 3 monster, 2 strong / big draw, 1 weak pair / draw, 0 air
int countOuts(int variant, const int *hole, const int *board, int nb, int counts[3]); // outs to [0] flush, [1] straight, [2] full house+
int betterCombos(int variant, const int *hole, const int *board, int nb);  // two-card holdings that beat you right now (0 = the nuts)
