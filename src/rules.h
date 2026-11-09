// rules.h - the rules coach: explains each game's rules as a hand is played (no raylib dependency).
// Rules follow Wikipedia's articles on each game.
#pragma once
#include "game.h"

#define RULE_LEN 420

// concepts the coach explains in full the first time, briefly after that (per game)
enum { RC_BUTTON, RC_BLINDS, RC_ANTES, RC_DEAL, RC_BRINGIN, RC_COMPLETE, RC_PREFLOP, RC_STREET, RC_ORDER, RC_OPENPAIR, RC_BIGBET, RC_CAP,
       RC_ALLIN, RC_SHORTALLIN, RC_DRAWS, RC_PAT, RC_DREW1, RC_DREW3, RC_RESHUFFLE, RC_UNCALLED, RC_WALK, RC_RUNOUT, RC_LAST, RC_N };
typedef struct { bool seen[V_COUNT][RC_N]; } RulesMemory;

// The full rules of a game (sections start with a line in capitals). Returns the length.
int rulesSheet(int variant, int limit, char *buf, int n);
int rulesMixedSheet(int mix, char *buf, int n);
// One engine event explained in plain English. Returns false when the event needs no comment.
bool rulesNarrate(const Game *g, const GameEvent *e, RulesMemory *mem, char *out, int n);
// What player p may do right now, and why. Returns the number of lines.
int rulesYourTurn(const Game *g, int p, char out[][RULE_LEN], int max);
// Why each pot went where it did. Returns the number of lines.
int rulesShowdown(const Game *g, char out[][RULE_LEN], int max);
// How two hand values compare, in words: "Two Pair beats One Pair" / "the kicker decides: Ace beats Queen".
void rulesCompare(int variant, int winner, int loser, char *buf, int n);
void cardLong(int c, char *buf, int n);       // "2 of clubs", "King of spades"
