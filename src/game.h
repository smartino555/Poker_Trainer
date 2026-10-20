// game.h - table state and betting rules for Hold'em and Omaha (no raylib dependency)
#pragma once
#include "cards.h"

#define MAXP 6
#define LOGN 64

enum { BL_NOLIMIT, BL_POTLIMIT };
enum { LV_HUMAN, LV_EASY, LV_MEDIUM, LV_HARD };
enum { ST_PREFLOP, ST_FLOP, ST_TURN, ST_RIVER, ST_SHOWDOWN };
enum { A_FOLD, A_CHECK, A_CALL, A_RAISE };           // A_RAISE covers bets; amount = total street bet ("raise to")
enum { PF_NONE, PF_LIMP, PF_CALL, PF_OPEN, PF_3BET }; // what a player did preflop this hand
enum { POS_UTG, POS_HJ, POS_CO, POS_BTN, POS_SB, POS_BB };

typedef struct { int type, amount; } Action;

typedef struct {
    int hands, vpip, pfr, bets, calls;   // counts for a simple HUD / opponent model
} Stats;

typedef struct {
    char name[20];
    int level;
    int stack;          // chips behind
    int bet;            // chips put in on this street
    int total;          // chips put in this hand
    int hole[4];
    bool inHand;        // dealt in and not folded
    bool allin;
    bool acted;         // acted since the last full raise
    int faced;          // street bet level when this seat last acted
    bool shown;         // cards face up
    int lastAct;            // for display
    bool lastBet;           // last A_RAISE opened the betting (a bet, not a raise)
    int pf;             // PF_* this hand
    int aggMask, callMask;  // bit s set: bet/raised (called) on street s
    int raiseMask;          // bit s set: raised over someone's bet after the flop
    bool vpipThis, pfrThis;
    Stats st;
    int buyins;         // number of buy-ins taken
    int won;            // chips won from the pot(s) this hand
    int value;          // showdown hand value
} Seat;

typedef struct {
    int variant, limit, n;
    int levels[MAXP];
    int sb, bb, buyin;
    int human;            // -1 for bot-only tables
    uint64_t seed;
} GameConfig;

typedef struct {
    int variant, limit, n;
    Seat s[MAXP];
    int human;
    int sbAmt, bbAmt, buyin;
    int button, sbSeat, bbSeat;
    int deck[52], dpos;
    int board[5], nboard;
    int street;
    int toAct;            // seat to act, -1 = betting round closed
    int curBet;           // highest bet this street
    int minRaise;         // size of the last full raise
    int lastRaiser;       // last aggressor this street (-1 none)
    int pfRaiser;         // last preflop aggressor
    int nRaises;          // voluntary raises this street
    int opener;           // first preflop raiser
    int nActs;            // actions this hand (decision ids)
    int handNo;
    bool handOver, showdown, pfCounted;
    int npots, potAmt[MAXP];
    char winText[160];
    char log[LOGN][110];
    int nlog;
    Rng rng;
} Game;

typedef struct {
    int toCall;
    bool canCheck, canRaise;
    int minTo, maxTo;     // legal raise-to range (maxTo = all-in or pot limit)
} Legal;

void gameSetup(Game *g, const GameConfig *cfg);
void gameNewHand(Game *g);
void gameLegal(const Game *g, Legal *l);
void gameAct(Game *g, Action a);
void gameAdvance(Game *g);              // deal the next street / run the showdown when toAct == -1
int gamePot(const Game *g);             // every chip in the middle, current bets included
int gameCallPot(const Game *g, int p);  // the pot p plays for if they call (their call included; an all-in
                                        // for less can't win the part of a bigger bet it doesn't cover)
int gameLive(const Game *g);            // players still holding cards
int gameEffStack(const Game *g, int p); // effective stack (chips incl. current bet) vs the live players
int posIndex(const Game *g, int p);     // POS_* (6-max naming; short tables drop the early seats)
const char *posName(const Game *g, int p);
bool inPosition(const Game *g, int p);  // acts last after the flop among the live players
int holeCount(const Game *g);
void gameLog(Game *g, const char *fmt, ...);
void fmtChips(int v, char *buf, int n);
