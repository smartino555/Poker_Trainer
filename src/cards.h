// cards.h - cards, RNG and hand evaluation (no raylib dependency)
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "variants.h"

// card = rank * 4 + suit; rank 0 = deuce .. 12 = ace; suit 0..3 = clubs, diamonds, hearts, spades
#define CARD(r, s) ((r) * 4 + (s))
static inline int cardRank(int c) { return c >> 2; }
static inline int cardSuit(int c) { return c & 3; }

enum { HC_HIGH, HC_PAIR, HC_TWOPAIR, HC_TRIPS, HC_STRAIGHT, HC_FLUSH, HC_FULLHOUSE, HC_QUADS, HC_STFLUSH };

typedef struct { uint64_t s; } Rng;
void rngSeed(Rng *r, uint64_t seed);
uint32_t rngNext(Rng *r);
int rngInt(Rng *r, int n);        // 0 .. n-1
float rngFloat(Rng *r);           // [0, 1)
void shuffle(int *a, int n, Rng *r);

// Hand values compare as plain ints: higher wins - for low games too (a better low gets a higher value).
int evalHand(const int *cards, int n);                  // best 5 of 5..7 cards (fewer: pairs and high cards only)
int evalShortDeck(const int *cards, int n);             // short deck: A-6-7-8-9 straight, flush beats full house
int evalOmaha(const int *hole, const int *board, int nb); // exactly 2 of 4 hole cards + 3 of the board
int evalOmahaN(const int *hole, int nh, const int *board, int nb);
int evalLowA5(const int *cards, int n);                 // ace-to-five low: best 5 of up to 7 (fewer: a partial hand)
int evalOmahaLow8(const int *hole, int nh, const int *board, int nb);   // 8-or-better low with 2 + 3, or 0
int evalLow27(const int *cards, int n);                 // deuce-to-seven low (5 cards)
int evalBadugi(const int *cards, int n);                // best badugi of 4 cards
bool lowQualifies8(int lowValue);                       // an ace-to-five low of 8 or better (no pair)
int evalPlayer(int variant, const int *hole, const int *board, int nb);   // main ranking, standard hole count
// every game: the main ranking of the hand; for hi-lo games *low gets the qualifying low (0 if none)
int evalCards(int variant, const int *hole, int nh, const int *board, int nb, int *low);
int showingValue(int variant, const int *up, int n);    // stud: the up cards' strength for who acts first

static inline int handCat(int v) { return (v >> 20) & 15; }
static inline int handRank1(int v) { return (v >> 16) & 15; }   // primary rank of the hand

extern const char RANKCH[14];
const char *rankName(int r);      // "Ace"
const char *rankPlural(int r);    // "Aces", "Sixes"
const char *catName(int cat);     // "Two Pair"
void handName(int v, char *buf, int n);   // "Two Pair, Kings and Nines" (high hands)
void lowName(int lowValue, char *buf, int n);   // "8-6-4-3-A" or "Pair of Sixes"
void handNameV(int variant, int v, char *buf, int n);   // the name of a value from evalCards / evalPlayer
void cardStr(int c, char *buf);           // "As" (buf >= 3)
