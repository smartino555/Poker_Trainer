// ai.c - three bot personalities
//   Easy   - loose-passive "calling station": plays half its hands, calls with any pair or draw, rarely raises
//   Medium - tight but position-blind; judges hands against random cards, so it pays off strong bettors
//   Hard   - plays the coach's strategy (position charts, range-based equity, mixed bluffs) and adjusts to opponents
#include "ai.h"
#include "range.h"
#include <math.h>

const char *levelName(int lv) {
    static const char *N[4] = { "You", "Easy", "Medium", "Hard" };
    return N[lv];
}

static Action act(int type, int amount) { return (Action){ type, amount }; }

static Action raiseTo(const Legal *l, int to) {
    if (!l->canRaise) return act(l->canCheck ? A_CHECK : A_CALL, 0);
    if (to < l->minTo) to = l->minTo;
    if (to > l->maxTo) to = l->maxTo;
    return act(A_RAISE, to);
}

static Action passive(const Legal *l) { return act(l->canCheck ? A_CHECK : A_CALL, 0); }
static Action foldOrCheck(const Legal *l) { return act(l->canCheck ? A_CHECK : A_FOLD, 0); }

static int countLimpers(const Game *g, int p) {
    int n = 0;
    for (int q = 0; q < g->n; q++) if (q != p && g->s[q].inHand && g->s[q].pf == PF_LIMP) n++;
    return n;
}

// does the hand use the hole cards for a pair or better, or hold a draw
static bool improves(const Game *g, int p) { return connected(g->variant, g->s[p].hole, g->board, g->nboard); }

static Action easyBot(const Game *g, int p, Rng *rng) {
    const Seat *me = &g->s[p];
    Legal l;
    gameLegal(g, &l);
    int pot = gamePot(g), bb = g->bbAmt;
    float t = handTop(g->variant, me->hole), r = rngFloat(rng);
    if (g->street == ST_PREFLOP) {
        if (l.canCheck) return t <= 0.05f ? raiseTo(&l, 3 * bb) : act(A_CHECK, 0);
        if (g->nRaises == 0) {
            if (t <= 0.06f) return raiseTo(&l, 3 * bb + countLimpers(g, p) * bb);
            return t <= 0.55f || r < 0.1f ? act(A_CALL, 0) : act(A_FOLD, 0);
        }
        float rb = (float)g->curBet / bb;
        if (t <= 0.025f) return raiseTo(&l, 3 * g->curBet);
        float lim = rb <= 4 ? 0.45f : rb <= 10 ? 0.25f : 0.10f;
        return t <= lim || r < 0.05f ? act(A_CALL, 0) : act(A_FOLD, 0);
    }
    int v = evalPlayer(g->variant, me->hole, g->board, g->nboard), cat = handCat(v);
    bool own = improves(g, p);
    bool strong = cat >= HC_TWOPAIR && (g->variant == V_OMAHA || handCat(evalHand(g->board, g->nboard)) < cat);
    if (l.canCheck) {
        if (strong && r < 0.6f) return raiseTo(&l, pot / 2);
        if (own && r < 0.15f) return raiseTo(&l, pot / 3);
        return act(A_CHECK, 0);
    }
    int prior = pot - l.toCall;
    if (strong) return cat >= HC_TRIPS && r < 0.4f ? raiseTo(&l, l.minTo) : act(A_CALL, 0);
    if (own) return l.toCall > 1.5f * prior && r < 0.5f ? act(A_FOLD, 0) : act(A_CALL, 0);
    return l.toCall <= prior / 2 && r < 0.2f ? act(A_CALL, 0) : act(A_FOLD, 0);
}

static Action mediumBot(const Game *g, int p, Rng *rng) {
    const Seat *me = &g->s[p];
    Legal l;
    gameLegal(g, &l);
    int pot = gamePot(g), bb = g->bbAmt;
    float t = handTop(g->variant, me->hole), r = rngFloat(rng);
    if (g->street == ST_PREFLOP) {
        if (l.toCall >= 0.45f * (me->stack + me->bet) && !l.canCheck) return t <= 0.06f ? act(A_CALL, 0) : act(A_FOLD, 0);
        if (l.canCheck) return t <= 0.08f ? raiseTo(&l, 4 * bb) : act(A_CHECK, 0);
        if (g->nRaises == 0) {
            int limp = countLimpers(g, p);
            if (t <= 0.20f) return raiseTo(&l, 3 * bb + limp * bb);
            return limp && t <= 0.30f ? act(A_CALL, 0) : act(A_FOLD, 0);
        }
        if (g->nRaises == 1) {
            if (t <= 0.04f) return raiseTo(&l, 3 * g->curBet);
            return t <= (posIndex(g, p) == POS_BB ? 0.25f : 0.13f) ? act(A_CALL, 0) : act(A_FOLD, 0);
        }
        if (g->nRaises == 2) {
            if (t <= 0.02f) return raiseTo(&l, (int)(2.3f * g->curBet));
            return t <= 0.05f ? act(A_CALL, 0) : act(A_FOLD, 0);
        }
        return t <= 0.015f ? raiseTo(&l, me->stack + me->bet) : act(A_FOLD, 0);
    }
    OppRange any[MAXP];
    int nopp = 0;
    for (int q = 0; q < g->n; q++)
        if (q != p && g->s[q].inHand) any[nopp++] = (OppRange){ 1.0f, 0, 0, 0 };
    float eq = equityCalc(g->variant, me->hole, g->board, g->nboard, any, nopp, g->variant == V_OMAHA ? 150 : 400, rng);
    if (l.canCheck) {
        float thr = nopp == 1 ? 0.6f : nopp == 2 ? 0.45f : 0.35f;
        if (eq >= thr) return raiseTo(&l, pot * 2 / 3);
        if (g->street == ST_FLOP && g->pfRaiser == p && r < 0.6f) return raiseTo(&l, pot / 2);
        return act(A_CHECK, 0);
    }
    float need = (float)l.toCall / gameCallPot(g, p);
    if (eq >= 0.8f) return raiseTo(&l, 3 * g->curBet);
    return eq >= need ? act(A_CALL, 0) : act(A_FOLD, 0);
}

static Action hardBot(const Game *g, int p, Rng *rng) {
    Legal l;
    gameLegal(g, &l);
    Advice a;
    advise(g, p, &a, 700, rng);
    int type = a.act, amount = a.amount;
    if (a.alt >= 0 && rngFloat(rng) < a.altFreq) { type = a.alt; amount = a.aggAmount; }
    // exploits: don't bluff calling stations, value-bet them thinner
    bool station = false;
    for (int q = 0; q < g->n; q++) {
        const Seat *s = &g->s[q];
        if (q == p || !s->inHand || s->st.hands < 20) continue;
        if ((float)s->st.vpip / s->st.hands > 0.45f && (float)s->st.pfr / s->st.hands < 0.12f) station = true;
    }
    if (station && g->street > ST_PREFLOP) {
        if (type == A_RAISE && a.bluff && a.equity < 0.45f) {
            if (l.canCheck) type = A_CHECK;
            else type = a.q[A_CALL] >= 0 && a.q[A_CALL] <= Q_GOOD ? A_CALL : A_FOLD;
        } else if (type == A_CHECK && a.equity >= 0.5f && a.nopp == 1) {
            type = A_RAISE;
            amount = gamePot(g) * 3 / 5;
        }
    }
    if (type == A_RAISE) {
        int all = g->s[p].stack + g->s[p].bet;
        if (amount < all) amount = (int)(amount * (0.92f + 0.16f * rngFloat(rng)));
        return raiseTo(&l, amount);
    }
    if (type == A_FOLD) return foldOrCheck(&l);
    if (type == A_CHECK || type == A_CALL) return passive(&l);
    return act(type, 0);
}

Action aiDecide(const Game *g, int p, Rng *rng) {
    switch (g->s[p].level) {
    case LV_EASY: return easyBot(g, p, rng);
    case LV_HARD: return hardBot(g, p, rng);
    default: return mediumBot(g, p, rng);
    }
}
