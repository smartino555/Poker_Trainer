// cards.c - RNG and bitmask hand evaluators (high, short deck, Omaha, ace-to-five, deuce-to-seven, badugi)
#include "cards.h"
#include <stdio.h>
#include <string.h>

const char RANKCH[14] = "23456789TJQKA";
static const char LOWCH[14] = "A23456789TJQK";     // ace-to-five order: the ace is the lowest card
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
#define LOWTOP (1 << 24)     // low values are LOWTOP - key, so a better (smaller) low compares higher

enum { EF_NOWHEEL = 1, EF_SHORT = 2 };

static int straightHigh(int m, int flags) {
    for (int hi = 12; hi >= 4; hi--) {
        int w = 0x1F << (hi - 4);
        if ((m & w) == w) return hi;
    }
    if (!(flags & EF_NOWHEEL) && (m & 0x100F) == 0x100F) return 3;   // wheel, five high
    if ((flags & EF_SHORT) && (m & 0x10F0) == 0x10F0) return 7;      // short deck: A-6-7-8-9, nine high
    return -1;
}

// the k highest ranks of mask, skipping ranks in 'skip'
static void topRanks(int mask, int skip, int k, int *out) {
    int n = 0;
    for (int r = 12; r >= 0 && n < k; r--)
        if ((mask >> r & 1) && !(skip >> r & 1)) out[n++] = r;
    while (n < k) out[n++] = 0;
}

static int evalCore(const int *c, int n, int flags) {
    int cnt[13] = { 0 }, sm[4] = { 0 }, all = 0;
    for (int i = 0; i < n; i++) {
        int r = cardRank(c[i]), s = cardSuit(c[i]);
        cnt[r]++; sm[s] |= 1 << r; all |= 1 << r;
    }
    int flushMask = 0;
    for (int s = 0; s < 4; s++)
        if (__builtin_popcount(sm[s]) >= 5) {
            int sf = straightHigh(sm[s], flags);
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
    if (flushMask && (flags & EF_SHORT)) { topRanks(flushMask, 0, 5, k); return HV(HC_FLUSH, k[0], k[1], k[2], k[3], k[4]); }
    if (nt >= 1 && (nt >= 2 || np >= 1)) {
        int pr = np ? pairs[0] : -1;
        if (nt >= 2 && trips[1] > pr) pr = trips[1];
        return HV(HC_FULLHOUSE, trips[0], pr, 0, 0, 0);
    }
    if (flushMask) { topRanks(flushMask, 0, 5, k); return HV(HC_FLUSH, k[0], k[1], k[2], k[3], k[4]); }
    int st = straightHigh(all, flags);
    if (st >= 0) return HV(HC_STRAIGHT, st, 0, 0, 0, 0);
    if (nt) { topRanks(all, 1 << trips[0], 2, k); return HV(HC_TRIPS, trips[0], k[0], k[1], 0, 0); }
    if (np >= 2) { topRanks(all, (1 << pairs[0]) | (1 << pairs[1]), 1, k); return HV(HC_TWOPAIR, pairs[0], pairs[1], k[0], 0, 0); }
    if (np == 1) { topRanks(all, 1 << pairs[0], 3, k); return HV(HC_PAIR, pairs[0], k[0], k[1], k[2], 0); }
    topRanks(all, 0, 5, k);
    return HV(HC_HIGH, k[0], k[1], k[2], k[3], k[4]);
}

int evalHand(const int *c, int n) { return evalCore(c, n, 0); }

// The category in bits 20-23 stays the real one (for names); bits 24+ give the short-deck order,
// where a flush outranks a full house.
int evalShortDeck(const int *c, int n) {
    int v = evalCore(c, n, EF_SHORT), cat = handCat(v);
    int ord = cat == HC_FLUSH ? HC_FULLHOUSE : cat == HC_FULLHOUSE ? HC_FLUSH : cat;
    return ord << 24 | v;
}

int evalOmahaN(const int *hole, int nh, const int *board, int nb) {
    int best = -1, h[5];
    for (int i = 0; i < nh; i++)
        for (int j = i + 1; j < nh; j++) {
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

int evalOmaha(const int *hole, const int *board, int nb) { return evalOmahaN(hole, 4, board, nb); }

// ---------------------------------------------------------------- lowball

static int lowRank(int c) { return (cardRank(c) + 1) % 13; }    // A = 0, 2 = 1 .. K = 12

// key of up to 5 ace-low ranks: pairing first (none < pair < two pair < trips < full house < quads),
// then the ranks of the groups from the biggest group down, highest first. A smaller key is a better low.
static int lowKey(const int *lr, int n) {
    int cnt[13] = { 0 }, maxc = 0, pairs = 0;
    for (int i = 0; i < n; i++) cnt[lr[i]]++;
    for (int r = 0; r < 13; r++) { if (cnt[r] > maxc) maxc = cnt[r]; if (cnt[r] == 2) pairs++; }
    int cat = maxc == 4 ? 5 : maxc == 3 ? (pairs ? 4 : 3) : pairs == 2 ? 2 : pairs == 1 ? 1 : 0;
    int key = cat << 20, sh = 16;
    for (int c = 4; c >= 1; c--)
        for (int r = 12; r >= 0; r--)
            if (cnt[r] == c) { key |= r << sh; sh -= 4; }
    return key;
}

static int maskKey(int m) {             // five distinct ace-low ranks (bit set) -> no-pair key
    int key = 0, sh = 16;
    for (int r = 12; r >= 0; r--) if (m >> r & 1) { key |= r << sh; sh -= 4; }
    return key;
}

int evalLowA5(const int *c, int n) {
    int lr[7], m = 0;
    if (n > 7) n = 7;
    for (int i = 0; i < n; i++) { lr[i] = lowRank(c[i]); m |= 1 << lr[i]; }
    if (n <= 5) return LOWTOP - lowKey(lr, n);
    if (__builtin_popcount(m) >= 5) {    // five different ranks: the five lowest are the best low
        int low = 0, k = 0;
        for (int r = 0; r < 13 && k < 5; r++) if (m >> r & 1) { low |= 1 << r; k++; }
        return LOWTOP - maskKey(low);
    }
    int best = 1 << 30, sub[5];          // pairs can't be avoided: try every five cards
    for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) for (int d = b + 1; d < n; d++)
    for (int e = d + 1; e < n; e++) for (int f = e + 1; f < n; f++) {
        sub[0] = lr[a]; sub[1] = lr[b]; sub[2] = lr[d]; sub[3] = lr[e]; sub[4] = lr[f];
        int k = lowKey(sub, 5);
        if (k < best) best = k;
    }
    return LOWTOP - best;
}

bool lowQualifies8(int v) {
    if (v <= 0) return false;
    int key = LOWTOP - v;
    return (key >> 20) == 0 && ((key >> 16) & 15) <= 7;      // no pair, eight or lower
}

int evalOmahaLow8(const int *hole, int nh, const int *board, int nb) {
    int bm = 0;
    for (int i = 0; i < nb; i++) { int r = lowRank(board[i]); if (r <= 7) bm |= 1 << r; }
    if (__builtin_popcount(bm) < 3) return 0;
    int best = 0;
    for (int i = 0; i < nh; i++)
        for (int j = i + 1; j < nh; j++) {
            int a = lowRank(hole[i]), b = lowRank(hole[j]);
            if (a > 7 || b > 7 || a == b) continue;
            int pm = 1 << a | 1 << b, avail = bm & ~pm, t = 0, k = 0;
            for (int r = 0; r <= 7 && k < 3; r++) if (avail >> r & 1) { t |= 1 << r; k++; }   // the three lowest board cards
            if (k < 3) continue;
            int m = pm | t;
            if (!best || m < best) best = m;     // with five ranks each, a smaller mask is a better low
        }
    return best ? LOWTOP - maskKey(best) : 0;
}

int evalLow27(const int *c, int n) { return LOWTOP - evalCore(c, n, EF_NOWHEEL); }

int evalBadugi(const int *c, int n) {
    if (n > 4) n = 4;
    int best = 0;
    for (int m = 1; m < 1 << n; m++) {
        int rm = 0, su = 0, size = 0, ok = 1;
        for (int i = 0; i < n; i++) {
            if (!(m >> i & 1)) continue;
            int r = lowRank(c[i]), s = cardSuit(c[i]);
            if ((rm >> r & 1) || (su >> s & 1)) { ok = 0; break; }
            rm |= 1 << r; su |= 1 << s; size++;
        }
        if (!ok) continue;
        int key = (4 - size) << 16, sh = 12;
        for (int r = 12; r >= 0; r--) if (rm >> r & 1) { key |= r << sh; sh -= 4; }
        int v = (1 << 20) - key;
        if (v > best) best = v;
    }
    return best;
}

// ---------------------------------------------------------------- every game

int evalCards(int variant, const int *hole, int nh, const int *board, int nb, int *low) {
    int lo = 0, v, c[12];
    switch (variant) {
    case V_OMAHA: case V_OMAHA5: v = evalOmahaN(hole, nh, board, nb); break;
    case V_OMAHA8: v = evalOmahaN(hole, nh, board, nb); lo = evalOmahaLow8(hole, nh, board, nb); break;
    case V_STUD: case V_DRAW5: v = evalHand(hole, nh); break;
    case V_STUD8: v = evalHand(hole, nh); lo = evalLowA5(hole, nh); if (!lowQualifies8(lo)) lo = 0; break;
    case V_RAZZ: v = evalLowA5(hole, nh); break;
    case V_TD27: case V_SD27: v = evalLow27(hole, nh); break;
    case V_BADUGI: v = evalBadugi(hole, nh); break;
    default:                                     // Hold'em and short deck: any 5 of hole + board
        memcpy(c, hole, sizeof(int) * nh);
        memcpy(c + nh, board, sizeof(int) * nb);
        v = variant == V_SHORTDECK ? evalShortDeck(c, nh + nb) : evalHand(c, nh + nb);
        break;
    }
    if (low) *low = lo;
    return v;
}

int evalPlayer(int variant, const int *hole, const int *board, int nb) {
    if (variant == V_OMAHA) return evalOmaha(hole, board, nb);
    if (variant == V_HOLDEM) {
        int c[7] = { hole[0], hole[1] };
        for (int i = 0; i < nb; i++) c[2 + i] = board[i];
        return evalHand(c, 2 + nb);
    }
    return evalCards(variant, hole, varInfo(variant)->nhole, board, nb, NULL);
}

int showingValue(int variant, const int *up, int n) {
    if (n <= 0) return 0;
    return variant == V_RAZZ ? evalLowA5(up, n) : evalHand(up, n);
}

// ---------------------------------------------------------------- names

void handName(int v, char *buf, int n) {
    int cat = (v >> 20) & 15, a = (v >> 16) & 15, b = (v >> 12) & 15;
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

static int hiOf(int lr) { return (lr + 12) % 13; }    // ace-low rank -> normal rank

void lowName(int v, char *buf, int n) {
    if (v <= 0) { snprintf(buf, n, "no low"); return; }
    int key = LOWTOP - v, cat = key >> 20;
    int r[5];
    for (int i = 0; i < 5; i++) r[i] = key >> (16 - 4 * i) & 15;
    switch (cat) {
    case 0: snprintf(buf, n, "%c-%c-%c-%c-%c low", LOWCH[r[0]], LOWCH[r[1]], LOWCH[r[2]], LOWCH[r[3]], LOWCH[r[4]]); break;
    case 1: snprintf(buf, n, "Pair of %s", RANK_PLURAL[hiOf(r[0])]); break;
    case 2: snprintf(buf, n, "Two Pair, %s and %s", RANK_PLURAL[hiOf(r[0])], RANK_PLURAL[hiOf(r[1])]); break;
    case 3: snprintf(buf, n, "Three %s", RANK_PLURAL[hiOf(r[0])]); break;
    case 4: snprintf(buf, n, "Full House, %s full of %s", RANK_PLURAL[hiOf(r[0])], RANK_PLURAL[hiOf(r[1])]); break;
    default: snprintf(buf, n, "Four %s", RANK_PLURAL[hiOf(r[0])]); break;
    }
}

void handNameV(int variant, int v, char *buf, int n) {
    switch (varInfo(variant)->rank) {
    case RK_LOW_A5: lowName(v, buf, n); return;
    case RK_LOW_27: {
        int hv = LOWTOP - v;
        if (handCat(hv) != HC_HIGH) { handName(hv, buf, n); return; }
        int r[5];
        for (int i = 0; i < 5; i++) r[i] = hv >> (16 - 4 * i) & 15;
        snprintf(buf, n, "%c-%c-%c-%c-%c low", RANKCH[r[0]], RANKCH[r[1]], RANKCH[r[2]], RANKCH[r[3]], RANKCH[r[4]]);
        return;
    }
    case RK_BADUGI: {
        int key = (1 << 20) - v, size = 4 - (key >> 16), len = 0;
        len = snprintf(buf, n, size == 4 ? "Badugi " : "%d-card ", size);
        for (int i = 0; i < size && len < n; i++) len += snprintf(buf + len, n - len, "%s%c", i ? "-" : "", LOWCH[key >> (12 - 4 * i) & 15]);
        return;
    }
    default: handName(v, buf, n); return;
    }
}
