// cards.c - RNG and a bitmask hand evaluator
#include "cards.h"
#include <stdio.h>

const char RANKCH[14] = "23456789TJQKA";
static const char *RANK_NAME[13] = { "Deuce", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten", "Jack", "Queen", "King", "Ace" };
static const char *RANK_PLURAL[13] = { "Deuces", "Threes", "Fours", "Fives", "Sixes", "Sevens", "Eights", "Nines", "Tens", "Jacks", "Queens", "Kings", "Aces" };
static const char *CAT_NAME[9] = { "High Card", "Pair", "Two Pair", "Three of a Kind", "Straight", "Flush", "Full House", "Four of a Kind", "Straight Flush" };

void rngSeed(Rng *r, uint64_t seed) { r->s = seed ? seed : 0x9E3779B97F4A7C15ull; for (int i = 0; i < 4; i++) rngNext(r); }
uint32_t rngNext(Rng *r) {
    uint64_t x = r->s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    r->s = x;
    return (uint32_t)((x * 0x2545F4914F6CDD1Dull) >> 32);
}
int rngInt(Rng *r, int n) { return n <= 1 ? 0 : (int)(((uint64_t)rngNext(r) * (uint64_t)n) >> 32); }
float rngFloat(Rng *r) { return (rngNext(r) >> 8) * (1.0f / 16777216.0f); }
void shuffle(int *a, int n, Rng *r) {
    for (int i = n - 1; i > 0; i--) { int j = rngInt(r, i + 1), t = a[i]; a[i] = a[j]; a[j] = t; }
}

const char *rankName(int r) { return RANK_NAME[r]; }
const char *rankPlural(int r) { return RANK_PLURAL[r]; }
const char *catName(int cat) { return CAT_NAME[cat]; }
void cardStr(int c, char *buf) { buf[0] = RANKCH[cardRank(c)]; buf[1] = "cdhs"[cardSuit(c)]; buf[2] = 0; }

#define HV(cat, a, b, c, d, e) (((cat) << 20) | ((a) << 16) | ((b) << 12) | ((c) << 8) | ((d) << 4) | (e))

static int straightHigh(int m) {
    for (int hi = 12; hi >= 4; hi--) {
        int w = 0x1F << (hi - 4);
        if ((m & w) == w) return hi;
    }
    if ((m & 0x100F) == 0x100F) return 3;   // wheel, five high
    return -1;
}

// the k highest ranks of mask, skipping ranks in 'skip'
static void topRanks(int mask, int skip, int k, int *out) {
    int n = 0;
    for (int r = 12; r >= 0 && n < k; r--)
        if ((mask >> r & 1) && !(skip >> r & 1)) out[n++] = r;
    while (n < k) out[n++] = 0;
}

int evalHand(const int *c, int n) {
    int cnt[13] = { 0 }, sm[4] = { 0 }, all = 0;
    for (int i = 0; i < n; i++) {
        int r = cardRank(c[i]), s = cardSuit(c[i]);
        cnt[r]++; sm[s] |= 1 << r; all |= 1 << r;
    }
    int flushMask = 0;
    for (int s = 0; s < 4; s++)
        if (__builtin_popcount(sm[s]) >= 5) {
            int sf = straightHigh(sm[s]);
            if (sf >= 0) return HV(HC_STFLUSH, sf, 0, 0, 0, 0);
            flushMask = sm[s];
        }
    int quad = -1, trips[3], nt = 0, pairs[4], np = 0;
    for (int r = 12; r >= 0; r--) {
        if (cnt[r] == 4) quad = r;
        else if (cnt[r] == 3) trips[nt++] = r;
        else if (cnt[r] == 2) pairs[np++] = r;
    }
    int k[5];
    if (quad >= 0) { topRanks(all, 1 << quad, 1, k); return HV(HC_QUADS, quad, k[0], 0, 0, 0); }
    if (nt >= 1 && (nt >= 2 || np >= 1)) {
        int pr = np ? pairs[0] : -1;
        if (nt >= 2 && trips[1] > pr) pr = trips[1];
        return HV(HC_FULLHOUSE, trips[0], pr, 0, 0, 0);
    }
    if (flushMask) { topRanks(flushMask, 0, 5, k); return HV(HC_FLUSH, k[0], k[1], k[2], k[3], k[4]); }
    int st = straightHigh(all);
    if (st >= 0) return HV(HC_STRAIGHT, st, 0, 0, 0, 0);
    if (nt) { topRanks(all, 1 << trips[0], 2, k); return HV(HC_TRIPS, trips[0], k[0], k[1], 0, 0); }
    if (np >= 2) { topRanks(all, (1 << pairs[0]) | (1 << pairs[1]), 1, k); return HV(HC_TWOPAIR, pairs[0], pairs[1], k[0], 0, 0); }
    if (np == 1) { topRanks(all, 1 << pairs[0], 3, k); return HV(HC_PAIR, pairs[0], k[0], k[1], k[2], 0); }
    topRanks(all, 0, 5, k);
    return HV(HC_HIGH, k[0], k[1], k[2], k[3], k[4]);
}

int evalOmaha(const int *hole, const int *board, int nb) {
    int best = -1, h[5];
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++) {
            h[0] = hole[i]; h[1] = hole[j];
            for (int a = 0; a < nb; a++)
                for (int b = a + 1; b < nb; b++)
                    for (int d = b + 1; d < nb; d++) {
                        h[2] = board[a]; h[3] = board[b]; h[4] = board[d];
                        int v = evalHand(h, 5);
                        if (v > best) best = v;
                    }
        }
    return best;
}

int evalPlayer(int variant, const int *hole, const int *board, int nb) {
    if (variant == V_OMAHA) return evalOmaha(hole, board, nb);
    int c[7] = { hole[0], hole[1] };
    for (int i = 0; i < nb; i++) c[2 + i] = board[i];
    return evalHand(c, 2 + nb);
}

void handName(int v, char *buf, int n) {
    int cat = v >> 20, a = (v >> 16) & 15, b = (v >> 12) & 15;
    switch (cat) {
    case HC_HIGH:      snprintf(buf, n, "High Card, %s", RANK_NAME[a]); break;
    case HC_PAIR:      snprintf(buf, n, "Pair of %s", RANK_PLURAL[a]); break;
    case HC_TWOPAIR:   snprintf(buf, n, "Two Pair, %s and %s", RANK_PLURAL[a], RANK_PLURAL[b]); break;
    case HC_TRIPS:     snprintf(buf, n, "Three %s", RANK_PLURAL[a]); break;
    case HC_STRAIGHT:  snprintf(buf, n, "Straight, %s high", RANK_NAME[a]); break;
    case HC_FLUSH:     snprintf(buf, n, "Flush, %s high", RANK_NAME[a]); break;
    case HC_FULLHOUSE: snprintf(buf, n, "Full House, %s full of %s", RANK_PLURAL[a], RANK_PLURAL[b]); break;
    case HC_QUADS:     snprintf(buf, n, "Four %s", RANK_PLURAL[a]); break;
    default:           snprintf(buf, n, a == 12 ? "Royal Flush" : "Straight Flush, %s high", RANK_NAME[a]); break;
    }
}
