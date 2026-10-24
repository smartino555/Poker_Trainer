// game.h - table state and betting rules for every game: board games (Hold'em, Omaha ...),
// stud games and draw games (no raylib dependency)
#pragma once
#include "cards.h"

#define MAXP 6
#define MAXH 7            // most cards a player holds (stud)
#define LOGN 64
#define MAXEV 64

enum { LV_HUMAN, LV_EASY, LV_MEDIUM, LV_HARD };
// betting rounds. Board games: preflop .. river. Stud: 3rd .. 7th street are rounds 0..4.
// Draw games: round 0 is before the first draw, round k after the k-th draw.
enum { ST_PREFLOP, ST_FLOP, ST_TURN, ST_RIVER, ST_SHOWDOWN = 8 };
enum { PH_BET, PH_DRAW };                            // draw games: betting, or players taking turns to draw
enum { A_FOLD, A_CHECK, A_CALL, A_RAISE, A_DRAW };   // A_RAISE covers bets; amount = total street bet ("raise to")
                                                     // A_DRAW: amount = bit mask of the hole cards thrown away
enum { PF_NONE, PF_LIMP, PF_CALL, PF_OPEN, PF_3BET }; // what a player did preflop this hand
enum { POS_UTG, POS_HJ, POS_CO, POS_BTN, POS_SB, POS_BB };

// what happened, for the rules coach (src/rules.c turns these into explanations)
enum {
    EV_GAME,        // mixed games: switched game (a = variant, b = limit)
    EV_HAND,        // a new hand (a = hand number)
    EV_BLINDS,      // seat = small blind, a = big blind seat
    EV_ANTES,       // a = ante
    EV_DEAL,        // a = cards face down, b = cards face up
    EV_BRINGIN,     // seat must bring in: a = their up card, b = 1 if the suit broke a tie on rank
    EV_BRINGIN_ACT, // seat brought in (a = 0) or completed (a = 1); b = amount
    EV_STREET,      // a betting round starts: a = round, seat = first to act (-1 none), b = stud: best hand showing
    EV_OPENPAIR,    // seat shows an open pair on 4th street: the big bet is allowed
    EV_CAP,         // seat capped the betting
    EV_ALLIN,       // seat is all-in
    EV_SHORTALLIN,  // seat's all-in raise was less than a full raise: it doesn't reopen the betting
    EV_DRAWS,       // a draw starts (a = draw number, 0-based)
    EV_DRAW,        // seat threw away a cards and drew as many
    EV_RESHUFFLE,   // the deck ran out while seat was drawing: a discards were shuffled into a new deck
    EV_UNCALLED,    // a bet nobody matched went back to seat (a = amount)
    EV_WALK,        // seat won without a showdown (a = amount)
    EV_SHOWDOWN,    // the pot(s) were decided at showdown
    EV_COUNT
};
typedef struct { int type, seat, a, b; } GameEvent;

typedef struct { int type, amount; } Action;

typedef struct {
    int hands, vpip, pfr, bets, calls;   // counts for a simple HUD / opponent model
} Stats;

typedef struct {
    char name[20];
    int level;
    int stack;          // chips behind
    int bet;            // chips put in on this street
    int total;          // chips put in this hand (antes included)
    int hole[MAXH];
    int nhole;          // cards held now
    int upMask;         // stud: bit k set = hole[k] is face up
    int drew[3];        // draw games: cards drawn in each draw (-1 not yet)
    bool inHand;        // dealt in and not folded
    bool allin;
    bool acted;         // acted since the last full raise (draw phase: has drawn)
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
    int value;          // showdown hand value (the game's main ranking; high in hi-lo games)
    int low;            // hi-lo games: qualifying low at showdown (0 = none)
} Seat;

typedef struct {
    int variant, limit, mix, n;
    int levels[MAXP];
    int sb, bb, buyin;
    int human;            // -1 for bot-only tables
    uint64_t seed;
} GameConfig;

typedef struct {
    int amount, hiAmt, loAmt;   // the pot and how it was split (loAmt 0 = no low / not a hi-lo game)
    int hiMask, loMask;         // winners of each half (bit per seat)
    int eligible;               // players who could win it
} PotResult;

typedef struct {
    int variant, limit, n;
    int mix, mixIdx, mixHands;  // mixed games: rotation, current game, hands played in it
    Seat s[MAXP];
    int human;
    int sbAmt, bbAmt, buyin;    // blinds. bbAmt is also the small bet in fixed limit and the unit for "BB"
    int ante, bringIn, smallBet, bigBet;
    int button, sbSeat, bbSeat; // stud: no blinds (-1); the button only marks the dealer
    int deck[52], deckN, dpos;  // the stock is deck[dpos .. deckN-1]
    int disc[52], discOwner[52], discDraw[52], ndisc;
    int preset[52], npreset;    // tests: the next hand's deck starts with these cards, in dealing order
    int board[5], nboard;
    int ncards;           // cards each player holds now
    int street, phase, drawNo;
    int toAct;            // seat to act, -1 = betting round (or draw) closed
    int curBet;           // highest bet this street
    int minRaise;         // size of the last full raise
    int betUnit;          // fixed limit: the bet / raise size this round
    int nBets;            // fixed limit: bets and raises this round (cap: a bet + 3 raises)
    bool capOff;          // the round started heads-up: no cap
    bool openPair;        // stud, 4th street: an open pair allows the big bet
    bool bringInPending;  // stud, 3rd street: the bring-in hasn't been posted yet
    int bringInSeat;
    int lastRaiser;       // last aggressor this street (-1 none)
    int pfRaiser;         // last preflop aggressor
    int nRaises;          // voluntary raises this street
    int opener;           // first preflop raiser
    int nActs;            // actions this hand (decision ids)
    int handNo;
    bool handOver, showdown, pfCounted;
    int npots, potAmt[MAXP];
    PotResult pots[MAXP];
    char winText[200];
    char log[LOGN][110];
    int nlog;
    GameEvent ev[MAXEV];
    int evSeq;            // events ever recorded; event i is ev[i % MAXEV]
    Rng rng;
} Game;

typedef struct {
    int toCall;
    bool canCheck, canRaise;
    int minTo, maxTo;     // legal raise-to range (maxTo = all-in or pot limit). Fixed limit: one size,
                          // or small / big bet when an open pair shows on 4th street
    bool bringIn;         // stud: calling = posting the bring-in, raising = completing
    bool draw;            // draw phase: choose cards to throw away
} Legal;

void gameSetup(Game *g, const GameConfig *cfg);
void gameNewHand(Game *g);
void gameLegal(const Game *g, Legal *l);
void gameAct(Game *g, Action a);
void gameAdvance(Game *g);              // deal the next street / start the draw / run the showdown when toAct == -1
int gamePot(const Game *g);             // every chip in the middle, current bets included
int gameCallPot(const Game *g, int p);  // the pot p plays for if they call (their call included; an all-in
                                        // for less can't win the part of a bigger bet it doesn't cover)
int gameLive(const Game *g);            // players still holding cards
int gameEffStack(const Game *g, int p); // effective stack (chips incl. current bet) vs the live players
int posIndex(const Game *g, int p);     // POS_* (6-max naming; short tables drop the early seats)
const char *posName(const Game *g, int p);
bool inPosition(const Game *g, int p);  // acts last after the flop among the live players
int holeCount(const Game *g);           // cards each player holds now
int gameUpCards(const Game *g, int p, int *out);    // stud: p's face-up cards
int gameBestShowing(const Game *g);     // stud: seat whose up cards act first (-1 none)
int gameSeatValue(const Game *g, int p, int *low);  // p's hand right now under this game's ranking
bool gameIsFixed(const Game *g);
int gameCapLeft(const Game *g);         // fixed limit: raises left before the cap (-1 no cap)
const char *streetName(const Game *g, int street);
void gameLog(Game *g, const char *fmt, ...);
void fmtChips(int v, char *buf, int n);
