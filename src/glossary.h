// glossary.h - poker terms used in the game, with plain-English definitions (no raylib dependency)
#pragma once
#include <stdbool.h>

enum { GC_BASICS, GC_POSITIONS, GC_PREFLOP, GC_POSTFLOP, GC_HANDS, GC_MATH, GC_THEORY, GC_PLAYERS, GC_GAMES, GC_APP, GC_N };

typedef struct {
    const char *term;
    int cat;
    const char *keys;     // '|'-separated lowercase phrases that mark the term in coach text ("" = never auto-detected)
    const char *def;
} GlossEntry;

int glossCount(void);
const GlossEntry *glossGet(int i);
const char *glossCatName(int cat);
int glossLookup(const char *term);                       // index by exact term (case-insensitive), -1 if none
bool glossMatches(int i, const char *query);             // search box: term, keys or definition contain the query
int glossFind(const char *text, int *out, int max);      // terms mentioned in a text, in order of first mention
