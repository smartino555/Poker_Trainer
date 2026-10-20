// strategy.h - the "coach": a solver-inspired baseline strategy with explanations.
// The Hard bot plays from the same advice, so what the coach teaches is what Hard does.
#pragma once
#include "game.h"
#include "equity.h"

enum { Q_BEST, Q_GOOD, Q_INACC, Q_MISTAKE, Q_BLUNDER };
#define NWHY 9

typedef struct {
    int act, amount;        // recommended action; amount = raise-to
    int alt;                // reasonable alternative (-1 none) ...
    float altFreq;          // ... and how often a balanced player picks it
    bool bluff;             // the aggressive option here is a bluff / semi-bluff
    int q[4];               // quality of each action type, indexed by A_FOLD..A_RAISE
    int lo, hi;             // sensible raise-to window for the aggressive option
    int aggAmount;          // suggested raise-to if you do raise (even when it isn't the pick)
    float equity, need, spr, top;
    int outs, boatOuts, nopp;      // outs = flush + straight outs
    bool preflop, hasEquity;
    bool openBet;           // the aggressive option is a bet (nobody has bet yet), not a raise
    char spot[96], hand[110], draws[96], head[120];
    char why[NWHY][280];
    int nwhy;
} Advice;

void advise(const Game *g, int p, Advice *a, int trials, Rng *rng);
void estimateRanges(const Game *g, int p, OppRange *out, int *n);   // live opponents of seat p
void describeMadeHand(const Game *g, int p, char *buf, int n);
const char *qualityName(int q);
float rfiFraction(const Game *g, int pos);

// "think it through" questions for quiz mode: facts about the spot plus the questions a good player asks
// (it never gives the answer). Returns the number of prompts written.
#define NTHINK 7
int thinkPrompts(const Game *g, int p, char out[NTHINK][220]);   // share of hands a seat opens from that position

// preflop charts for the range viewer (Hold'em)
typedef struct { const char *name, *desc, *raise, *call; } ChartDef;
int chartCount(void);
const ChartDef *chartGet(int i);
