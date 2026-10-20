// range.c - hand classes, range strings and preflop strength tables
#include "range.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float hcTop[NCLS];
static float hcScore[NCLS];

// every starting hand, strongest first, for exact percentiles and range sampling
static int heCombo[1326];               // c1 | c2 << 8
static float heTopSorted[1326];         // ascending
#define OM_N 270725
static int *omCombo;                    // c1 | c2 << 8 | c3 << 16 | c4 << 24
static float *omScoreSorted;            // descending

int hcClass(int c1, int c2) {
    int a = cardRank(c1), b = cardRank(c2);
    int hi = a > b ? a : b, lo = a > b ? b : a;
    int rh = 12 - hi, rl = 12 - lo;
    if (hi == lo) return rh * 13 + rh;
    if (cardSuit(c1) == cardSuit(c2)) return rh * 13 + rl;
    return rl * 13 + rh;
}

void hcClassName(int cls, char *buf) {
    int r = cls / 13, c = cls % 13;
    if (r == c) { buf[0] = buf[1] = RANKCH[12 - r]; buf[2] = 0; return; }
    int hi = r < c ? 12 - r : 12 - c, lo = r < c ? 12 - c : 12 - r;
    buf[0] = RANKCH[hi]; buf[1] = RANKCH[lo]; buf[2] = r < c ? 's' : 'o'; buf[3] = 0;
}

int hcCombos(int cls) { int r = cls / 13, c = cls % 13; return r == c ? 6 : r < c ? 4 : 12; }

static int clsOf(int hi, int lo, int kind) {   // kind 0 pair, 1 suited, 2 offsuit
    int rh = 12 - hi, rl = 12 - lo;
    if (kind == 0) return rh * 13 + rh;
    return kind == 1 ? rh * 13 + rl : rl * 13 + rh;
}

static int rankOf(char ch) {
    for (int i = 0; i < 13; i++) if (RANKCH[i] == ch) return i;
    return -1;
}

static void addCls(Range *r, int hi, int lo, char suf) {
    if (hi == lo) { r->in[clsOf(hi, lo, 0)] = 1; return; }
    if (hi < lo) { int t = hi; hi = lo; lo = t; }
    if (suf != 'o') r->in[clsOf(hi, lo, 1)] = 1;
    if (suf != 's') r->in[clsOf(hi, lo, 2)] = 1;
}

bool rangeParse(Range *r, const char *spec) {
    memset(r, 0, sizeof *r);
    char tok[16];
    const char *p = spec;
    bool ok = true;
    while (*p) {
        while (*p == ',' || *p == ' ') p++;
        int n = 0;
        while (*p && *p != ',' && n < 15) tok[n++] = *p++;
        tok[n] = 0;
        if (!n) continue;
        int a = rankOf(tok[0]), b = rankOf(tok[1]);
        if (a < 0 || b < 0) { ok = false; continue; }
        char suf = 0;
        int k = 2;
        if (tok[k] == 's' || tok[k] == 'o') suf = tok[k++];
        if (a == b) {
            int lo = a, hi = a;
            if (tok[k] == '+') hi = 12;
            else if (tok[k] == '-') { int c = rankOf(tok[k + 1]); if (c >= 0) { lo = c < a ? c : a; hi = c < a ? a : c; } }
            for (int x = lo; x <= hi; x++) addCls(r, x, x, 0);
        } else {
            int hi = a > b ? a : b, lo = a > b ? b : a, from = lo, to = lo;
            if (tok[k] == '+') to = hi - 1;
            else if (tok[k] == '-') {
                int c = rankOf(tok[k + 2]);
                if (c >= 0) { from = c < lo ? c : lo; to = c < lo ? lo : c; }
            }
            for (int x = from; x <= to; x++) addCls(r, hi, x, suf);
        }
    }
    return ok;
}

float rangeFraction(const Range *r) {
    int c = 0;
    for (int i = 0; i < NCLS; i++) if (r->in[i]) c += hcCombos(i);
    return c / 1326.0f;
}

// ---------------------------------------------------------------- Omaha starting hands

float omahaScore(const int *hole) {
    int r[4], s[4];
    for (int i = 0; i < 4; i++) { r[i] = cardRank(hole[i]); s[i] = cardSuit(hole[i]); }
    float sc = 0;
    int cnt[13] = { 0 };
    for (int i = 0; i < 4; i++) { sc += (r[i] + 2) * 0.5f; cnt[r[i]]++; }
    for (int k = 0; k < 13; k++) {
        if (cnt[k] == 2) sc += 6 + k * 0.8f + (k == 12 ? 6 : k == 11 ? 2.5f : 0);
        else if (cnt[k] == 3) sc += k * 0.5f - 3;
        else if (cnt[k] == 4) sc -= 14;
    }
    for (int su = 0; su < 4; su++) {
        int n = 0, top = -1;
        for (int i = 0; i < 4; i++) if (s[i] == su) { n++; if (r[i] > top) top = r[i]; }
        float nutBonus = top == 12 ? 3.5f : top == 11 ? 1.5f : top >= 9 ? 0.6f : 0;
        if (n == 2) sc += 3.5f + nutBonus;
        else if (n == 3) sc += 1.5f + nutBonus * 0.5f;
    }
    int u[4], nu = 0;
    for (int k = 12; k >= 0; k--) if (cnt[k]) u[nu++] = k;
    static const float GAP[5] = { 0, 3.2f, 2.2f, 1.2f, 0.5f };
    bool linked[4] = { 0 };
    for (int i = 0; i < nu; i++)
        for (int j = i + 1; j < nu; j++) {
            int gap = u[i] - u[j];
            if (gap <= 4) { sc += GAP[gap] * (u[j] >= 7 ? 1.15f : 1.0f); linked[i] = linked[j] = true; }
            else if (u[i] == 12 && u[j] <= 3) { sc += 0.5f; linked[i] = linked[j] = true; }
        }
    for (int i = 0; i < nu; i++) {
        if (linked[i] || cnt[u[i]] > 1) continue;
        bool suited = false;
        for (int a = 0; a < 4; a++)
            if (r[a] == u[i])
                for (int b = 0; b < 4; b++) if (b != a && s[b] == s[a]) suited = true;
        if (!suited) sc -= 3;
    }
    return sc;
}

float omahaTop(const int *hole) {
    rangesInit();
    float v = omahaScore(hole);
    int lo = 0, hi = OM_N;          // number of hands scoring >= v
    while (lo < hi) { int m = (lo + hi) / 2; if (omScoreSorted[m] >= v) lo = m + 1; else hi = m; }
    return (float)lo / OM_N;
}

float handTop(int variant, const int *hole) {
    return variant == V_OMAHA ? omahaTop(hole) : hcTop[hcClass(hole[0], hole[1])];
}

static int topCount(int variant, float top) {
    if (variant == V_OMAHA) {
        int n = (int)(top * OM_N);
        return n < 1 ? 1 : n > OM_N ? OM_N : n;
    }
    int lo = 0, hi = 1326;          // combos with percentile <= top
    while (lo < hi) { int m = (lo + hi) / 2; if (heTopSorted[m] <= top + 1e-6f) lo = m + 1; else hi = m; }
    return lo < 1 ? 1 : lo;
}

void sampleRange(int variant, float top, const bool *used, Rng *rng, int *out) {
    rangesInit();
    int nc = variant == V_OMAHA ? 4 : 2, n = topCount(variant, top);
    for (int tries = 0; tries < 200; tries++) {
        int packed = variant == V_OMAHA ? omCombo[rngInt(rng, n)] : heCombo[rngInt(rng, n)];
        bool ok = true;
        for (int k = 0; k < nc; k++) { out[k] = packed >> (8 * k) & 0xFF; if (used[out[k]]) ok = false; }
        if (ok) return;
    }
    // every hand in a tiny range is blocked: fall back to any live cards
    for (int k = 0; k < nc; k++) {
        int c;
        do c = rngInt(rng, 52); while (used[c] || (k > 0 && c == out[0]) || (k > 1 && c == out[1]) || (k > 2 && c == out[2]));
        out[k] = c;
    }
}

void sampleSet(const Range *r, const bool *used, Rng *rng, int *out) {
    for (int tries = 0; tries < 4000; tries++) {
        int a = rngInt(rng, 52), b = rngInt(rng, 52);
        if (a == b || used[a] || used[b]) continue;
        out[0] = a; out[1] = b;
        if (r->in[hcClass(a, b)]) return;
    }
    do out[0] = rngInt(rng, 52); while (used[out[0]]);
    do out[1] = rngInt(rng, 52); while (used[out[1]] || out[1] == out[0]);
}

void handClassText(int variant, const int *hole, char *buf, int n) {
    if (variant == V_HOLDEM) { char c[4]; hcClassName(hcClass(hole[0], hole[1]), c); snprintf(buf, n, "%s", c); return; }
    int sc[4] = { 0 }, pairs = 0, cnt[13] = { 0 };
    for (int i = 0; i < 4; i++) { sc[cardSuit(hole[i])]++; cnt[cardRank(hole[i])]++; }
    int suitedPairs = 0;
    for (int i = 0; i < 4; i++) suitedPairs += sc[i] >= 2;
    for (int i = 0; i < 13; i++) pairs += cnt[i] >= 2;
    int rs[4];
    for (int i = 0; i < 4; i++) rs[i] = cardRank(hole[i]);
    for (int i = 0; i < 4; i++) for (int j = i + 1; j < 4; j++) if (rs[j] > rs[i]) { int t = rs[i]; rs[i] = rs[j]; rs[j] = t; }
    const char *shape = suitedPairs >= 2 ? "double-suited" : suitedPairs == 1 ? "single-suited" : "rainbow";
    snprintf(buf, n, "%c%c%c%c %s%s", RANKCH[rs[0]], RANKCH[rs[1]], RANKCH[rs[2]], RANKCH[rs[3]], shape, pairs ? ", paired" : "");
}

// ---------------------------------------------------------------- strength tables

static int cmpDesc(const void *a, const void *b) {
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? 1 : x > y ? -1 : 0;
}

static float eqVsRandom(int c1, int c2, int nopp, int trials, Rng *rng) {
    int deck[52], nd = 0;
    for (int c = 0; c < 52; c++) if (c != c1 && c != c2) deck[nd++] = c;
    float win = 0;
    for (int t = 0; t < trials; t++) {
        int need = 5 + 2 * nopp;
        for (int i = 0; i < need; i++) { int j = i + rngInt(rng, nd - i), x = deck[i]; deck[i] = deck[j]; deck[j] = x; }
        int me[7] = { c1, c2, deck[0], deck[1], deck[2], deck[3], deck[4] };
        int mv = evalHand(me, 7), best = 0, ties = 0;
        for (int o = 0; o < nopp; o++) {
            int h[7] = { deck[5 + 2 * o], deck[6 + 2 * o], deck[0], deck[1], deck[2], deck[3], deck[4] };
            int v = evalHand(h, 7);
            if (v > best) { best = v; ties = 0; }
            if (v == best) ties++;
        }
        if (mv > best) win += 1;
        else if (mv == best) win += 1.0f / (ties + 1);
    }
    return win / trials;
}

void rangesInit(void) {
    static bool done = false;
    if (done) return;
    done = true;
    Rng rng;
    rngSeed(&rng, 20261019);
    // Hold'em: blend heads-up equity with 4-way equity so suited / connected hands rank
    // closer to how they actually play, not just how they do all-in against one random hand.
    for (int cls = 0; cls < NCLS; cls++) {
        int r = cls / 13, c = cls % 13, hi, lo, c1, c2;
        if (r == c) { hi = lo = 12 - r; c1 = CARD(hi, 0); c2 = CARD(lo, 1); }
        else if (r < c) { hi = 12 - r; lo = 12 - c; c1 = CARD(hi, 0); c2 = CARD(lo, 0); }
        else { hi = 12 - c; lo = 12 - r; c1 = CARD(hi, 0); c2 = CARD(lo, 1); }
        float hu = eqVsRandom(c1, c2, 1, 4000, &rng);
        float m4 = eqVsRandom(c1, c2, 3, 2500, &rng);
        hcScore[cls] = hu + 1.6f * m4;
    }
    int order[NCLS];
    for (int i = 0; i < NCLS; i++) order[i] = i;
    for (int i = 1; i < NCLS; i++)
        for (int j = i; j > 0 && hcScore[order[j]] > hcScore[order[j - 1]]; j--) { int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t; }
    int cum = 0;
    for (int i = 0; i < NCLS; i++) { cum += hcCombos(order[i]); hcTop[order[i]] = cum / 1326.0f; }

    // Hold'em combos sorted by percentile
    int n = 0;
    for (int a = 0; a < 52; a++)
        for (int b = a + 1; b < 52; b++) heCombo[n++] = a | b << 8;
    for (int i = 1; i < 1326; i++)      // insertion sort by hcTop
        for (int j = i; j > 0 && hcTop[hcClass(heCombo[j] & 0xFF, heCombo[j] >> 8)] < hcTop[hcClass(heCombo[j - 1] & 0xFF, heCombo[j - 1] >> 8)]; j--) {
            int t = heCombo[j]; heCombo[j] = heCombo[j - 1]; heCombo[j - 1] = t;
        }
    for (int i = 0; i < 1326; i++) heTopSorted[i] = hcTop[hcClass(heCombo[i] & 0xFF, heCombo[i] >> 8)];

    // every Omaha hand sorted by score
    typedef struct { float s; int c; } SC;
    SC *all = malloc(sizeof(SC) * OM_N);
    omCombo = malloc(sizeof(int) * OM_N);
    omScoreSorted = malloc(sizeof(float) * OM_N);
    n = 0;
    for (int a = 0; a < 52; a++) for (int b = a + 1; b < 52; b++) for (int c = b + 1; c < 52; c++) for (int d = c + 1; d < 52; d++) {
        int h[4] = { a, b, c, d };
        all[n].s = omahaScore(h);
        all[n++].c = a | b << 8 | c << 16 | d << 24;
    }
    qsort(all, OM_N, sizeof(SC), cmpDesc);
    for (int i = 0; i < OM_N; i++) { omCombo[i] = all[i].c; omScoreSorted[i] = all[i].s; }
    free(all);
}
