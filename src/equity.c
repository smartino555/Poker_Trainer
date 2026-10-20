// equity.c - range-weighted Monte Carlo equity, made-hand / draw checks and outs
#include "equity.h"
#include "game.h"
#include "range.h"
#include <string.h>

static int rankMask(const int *c, int n) { int m = 0; for (int i = 0; i < n; i++) m |= 1 << cardRank(c[i]); return m; }

static bool straightDraw(int holeMask, int boardMask, bool omaha) {
    for (int lo = -1; lo <= 8; lo++) {
        int w = lo < 0 ? 0x100F : 0x1F << lo;       // A2345 window, then 23456 .. TJQKA
        int h = __builtin_popcount(holeMask & w), b = __builtin_popcount(boardMask & w);
        int all = __builtin_popcount((holeMask | boardMask) & w);
        if (all < 4 || b >= 4) continue;
        if (omaha ? (h >= 2 && b >= 2) : h >= 1) return true;
    }
    return false;
}

bool connected(int variant, const int *hole, const int *board, int nb) {
    bool om = variant == V_OMAHA;
    int nh = om ? 4 : 2;
    int hm = rankMask(hole, nh), bm = rankMask(board, nb);
    if (hm & bm) return true;
    int topBoard = 0;
    for (int i = 0; i < nb; i++) if (cardRank(board[i]) > topBoard) topBoard = cardRank(board[i]);
    for (int i = 0; i < nh; i++)
        for (int j = i + 1; j < nh; j++)
            if (cardRank(hole[i]) == cardRank(hole[j]) && (!om || cardRank(hole[i]) > topBoard)) return true;
    if (nb >= 5) return false;                   // no draws left on the river
    for (int s = 0; s < 4; s++) {
        int h = 0, b = 0;
        for (int i = 0; i < nh; i++) h += cardSuit(hole[i]) == s;
        for (int i = 0; i < nb; i++) b += cardSuit(board[i]) == s;
        if (om ? (h >= 2 && b >= 2) : (h >= 1 && h + b >= 4)) return true;
    }
    return straightDraw(hm, bm, om);
}

int countOuts(int variant, const int *hole, const int *board, int nb, int counts[3]) {
    counts[0] = counts[1] = counts[2] = 0;
    if (nb < 3 || nb > 4) return 0;
    int nh = variant == V_OMAHA ? 4 : 2;
    int cur = handCat(evalPlayer(variant, hole, board, nb));
    if (cur >= HC_FULLHOUSE) return 0;
    bool used[52] = { 0 };
    for (int i = 0; i < nh; i++) used[hole[i]] = true;
    for (int i = 0; i < nb; i++) used[board[i]] = true;
    int b[5], outs = 0;
    for (int i = 0; i < nb; i++) b[i] = board[i];
    for (int c = 0; c < 52; c++) {
        if (used[c]) continue;
        b[nb] = c;
        int cat = handCat(evalPlayer(variant, hole, b, nb + 1));
        if (cat <= cur || cat < HC_STRAIGHT) continue;
        outs++;
        if (cat >= HC_FULLHOUSE && cat != HC_STFLUSH) counts[2]++;
        else if (cat == HC_FLUSH || cat == HC_STFLUSH) counts[0]++;
        else counts[1]++;
    }
    return outs;
}

static int evalTwo(int variant, int c1, int c2, const int *board, int nb) {
    if (variant == V_HOLDEM) {
        int c[7] = { c1, c2 };
        for (int i = 0; i < nb; i++) c[2 + i] = board[i];
        return evalHand(c, 2 + nb);
    }
    int best = -1, h[5] = { c1, c2 };
    for (int a = 0; a < nb; a++)
        for (int b = a + 1; b < nb; b++)
            for (int d = b + 1; d < nb; d++) {
                h[2] = board[a]; h[3] = board[b]; h[4] = board[d];
                int v = evalHand(h, 5);
                if (v > best) best = v;
            }
    return best;
}

int betterCombos(int variant, const int *hole, const int *board, int nb) {
    int nh = variant == V_OMAHA ? 4 : 2;
    int me = evalPlayer(variant, hole, board, nb);
    bool used[52] = { 0 };
    for (int i = 0; i < nh; i++) used[hole[i]] = true;
    for (int i = 0; i < nb; i++) used[board[i]] = true;
    int n = 0;
    for (int a = 0; a < 52; a++) {
        if (used[a]) continue;
        for (int b = a + 1; b < 52; b++)
            if (!used[b] && evalTwo(variant, a, b, board, nb) > me) n++;
    }
    return n;
}

static bool flushDraw(const int *h, int nh, const int *board, int nb, bool omaha) {
    for (int s = 0; s < 4; s++) {
        int hs = 0, bs = 0;
        for (int i = 0; i < nh; i++) hs += cardSuit(h[i]) == s;
        for (int i = 0; i < nb; i++) bs += cardSuit(board[i]) == s;
        if (omaha ? (hs >= 2 && bs == 2) : (hs >= 1 && hs + bs == 4)) return true;
    }
    return false;
}

// How an opponent's holding fits a betting line, independent of exact equity.
int handTier(int variant, const int *h, const int *board, int nb) {
    bool om = variant == V_OMAHA;
    int nh = om ? 4 : 2;
    int v = evalPlayer(variant, h, board, nb), cat = handCat(v);
    int bv = evalHand(board, nb), bcat = handCat(bv);
    int topBoard = 0;
    for (int i = 0; i < nb; i++) if (cardRank(board[i]) > topBoard) topBoard = cardRank(board[i]);
    bool fd = nb < 5 && flushDraw(h, nh, board, nb, om);
    bool sd = nb < 5 && straightDraw(rankMask(h, nh), rankMask(board, nb), om);
    if (cat >= HC_STRAIGHT && (om || v > bv)) return 3;
    if (cat == HC_TRIPS || cat == HC_FULLHOUSE || cat == HC_QUADS) {
        if (om || bcat < cat) return 3;
    }
    if (cat == HC_TWOPAIR && bcat < HC_PAIR) return 3;                 // two pair made with hole cards
    bool topPair = false, overPair = false;
    if (cat >= HC_PAIR) {
        int pr = handRank1(v);
        if (cat == HC_TWOPAIR && bcat == HC_PAIR) pr = handRank1(bv) == pr ? (v >> 12 & 15) : pr;  // our own pair
        for (int i = 0; i < nh; i++)
            for (int j = i + 1; j < nh; j++)
                if (cardRank(h[i]) == cardRank(h[j]) && cardRank(h[i]) > topBoard) overPair = true;
        topPair = pr >= topBoard && (cat > bcat || overPair);
    }
    if (overPair || topPair) return fd || sd ? 3 : 2;
    if (fd && sd) return 3;
    if (fd || sd) return 2;
    if (cat > bcat) return 1;                                           // some pair of our own
    return 0;
}

// postflop + "would have re-raised" filters on a hand already drawn from the preflop range
static bool accept(int variant, const int *h, const int *board, int nb, const OppRange *o, Rng *rng) {
    static const float KEEP_BET[4] = { 0.22f, 0.5f, 1.0f, 1.0f };
    static const float KEEP_RAISE[4] = { 0.06f, 0.15f, 0.45f, 1.0f };
    static const float KEEP_CALL[4] = { 0.25f, 0.8f, 1.0f, 1.0f };
    static const float KEEP_CONT[4] = { 0.03f, 0.25f, 0.8f, 1.0f };      // still in after facing a raise
    if (o->floor > 0 && handTop(variant, h) < o->floor && rngFloat(rng) < 0.7f) return false;
    for (int st = ST_FLOP; st <= ST_RIVER; st++) {
        int len = st == ST_FLOP ? 3 : st == ST_TURN ? 4 : 5;
        if (len > nb) break;
        int bit = 1 << st;
        if (!((o->aggMask | o->callMask | o->contMask) & bit)) continue;
        int k = handTier(variant, h, board, len);
        float keep = o->raiseMask & bit ? KEEP_RAISE[k] : o->aggMask & bit ? KEEP_BET[k] : o->callMask & bit ? KEEP_CALL[k] : 1.0f;
        if (o->contMask & bit) keep *= KEEP_CONT[k];
        if (keep < 1.0f && rngFloat(rng) >= keep) return false;
    }
    return true;
}

float equityCalc(int variant, const int *hole, const int *board, int nb,
                 const OppRange *opp, int nopp, int trials, Rng *rng) {
    if (nopp <= 0) return 1.0f;
    int nh = variant == V_OMAHA ? 4 : 2;
    bool dead[52] = { 0 };
    for (int i = 0; i < nh; i++) dead[hole[i]] = true;
    for (int i = 0; i < nb; i++) dead[board[i]] = true;
    float win = 0;
    int oh[MAXP][4], b[5];
    for (int i = 0; i < nb; i++) b[i] = board[i];
    for (int t = 0; t < trials; t++) {
        bool used[52];
        memcpy(used, dead, sizeof used);
        for (int o = 0; o < nopp; o++) {
            for (int tries = 0;; tries++) {
                if (opp[o].set && variant == V_HOLDEM) sampleSet(opp[o].set, used, rng, oh[o]);
                else sampleRange(variant, opp[o].top, used, rng, oh[o]);
                if (tries >= 60 || accept(variant, oh[o], board, nb, &opp[o], rng)) break;
            }
            for (int k = 0; k < nh; k++) used[oh[o][k]] = true;
        }
        for (int i = nb; i < 5; i++) {
            int c;
            do c = rngInt(rng, 52); while (used[c]);
            used[c] = true;
            b[i] = c;
        }
        int mv = evalPlayer(variant, hole, b, 5), best = 0, ties = 0;
        for (int o = 0; o < nopp; o++) {
            int v = evalPlayer(variant, oh[o], b, 5);
            if (v > best) { best = v; ties = 0; }
            if (v == best) ties++;
        }
        if (mv > best) win += 1;
        else if (mv == best) win += 1.0f / (ties + 1);
    }
    return win / trials;
}
