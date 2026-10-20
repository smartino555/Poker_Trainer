// range.h - starting-hand classes, preflop strength and hand ranges
#pragma once
#include "cards.h"

// Hold'em: 169 classes laid out like a range chart. Row/col 0 = ace.
// Pairs on the diagonal, suited above it (row < col), offsuit below.
#define NCLS 169
int hcClass(int c1, int c2);
void hcClassName(int cls, char *buf);    // "AKs", "T9o", "77" (buf >= 4)
int hcCombos(int cls);                   // 6 pairs, 4 suited, 12 offsuit

typedef struct { unsigned char in[NCLS]; } Range;
bool rangeParse(Range *r, const char *spec);   // "22+,A2s+,KTo+,T9s"
float rangeFraction(const Range *r);           // share of all 1326 combos

void rangesInit(void);                 // builds the strength tables (call once)
extern float hcTop[NCLS];              // "top X" fraction of hands at least this strong (AA ~ 0.0045)
float omahaScore(const int *hole);     // heuristic PLO starting-hand score
float omahaTop(const int *hole);       // "top X" fraction for a 4-card hand
float handTop(int variant, const int *hole);
// draws a starting hand from the top fraction of hands, avoiding cards marked in 'used'
void sampleRange(int variant, float top, const bool *used, Rng *rng, int *out);
// draws a Hold'em hand from an explicit class range (falls back to any hand if the range is empty)
void sampleSet(const Range *r, const bool *used, Rng *rng, int *out);
void handClassText(int variant, const int *hole, char *buf, int n);  // "AKo" or "As Kd Qs Jd (double-suited)"
