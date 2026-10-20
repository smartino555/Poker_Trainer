// cards.h - cards, RNG and hand evaluation (no raylib dependency)
#pragma once
#include <stdbool.h>
#include <stdint.h>

// card = rank * 4 + suit; rank 0 = deuce .. 12 = ace; suit 0..3 = clubs, diamonds, hearts, spades
#define CARD(r, s) ((r) * 4 + (s))
static inline int cardRank(int c) { return c >> 2; }
static inline int cardSuit(int c) { return c & 3; }

enum { V_HOLDEM, V_OMAHA };
enum { HC_HIGH, HC_PAIR, HC_TWOPAIR, HC_TRIPS, HC_STRAIGHT, HC_FLUSH, HC_FULLHOUSE, HC_QUADS, HC_STFLUSH };

typedef struct { uint64_t s; } Rng;
void rngSeed(Rng *r, uint64_t seed);
uint32_t rngNext(Rng *r);
int rngInt(Rng *r, int n);        // 0 .. n-1
float rngFloat(Rng *r);           // [0, 1)
void shuffle(int *a, int n, Rng *r);

// Hand values compare as plain ints: higher wins.
int evalHand(const int *cards, int n);                  // best 5 of 5..7 cards
int evalOmaha(const int *hole, const int *board, int nb); // exactly 2 of 4 hole cards + 3 of the board
int evalPlayer(int variant, const int *hole, const int *board, int nb);
static inline int handCat(int v) { return v >> 20; }
static inline int handRank1(int v) { return (v >> 16) & 15; }   // primary rank of the hand

extern const char RANKCH[14];
const char *rankName(int r);      // "Ace"
const char *rankPlural(int r);    // "Aces", "Sixes"
const char *catName(int cat);     // "Two Pair"
void handName(int v, char *buf, int n);   // "Two Pair, Kings and Nines"
void cardStr(int c, char *buf);           // "As" (buf >= 3)
