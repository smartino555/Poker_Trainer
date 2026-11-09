// basic.c - Monte Carlo equity for every game, draw heuristics, simple advice and the bots that play it
#include "basic.h"
#include "range.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const VariantInfo *VI(const Game *g) { return varInfo(g->variant); }
static int lowR(int c) { return (cardRank(c) + 1) % 13; }     // ace-to-five order: A = 0 .. K = 12

int drawsLeft(const Game *g, int p) {
    const VariantInfo *vi = VI(g);
    if (vi->family != FAM_DRAW) return 0;
    int left = vi->draws - g->drawNo;
    if (g->phase == PH_DRAW && g->s[p].acted) left--;
    return left < 0 ? 0 : left;
}

// ---------------------------------------------------------------- what to draw

static int keepToMask(int keep, int n) { return ~keep & ((1 << n) - 1); }

static int drawHigh(const int *h, int n) {
    int v = evalHand(h, n), cat = handCat(v);
    if (cat >= HC_STRAIGHT) return 0;
    int cnt[13] = { 0 }, keep = 0;
    for (int i = 0; i < n; i++) cnt[cardRank(h[i])]++;
    if (cat >= HC_PAIR) {                          // keep the pairs / trips, draw to the rest
        for (int i = 0; i < n; i++) if (cnt[cardRank(h[i])] >= 2) keep |= 1 << i;
        return keepToMask(keep, n);
    }
    for (int s = 0; s < 4; s++) {                  // four to a flush
        int m = 0;
        for (int i = 0; i < n; i++) if (cardSuit(h[i]) == s) m |= 1 << i;
        if (__builtin_popcount(m) == 4) return keepToMask(m, n);
    }
    for (int lo = 1; lo + 3 <= 11; lo++) {         // four in a row, open at both ends
        int m = 0, have = 0;
        for (int i = 0; i < n; i++) {
            int r = cardRank(h[i]);
            if (r >= lo && r <= lo + 3 && !(have >> r & 1)) { m |= 1 << i; have |= 1 << r; }
        }
        if (__builtin_popcount(m) == 4) return keepToMask(m, n);
    }
    int hi = 0;                                    // nothing: keep the highest card
    for (int i = 1; i < n; i++) if (cardRank(h[i]) > cardRank(h[hi])) hi = i;
    return keepToMask(1 << hi, n);
}

// 2-7: five different low cards, no pair, straight or flush. Aces are high (bad).
static int draw27(const int *h, int n, int left) {
    int hv = (1 << 24) - evalLow27(h, n);
    if (handCat(hv) == HC_HIGH && handRank1(hv) <= (left <= 1 ? 7 : 6)) return 0;   // a nine (last draw) or eight: stand pat
    int T = left >= 3 ? 5 : 6;                     // keep sevens and lower early, eights later
    int order[5];
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && cardRank(h[order[j]]) < cardRank(h[order[j - 1]]); j--) { int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t; }
    int keep = 0, used = 0, k = 0;
    for (int i = 0; i < n && k < 4; i++) {
        int r = cardRank(h[order[i]]);
        if (r > T || (used >> r & 1)) continue;
        keep |= 1 << order[i];
        used |= 1 << r;
        k++;
    }
    return keepToMask(keep, n);
}

// badugi: low cards of four different suits and ranks
static int drawBadugi(const int *h, int n, int left) {
    int v = evalBadugi(h, n), key = (1 << 20) - v, size = 4 - (key >> 16), top = key >> 12 & 15;
    if (size == 4 && top <= (left <= 1 ? 12 : 9)) return 0;     // a badugi (ten or better early): stand pat
    int sub = 0;
    for (int m = 1; m < 1 << n && !sub; m++) {                    // the cards that make the hand
        int sel[4], k = 0;
        for (int i = 0; i < n; i++) if (m >> i & 1) sel[k++] = h[i];
        if (k == size && evalBadugi(sel, k) == v) sub = m;
    }
    int T = left >= 2 ? 7 : 9, keep = 0;
    for (int i = 0; i < n; i++) if ((sub >> i & 1) && lowR(h[i]) <= T) keep |= 1 << i;
    return keepToMask(keep, n);
}

int basicDrawChoice(int variant, const int *hole, int n, int left) {
    switch (varInfo(variant)->rank) {
    case RK_LOW_27: return draw27(hole, n, left);
    case RK_BADUGI: return drawBadugi(hole, n, left);
    default: return drawHigh(hole, n);
    }
}

// ---------------------------------------------------------------- hand strength for range weighting

static int preScore(int variant, const int *c, int n) {
    if (n == 2) return (int)((1 - hcTop[hcClass(c[0], c[1])]) * 1000);
    float best = -1e9f;
    if (n == 5) {
        for (int skip = 0; skip < 5; skip++) {
            int h[4], k = 0;
            for (int i = 0; i < 5; i++) if (i != skip) h[k++] = c[i];
            float s = omahaScore(h);
            if (s > best) best = s;
        }
    } else best = omahaScore(c);
    int sc = (int)(best * 10);
    if (variant == V_OMAHA8) {                     // low cards matter: aces and deuces above all
        int lm = 0;
        for (int i = 0; i < n; i++) if (lowR(c[i]) <= 2) lm |= 1 << lowR(c[i]);
        sc += 40 * __builtin_popcount(lm) + ((lm & 3) == 3 ? 60 : 0);
    }
    return sc;
}

// a comparable score for the hand someone holds right now (only used to compare samples of one player)
static int strength(int variant, const int *c, int n, const int *board, int nb) {
    const VariantInfo *vi = varInfo(variant);
    if (vi->family == FAM_BOARD && nb < 3) return preScore(variant, c, n);
    int lo = 0, v;
    if (vi->family == FAM_STUD && n < 5) v = variant == V_RAZZ ? evalLowA5(c, n) : evalHand(c, n);
    else v = evalCards(variant, c, n, board, nb, &lo);
    if (vi->rank == RK_HILO8) {
        int lm = 0;
        for (int i = 0; i < n; i++) if (lowR(c[i]) <= 7) lm |= 1 << lowR(c[i]);
        for (int i = 0; i < nb; i++) if (lowR(board[i]) <= 7) lm |= 1 << lowR(board[i]);
        return handCat(v) * 10 + (lo ? 20 : __builtin_popcount(lm) * 3);
    }
    return v;
}

// ---------------------------------------------------------------- equity

typedef struct {
    int seat, n, known, k;   // known: bit mask of hole positions everyone can see
    int drew, drewLeft;      // draw games: cards drawn last time, and the draws left then (-1 none)
    bool toDraw;             // still to draw in the current draw
} SimOpp;

// picks m random cards from pool[0..np) without removing them: they end up in pool[np-m .. np)
static void peek(int *pool, int np, int m, int *out, Rng *rng) {
    for (int i = 0; i < m && i < np; i++) {
        int j = rngInt(rng, np - i), t = pool[j];
        pool[j] = pool[np - 1 - i];
        pool[np - 1 - i] = t;
        out[i] = t;
    }
}

static void removeCards(int *pool, int *np, const int *c, int m) {
    for (int i = 0; i < m; i++)
        for (int j = 0; j < *np; j++)
            if (pool[j] == c[i]) { pool[j] = pool[--*np]; break; }
}

static int take(int *pool, int *np, Rng *rng) {
    if (*np <= 0) return -1;
    int j = rngInt(rng, *np), c = pool[j];
    pool[j] = pool[--*np];
    return c;
}

static void redraw(int *h, int n, int mask, int *pool, int *np, Rng *rng) {
    for (int i = 0; i < n; i++)
        if (mask >> i & 1) { int c = take(pool, np, rng); if (c >= 0) h[i] = c; }
}

float basicEquity(const Game *g, int p, int discard, bool weighted, int trials, Rng *rng) {
    const VariantInfo *vi = VI(g);
    const Seat *me = &g->s[p];
    bool dead[52] = { 0 };
    for (int c = 0; c < 52; c++) if (vi->deck == 36 && cardRank(c) < 4) dead[c] = true;
    for (int k = 0; k < me->nhole; k++) dead[me->hole[k]] = true;
    for (int i = 0; i < g->nboard; i++) dead[g->board[i]] = true;
    for (int q = 0; q < g->n; q++)
        if (q != p) for (int k = 0; k < g->s[q].nhole; k++) if (g->s[q].upMask >> k & 1) dead[g->s[q].hole[k]] = true;
    for (int i = 0; i < g->ndisc; i++) if (g->discOwner[i] == p) dead[g->disc[i]] = true;
    int pool0[52], np0 = 0;
    for (int c = 0; c < 52; c++) if (!dead[c]) pool0[np0++] = c;

    SimOpp opp[MAXP];
    int nopp = 0;
    for (int q = 0; q < g->n; q++) {
        const Seat *s = &g->s[q];
        if (q == p || !s->inHand) continue;
        SimOpp *o = &opp[nopp++];
        memset(o, 0, sizeof *o);
        o->seat = q;
        o->n = s->nhole;
        o->known = s->upMask;
        o->k = 1;
        if (weighted) {
            o->k += __builtin_popcount(s->aggMask) + __builtin_popcount(s->raiseMask);
            if (s->pfrThis && vi->family == FAM_BOARD) o->k++;
            if (o->k > 4) o->k = 4;
        }
        o->drew = -1;
        if (vi->family == FAM_DRAW) {
            int last = g->phase == PH_DRAW && s->acted ? g->drawNo : g->drawNo - 1;
            if (last >= 0 && s->drew[last] >= 0) { o->drew = s->drew[last]; o->drewLeft = vi->draws - last; }
            o->toDraw = g->phase == PH_DRAW && !s->acted;
        }
    }
    if (nopp == 0) return 1.0f;
    int future = vi->family == FAM_DRAW ? vi->draws - g->drawNo - (g->phase == PH_DRAW ? 1 : 0) : 0;
    if (future < 0) future = 0;
    bool heroDrawsNow = g->phase == PH_DRAW && !me->acted;
    bool split = vi->rank == RK_HILO8;

    float total = 0;
    for (int t = 0; t < trials; t++) {
        int pool[52], np = np0;
        memcpy(pool, pool0, sizeof(int) * np0);
        int hand[MAXP + 1][MAXH], nh[MAXP + 1];
        nh[0] = me->nhole;
        memcpy(hand[0], me->hole, sizeof(int) * MAXH);
        int board[5], nb = g->nboard;
        memcpy(board, g->board, sizeof board);
        // opponents' hidden cards
        for (int o = 0; o < nopp; o++) {
            const SimOpp *so = &opp[o];
            const Seat *s = &g->s[so->seat];
            int *h = hand[o + 1], hidden = 0, pos[MAXH];
            nh[o + 1] = so->n;
            for (int k = 0; k < so->n; k++) {
                if (so->known >> k & 1) h[k] = s->hole[k];
                else pos[hidden++] = k;
            }
            if (vi->family == FAM_DRAW && weighted && so->drew >= 0) {
                // a hand that would have drawn exactly as many cards as they did, then their new cards
                int pre[MAXH];
                for (int tries = 0; tries < 40; tries++) {
                    peek(pool, np, so->n, pre, rng);
                    if (__builtin_popcount(basicDrawChoice(g->variant, pre, so->n, so->drewLeft)) == so->drew) break;
                }
                removeCards(pool, &np, pre, so->n);
                int m = basicDrawChoice(g->variant, pre, so->n, so->drewLeft);
                if (__builtin_popcount(m) != so->drew) m = (1 << so->drew) - 1;
                memcpy(h, pre, sizeof(int) * so->n);
                redraw(h, so->n, m, pool, &np, rng);
                continue;
            }
            int best[MAXH], bestS = -(1 << 30);
            for (int k = 0; k < so->k; k++) {
                int smp[MAXH], cur[MAXH];
                peek(pool, np, hidden, smp, rng);
                memcpy(cur, h, sizeof(int) * so->n);
                for (int i = 0; i < hidden; i++) cur[pos[i]] = smp[i];
                int sc = so->k > 1 ? strength(g->variant, cur, so->n, board, nb) : 0;
                if (k == 0 || sc > bestS) { bestS = sc; memcpy(best, smp, sizeof(int) * hidden); }
            }
            removeCards(pool, &np, best, hidden);
            for (int i = 0; i < hidden; i++) h[pos[i]] = best[i];
        }
        // draws: the one in progress, then the ones still to come
        if (vi->family == FAM_DRAW) {
            if (g->phase == PH_DRAW) {
                if (heroDrawsNow) redraw(hand[0], nh[0], discard >= 0 ? discard : basicDrawChoice(g->variant, hand[0], nh[0], future + 1), pool, &np, rng);
                for (int o = 0; o < nopp; o++)
                    if (opp[o].toDraw) redraw(hand[o + 1], nh[o + 1], basicDrawChoice(g->variant, hand[o + 1], nh[o + 1], future + 1), pool, &np, rng);
            }
            for (int d = 0; d < future; d++)
                for (int o = 0; o <= nopp; o++)
                    redraw(hand[o], nh[o], basicDrawChoice(g->variant, hand[o], nh[o], future - d), pool, &np, rng);
        } else if (vi->family == FAM_STUD) {
            for (int o = 0; o <= nopp; o++)
                while (nh[o] < 7) { int c = take(pool, &np, rng); if (c < 0) break; hand[o][nh[o]++] = c; }
        } else {
            while (nb < 5) { int c = take(pool, &np, rng); if (c < 0) break; board[nb++] = c; }
        }
        // showdown
        int hv[MAXP + 1], lv[MAXP + 1], bestH = -1, bestL = 0;
        for (int o = 0; o <= nopp; o++) {
            hv[o] = evalCards(g->variant, hand[o], nh[o], board, nb, &lv[o]);
            if (hv[o] > bestH) bestH = hv[o];
            if (lv[o] > bestL) bestL = lv[o];
        }
        int tieH = 0, tieL = 0;
        for (int o = 0; o <= nopp; o++) { tieH += hv[o] == bestH; tieL += bestL && lv[o] == bestL; }
        float hiShare = hv[0] == bestH ? 1.0f / tieH : 0;
        if (split && bestL) total += 0.5f * hiShare + (lv[0] == bestL ? 0.5f / tieL : 0);
        else total += hiShare;
    }
    return total / trials;
}

// ---------------------------------------------------------------- describing a hand

static void listCards(const int *c, int n, char *buf, int len) {
    int o = 0;
    buf[0] = 0;
    for (int i = 0; i < n && o < len - 4; i++) {
        char cs[3];
        cardStr(c[i], cs);
        o += snprintf(buf + o, len - o, "%s%s", i ? " " : "", cs);
    }
}

void describeHandV(const Game *g, int p, char *buf, int n) {
    const Seat *s = &g->s[p];
    const VariantInfo *vi = VI(g);
    char hn[64], ln[40];
    buf[0] = 0;
    if (s->nhole == 0) return;
    if (vi->family == FAM_BOARD && g->nboard < 3) {
        char cs[32];
        listCards(s->hole, s->nhole, cs, sizeof cs);
        if (s->nhole == 2) {
            char cls[4];
            hcClassName(hcClass(s->hole[0], s->hole[1]), cls);
            snprintf(buf, n, "%s (%s)", cls, cs);
        } else snprintf(buf, n, "%s", cs);
        if (g->variant == V_OMAHA8) {
            int lm = 0;
            for (int i = 0; i < s->nhole; i++) if (lowR(s->hole[i]) <= 7) lm |= 1 << lowR(s->hole[i]);
            size_t l = strlen(buf);
            snprintf(buf + l, n - l, __builtin_popcount(lm) >= 2 ? " - %d different low cards (8 or lower)" : " - not much for low", __builtin_popcount(lm));
        }
        return;
    }
    if (vi->family == FAM_STUD && s->nhole < 5) {
        if (g->variant == V_RAZZ) {                // a partial low: "7-4-2 so far"
            int r[5], k = s->nhole, pairs = 0;
            for (int i = 0; i < k; i++) r[i] = lowR(s->hole[i]);
            for (int i = 1; i < k; i++) for (int j = i; j > 0 && r[j] > r[j - 1]; j--) { int t = r[j]; r[j] = r[j - 1]; r[j - 1] = t; }
            int o = 0;
            for (int i = 0; i < k; i++) {
                if (i && r[i] == r[i - 1]) { pairs++; continue; }
                o += snprintf(buf + o, n - o, "%s%c", o ? "-" : "", "A23456789TJQK"[r[i]]);
            }
            snprintf(buf + o, n - o, pairs ? " so far (with a pair - bad in Razz)" : " so far");
            return;
        }
        handName(evalHand(s->hole, s->nhole), hn, sizeof hn);
        snprintf(buf, n, "%s so far", hn);
        return;
    }
    int lo = 0, v = gameSeatValue(g, p, &lo);
    handNameV(g->variant, v, hn, sizeof hn);
    if (vi->rank == RK_HILO8) {
        if (lo) { lowName(lo, ln, sizeof ln); snprintf(buf, n, "High: %s. Low: %s", hn, ln); }
        else snprintf(buf, n, "High: %s. No low yet", hn);
    } else snprintf(buf, n, "%s", hn);
}

// ---------------------------------------------------------------- advice

static void tip(Tips *t, const char *fmt, ...) {
    if (t->ntips >= NTIPS) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(t->tips[t->ntips++], sizeof t->tips[0], fmt, ap);
    va_end(ap);
}

static const char *M(int v) {
    static char buf[6][24];
    static int k = 0;
    char *b = buf[k++ % 6];
    fmtChips(v, b, 24);
    return b;
}

static int clampTo(const Legal *l, int to) { return to < l->minTo ? l->minTo : to > l->maxTo ? l->maxTo : to; }

// bet / raise size: fixed limit has one size; pot-limit and no-limit bet 2/3 pot or raise to 3x
static int aggSize(const Game *g, const Legal *l, bool strong) {
    if (gameIsFixed(g)) return strong ? l->maxTo : l->minTo;
    int pot = gamePot(g), to = g->curBet == 0 ? pot * 2 / 3 : 3 * g->curBet;
    int unit = g->bbAmt >= 2 ? g->bbAmt / 2 : 1;
    to = (to + unit / 2) / unit * unit;
    return clampTo(l, to);
}

static const char *goalText(int variant) {
    switch (varInfo(variant)->rank) {
    case RK_LOW_A5: return "Razz is played for low: five different low cards win, aces are low, and straights and flushes don't count. 5-4-3-2-A is the best hand.";
    case RK_LOW_27: return "2-7 is played for low: five different low cards, with no pair, straight or flush. Aces are high. 7-5-4-3-2 is the best hand.";
    case RK_BADUGI: return "Badugi: you want four low cards of four different suits and four different ranks. A 4-card badugi beats any 3-card hand; aces are low.";
    case RK_HILO8: return "Hi-lo: the best high hand and the best low (five different cards 8 or lower) split the pot. Hands that can win both ways scoop it all.";
    default: return "";
    }
}

static void drawAdvice(const Game *g, int p, Tips *t, int trials, Rng *rng) {
    const Seat *s = &g->s[p];
    int n = s->nhole, left = drawsLeft(g, p);
    int h = basicDrawChoice(g->variant, s->hole, n, left);
    bool high = varInfo(g->variant)->rank == RK_HIGH;
    // alternatives: stand pat; for high hands keep one more card (a kicker), for lowball break one more
    int cand[3] = { h, 0, -1 }, nc = 2, pick = -1;
    for (int i = 0; i < n; i++) {
        bool thrown = h >> i & 1;
        if (high ? !thrown : thrown) continue;
        int r = varInfo(g->variant)->rank == RK_BADUGI ? lowR(s->hole[i]) : cardRank(s->hole[i]);
        int pr = pick < 0 ? -1 : varInfo(g->variant)->rank == RK_BADUGI ? lowR(s->hole[pick]) : cardRank(s->hole[pick]);
        if (pick < 0 || r > pr) pick = i;        // high: the best card thrown away; low: the worst card kept
    }
    // lowball: breaking one more card only makes sense from a near-made hand (it must leave at least two low cards)
    if (pick >= 0 && (high ? __builtin_popcount(h) >= 2 : h != 0 && n - __builtin_popcount(h) >= 3)) cand[nc++] = h ^ 1 << pick;
    float eq[3];
    int best = 0;
    for (int i = 0; i < nc; i++) {
        bool dup = false;
        for (int j = 0; j < i; j++) dup |= cand[j] == cand[i];
        eq[i] = dup ? -1 : basicEquity(g, p, cand[i], true, trials / 3 + 1, rng);
        // the simulation is noisy (a few hundred deals): leave the heuristic's draw only for a clear gain
        if (eq[i] > eq[best] + (best == 0 ? 0.04f : 0.01f)) best = i;
    }
    int m = cand[best];
    t->draw = true;
    t->act = A_DRAW;
    t->amount = m;
    t->equity = eq[best];
    char cs[40] = "";
    int o = 0;
    for (int i = 0; i < n; i++) if (m >> i & 1) { char c[3]; cardStr(s->hole[i], c); o += snprintf(cs + o, sizeof cs - o, "%s%s", o ? " " : "", c); }
    if (m == 0) snprintf(t->head, sizeof t->head, "Stand pat (keep all %d)", n);
    else snprintf(t->head, sizeof t->head, "Draw %d (throw away %s)", __builtin_popcount(m), cs);
    if (goalText(g->variant)[0]) tip(t, "%s", goalText(g->variant));
    tip(t, "Your hand now: %s.", t->hand);
    if (m == 0) tip(t, "Your hand is already good enough to keep: standing pat also tells the others you have a made hand.");
    else tip(t, "Keep the cards that already work together and replace the rest. With this draw you win about %.0f%% of the pot on average; standing pat wins about %.0f%%.",
             eq[best] * 100, eq[1] >= 0 ? eq[1] * 100 : t->equity * 100);
    if (left > 1) tip(t, "This is one of %d draws left, so you can still aim for a strong hand.", left);
    else if (left == 1) tip(t, "This is the last draw - after it, only betting is left.");
    for (int q = 0; q < g->n; q++) {
        const Seat *os = &g->s[q];
        if (q == p || !os->inHand) continue;
        int last = g->phase == PH_DRAW && os->acted ? g->drawNo : g->drawNo - 1;
        if (last < 0 || os->drew[last] < 0) continue;
        if (os->drew[last] == 0) { tip(t, "%s stood pat last draw: expect a made hand (or a bluff).", os->name); break; }
    }
}

void adviseBasic(const Game *g, int p, Tips *t, int trials, Rng *rng) {
    memset(t, 0, sizeof *t);
    t->alt = -1;
    if (g->toAct != p || g->handOver) return;
    const VariantInfo *vi = VI(g);
    Legal l;
    gameLegal(g, &l);
    describeHandV(g, p, t->hand, sizeof t->hand);
    t->nopp = gameLive(g) - 1;
    if (l.draw) { drawAdvice(g, p, t, trials, rng); return; }
    float eq = basicEquity(g, p, -1, true, trials, rng);
    int winPot = gameCallPot(g, p) - l.toCall;
    float need = l.toCall > 0 ? (float)l.toCall / (winPot + l.toCall) : 0;
    float fair = 1.0f / (t->nopp + 1);
    bool moreToCome = vi->family == FAM_DRAW ? drawsLeft(g, p) > 0 : g->street < vi->rounds - 1;
    t->equity = eq;
    t->need = need;
    bool aggrOpp = false;
    for (int q = 0; q < g->n; q++) if (q != p && g->s[q].inHand && g->s[q].aggMask) aggrOpp = true;
    if (goalText(g->variant)[0] && g->street == 0) tip(t, "%s", goalText(g->variant));
    tip(t, "Your hand: %s.", t->hand);
    tip(t, "Against %d opponent%s you win about %.0f%% of the pot on average%s.", t->nopp, t->nopp == 1 ? "" : "s", eq * 100,
        aggrOpp ? " (their bets and raises point to stronger hands, so that's counted in)" : "");
    if (l.toCall > 0) tip(t, "Pot odds: call %s to win %s - you need %.0f%% to break even.", M(l.toCall), M(winPot), need * 100);
    if (l.bringIn) {
        bool comp = l.canRaise && eq >= fair + 0.12f;
        t->act = comp ? A_RAISE : A_CALL;
        t->amount = comp ? l.minTo : 0;
        snprintf(t->head, sizeof t->head, comp ? "Complete to %s" : "Bring in for %s", M(comp ? l.minTo : l.toCall));
        tip(t, comp ? "Your hand is strong for this street: completing makes everyone pay a full bet to see the next card."
                    : "You have to put in the bring-in. With an ordinary hand, the smallest amount is best: you can't fold, and completing builds a pot you're not ahead in.");
        return;
    }
    if (l.canCheck) {
        float value = fair + (t->nopp == 1 ? 0.12f : 0.15f);
        if (l.canRaise && eq >= value) {
            t->act = A_RAISE;
            t->amount = aggSize(g, &l, eq >= value + 0.15f);
            snprintf(t->head, sizeof t->head, "Bet %s", M(t->amount));
            tip(t, "You're ahead of the hands that could call: bet for value. Checking lets worse hands see the next card for free.");
        } else {
            t->act = A_CHECK;
            snprintf(t->head, sizeof t->head, "Check");
            if (l.canRaise && moreToCome && t->nopp == 1 && eq >= 0.38f) {
                t->alt = A_RAISE;
                t->amount = aggSize(g, &l, false);
                tip(t, "A bet (a semi-bluff) is a fine alternative: you can win now if they fold, and still improve if they call.");
            } else tip(t, eq >= fair ? "About an even share: checking keeps the pot small and gets you to the next card cheaply."
                                     : "You're behind: check and see what comes. Betting would mostly get called by better hands.");
        }
        return;
    }
    float raiseAt = fmaxf(fair + 0.25f, need + 0.2f);
    if (gameIsFixed(g) && g->nBets >= 4) raiseAt = g->nBets >= 6 ? 2.0f : fmaxf(raiseAt, 0.85f);   // heads-up, uncapped: don't war forever
    int behind = 0;                                // players who still act after you this round
    for (int q = 0; q < g->n; q++) if (q != p && g->s[q].inHand && !g->s[q].allin && !g->s[q].acted) behind++;
    bool firstRound = g->street == 0 && behind >= 2;
    if (l.canRaise && eq >= raiseAt) {
        t->act = A_RAISE;
        t->amount = aggSize(g, &l, true);
        snprintf(t->head, sizeof t->head, "Raise to %s", M(t->amount));
        tip(t, "You win far more than your share: raise for value and make the worse hands pay.");
    } else if (firstRound && eq < fair * 1.1f && eq < need + 0.15f) {
        t->act = A_FOLD;
        snprintf(t->head, sizeof t->head, "Fold");
        tip(t, "The price looks cheap, but %d players still act behind you and any of them can raise. On the first round, play hands that win more than their fair share (about %.0f%% here) - this one wins %.0f%%.",
            behind, fair * 100, eq * 100);
    } else if (eq >= need) {
        t->act = A_CALL;
        snprintf(t->head, sizeof t->head, "Call %s", M(l.toCall));
        tip(t, "Your share of the pot is bigger than the price: calling wins money in the long run.");
    } else {
        t->act = A_FOLD;
        snprintf(t->head, sizeof t->head, "Fold");
        tip(t, "You need %.0f%% but win only about %.0f%%: fold. Calling here loses chips on average.", need * 100, eq * 100);
        if (moreToCome && eq >= need * 0.8f) t->alt = A_CALL;
    }
    if (vi->family == FAM_STUD) {
        int best = gameBestShowing(g);
        if (best >= 0 && best != p) {
            int up[MAXH], k = gameUpCards(g, best, up);
            char cs[32];
            listCards(up, k, cs, sizeof cs);
            tip(t, "Watch the up cards: %s shows %s. Cards you need that are showing elsewhere are dead - they can't come to you.", g->s[best].name, cs);
        }
    }
}

// ---------------------------------------------------------------- bots

static Action act(int type, int amount) { return (Action){ type, amount }; }

Action basicBot(const Game *g, int p, int level, Rng *rng) {
    const Seat *me = &g->s[p];
    Legal l;
    gameLegal(g, &l);
    if (l.draw) {
        if (level == LV_HARD) {
            Tips t;
            adviseBasic(g, p, &t, 400, rng);
            return act(A_DRAW, t.amount);
        }
        int m = basicDrawChoice(g->variant, me->hole, me->nhole, drawsLeft(g, p));
        if (level == LV_EASY && m && rngFloat(rng) < 0.3f) m &= m - 1;     // sloppy: keeps a card it should throw
        return act(A_DRAW, m);
    }
    int nopp = gameLive(g) - 1;
    float fair = 1.0f / (nopp + 1), r = rngFloat(rng);
    if (level == LV_HARD) {
        Tips t;
        adviseBasic(g, p, &t, 260, rng);
        int type = t.act, amount = t.amount;
        if (t.alt == A_RAISE && r < 0.35f && l.canRaise) type = A_RAISE;
        else if (t.alt == A_CALL && r < 0.3f) type = A_CALL;
        if (type == A_RAISE) {
            if (!l.canRaise) return act(l.canCheck ? A_CHECK : A_CALL, 0);
            return act(A_RAISE, clampTo(&l, amount > 0 ? amount : aggSize(g, &l, false)));
        }
        if (type == A_FOLD && l.canCheck) type = A_CHECK;
        if (type == A_CHECK && !l.canCheck) type = A_CALL;
        if (type == A_CALL && l.canCheck) type = A_CHECK;
        return act(type, 0);
    }
    float eq = basicEquity(g, p, -1, false, level == LV_EASY ? 80 : 160, rng);
    int winPot = gameCallPot(g, p) - l.toCall;
    float need = l.toCall > 0 ? (float)l.toCall / (winPot + l.toCall) : 0;
    bool easy = level == LV_EASY;
    if (l.bringIn) return l.canRaise && eq >= fair + (easy ? 0.3f : 0.2f) ? act(A_RAISE, l.minTo) : act(A_CALL, 0);
    if (l.canCheck) {
        bool bet = l.canRaise && eq >= fair + (easy ? 0.25f : 0.18f) && (!easy || r < 0.5f);
        return bet ? act(A_RAISE, aggSize(g, &l, false)) : act(A_CHECK, 0);
    }
    bool war = gameIsFixed(g) && (g->nBets >= 6 || (g->nBets >= 4 && eq < 0.85f));   // heads-up, uncapped: stop re-raising
    if (l.canRaise && !war && eq >= fair + (easy ? 0.4f : 0.35f) && (!easy || r < 0.3f)) return act(A_RAISE, aggSize(g, &l, false));
    if (easy) return eq >= need * 0.6f || r < 0.15f ? act(A_CALL, 0) : act(A_FOLD, 0);
    return eq >= need + 0.04f ? act(A_CALL, 0) : act(A_FOLD, 0);
}
