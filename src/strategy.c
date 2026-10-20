// strategy.c - preflop charts, range estimates and equity-based postflop play, with reasons
#include "strategy.h"
#include "range.h"
#include "pushfold.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------- Hold'em preflop charts (6-max, ~100 BB)

static const char *RFI_STR[5] = {
    "22+,A2s+,K9s+,Q9s+,J9s+,T9s,98s,87s,76s,ATo+,KJo+,QJo",                                           // UTG ~18%
    "22+,A2s+,K7s+,Q9s+,J9s+,T8s+,97s+,87s,76s,65s,ATo+,KTo+,QTo+,JTo",                                // HJ ~22%
    "22+,A2s+,K5s+,Q7s+,J8s+,T7s+,97s+,86s+,75s+,65s,54s,A8o+,KTo+,QTo+,JTo",                          // CO ~29%
    "22+,A2s+,K2s+,Q4s+,J6s+,T6s+,96s+,85s+,74s+,63s+,53s+,43s,A2o+,K8o+,Q9o+,J9o+,T8o+,98o,87o",      // BTN ~45%
    "22+,A2s+,K4s+,Q6s+,J7s+,T7s+,96s+,86s+,75s+,64s+,54s,A4o+,K9o+,Q9o+,J9o+,T9o",                    // SB ~38%
};
static const char *HU_BTN_STR = "22+,A2s+,K2s+,Q2s+,J3s+,T5s+,95s+,84s+,74s+,63s+,53s+,43s,A2o+,K3o+,Q6o+,J7o+,T7o+,97o+,87o,76o";
// facing a single open: [opener early / late][hero in position, small blind, big blind]
static const char *VS_OPEN_3B[2][3] = {
    { "QQ+,AKs,AKo,A5s,A4s", "JJ+,AQs+,AKo,KQs,A5s", "QQ+,AKs,AKo,A5s,A4s" },
    { "TT+,AJs+,KQs,AQo+,A5s,A4s,K9s,76s", "99+,ATs+,KJs+,QJs,AJo+,KQo,A5s-A3s,K9s,JTs,T9s", "TT+,AJs+,KQs,AQo+,A5s-A3s,K5s,Q9s,76s,65s" },
};
static const char *VS_OPEN_CALL[2][3] = {
    { "22-JJ,ATs-AQs,KQs,KJs,QJs,JTs,T9s,98s,AQo", "",
      "22-JJ,A6s-AQs,A2s-A3s,K8s+,Q9s+,J9s+,T8s+,97s+,86s+,76s,65s,54s,AJo-AQo,KJo+,QJo" },
    { "22-99,A9s-ATs,KTs-KJs,QTs+,J9s+,T9s,98s,87s,AJo,KQo", "",
      "22-99,A6s-ATs,A2s,K6s-KJs,K2s-K4s,Q4s-Q8s,QTs+,J6s+,T6s+,96s+,85s+,74s-75s,63s-64s,53s+,43s,A2o-AJo,K8o+,Q9o+,J9o+,T8o+,98o,87o" },
};
static const char *HU_BB_3B = "88+,ATs+,KTs+,QJs,AJo+,KQo,A5s-A2s,K9s,J9s,T8s";
static const char *VS3B_4B = "QQ+,AKs,AKo,A5s";
static const char *VS3B_CALL_IP = "77-JJ,AJs-AQs,KQs,KJs,QJs,JTs,T9s,98s,AQo";
static const char *VS3B_CALL_OOP = "TT-JJ,AJs-AQs,KQs,AQo";
static const char *VS3B_CALL_LATE = "55-JJ,A9s-AQs,KTs+,QTs+,J9s+,T9s,98s,87s,AJo-AQo,KQo";
static const char *VS3B_CALL_LATE_OOP = "88-JJ,ATs-AQs,KJs+,QJs,JTs,AQo";
static const char *COLD3B_4B = "KK+,AKs", *COLD3B_CALL = "JJ-QQ,AKo,AQs";
static const char *VS4B_JAM = "KK+,AKs", *VS4B_CALL = "QQ,AKo";
static const char *BB_ISO = "99+,AJs+,KQs,AQo+", *BB_ISO_HU = "77+,A8s+,KTs+,QJs,ATo+,KJo+";
static const char *OVERLIMP = "22-77,A2s-A9s,KTs,QTs,JTs,T9s,98s,87s,76s,65s,54s";

static Range R_RFI[5], R_HU, R_3B[2][3], R_CALL[2][3], R_HU3B, R_V3B4, R_V3BIP, R_V3BOOP, R_V3BLATE, R_V3BLOOP,
             R_C3B4, R_C3BC, R_V4J, R_V4C, R_ISO, R_ISOHU, R_OVL;

// Omaha: share of hands to play by position (pot-limit, ~100 BB)
static const float OM_RFI[5] = { 0.15f, 0.19f, 0.26f, 0.42f, 0.32f };

static void initCharts(void) {
    static bool done = false;
    if (done) return;
    done = true;
    rangesInit();
    for (int i = 0; i < 5; i++) rangeParse(&R_RFI[i], RFI_STR[i]);
    rangeParse(&R_HU, HU_BTN_STR);
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 3; b++) {
            rangeParse(&R_3B[a][b], VS_OPEN_3B[a][b]);
            rangeParse(&R_CALL[a][b], VS_OPEN_CALL[a][b]);
            for (int k = 0; k < NCLS; k++) if (R_3B[a][b].in[k]) R_CALL[a][b].in[k] = 0;   // a hand is either a 3-bet or a call
        }
    rangeParse(&R_HU3B, HU_BB_3B);
    rangeParse(&R_V3B4, VS3B_4B);
    rangeParse(&R_V3BIP, VS3B_CALL_IP);
    rangeParse(&R_V3BOOP, VS3B_CALL_OOP);
    rangeParse(&R_V3BLATE, VS3B_CALL_LATE);
    rangeParse(&R_V3BLOOP, VS3B_CALL_LATE_OOP);
    rangeParse(&R_C3B4, COLD3B_4B);
    rangeParse(&R_C3BC, COLD3B_CALL);
    rangeParse(&R_V4J, VS4B_JAM);
    rangeParse(&R_V4C, VS4B_CALL);
    rangeParse(&R_ISO, BB_ISO);
    rangeParse(&R_ISOHU, BB_ISO_HU);
    rangeParse(&R_OVL, OVERLIMP);
}

float rfiFraction(const Game *g, int pos) {
    initCharts();
    if (pos >= POS_BB) return 1.0f;
    bool hu = g->n == 2;
    if (g->variant == V_OMAHA) return hu ? 0.65f : OM_RFI[pos];
    return hu ? rangeFraction(&R_HU) : rangeFraction(&R_RFI[pos]);
}

static ChartDef CHARTS[] = {
    { "UTG open", "First in from Under the Gun: 5 players still act behind you, so open only strong hands.", NULL, "" },
    { "HJ open", "Hijack: one seat later, a little wider.", NULL, "" },
    { "CO open", "Cutoff: only the button and blinds remain - open almost 30%.", NULL, "" },
    { "BTN open", "Button: you'll have position on everyone after the flop - open nearly half your hands.", NULL, "" },
    { "SB open", "Small blind, folded to you: raise or fold (only the big blind is left, but you'll be out of position).", NULL, "" },
    { "Heads-up BTN", "Heads-up the button is also the small blind and should raise most hands.", NULL, "" },
    { "vs early open", "In position facing an open from UTG / HJ. Red = 3-bet, blue = call.", NULL, NULL },
    { "vs late open", "In position facing an open from CO / BTN. Red = 3-bet, blue = call.", NULL, NULL },
    { "SB vs late", "Small blind facing a late open: 3-bet or fold. Calling out of position with the BB behind is a leak.", NULL, NULL },
    { "BB vs early", "Big blind facing an early open: you close the action at a discount, so defend more.", NULL, NULL },
    { "BB vs late", "Big blind facing a button / cutoff open: defend wide - you need little equity at this price.", NULL, NULL },
    { "vs 3-bet (IP)", "You opened, got 3-bet, and will be in position. Red = 4-bet, blue = call.", NULL, NULL },
    { "vs 3-bet (OOP)", "You opened, got 3-bet, and will be out of position: continue tighter.", NULL, NULL },
};

int chartCount(void) { return (int)(sizeof CHARTS / sizeof CHARTS[0]); }

const ChartDef *chartGet(int i) {
    if (!CHARTS[0].raise) {
        for (int k = 0; k < 5; k++) CHARTS[k].raise = RFI_STR[k];
        CHARTS[5].raise = HU_BTN_STR;
        CHARTS[6].raise = VS_OPEN_3B[0][0]; CHARTS[6].call = VS_OPEN_CALL[0][0];
        CHARTS[7].raise = VS_OPEN_3B[1][0]; CHARTS[7].call = VS_OPEN_CALL[1][0];
        CHARTS[8].raise = VS_OPEN_3B[1][1]; CHARTS[8].call = VS_OPEN_CALL[1][1];
        CHARTS[9].raise = VS_OPEN_3B[0][2]; CHARTS[9].call = VS_OPEN_CALL[0][2];
        CHARTS[10].raise = VS_OPEN_3B[1][2]; CHARTS[10].call = VS_OPEN_CALL[1][2];
        CHARTS[11].raise = VS3B_4B; CHARTS[11].call = VS3B_CALL_IP;
        CHARTS[12].raise = VS3B_4B; CHARTS[12].call = VS3B_CALL_OOP;
    }
    return &CHARTS[i < 0 ? 0 : i >= chartCount() ? chartCount() - 1 : i];
}

const char *qualityName(int q) {
    static const char *N[5] = { "Best", "Good", "Inaccuracy", "Mistake", "Blunder" };
    return q >= 0 && q < 5 ? N[q] : "";
}

// ---------------------------------------------------------------- helpers

typedef struct {
    const Game *g;
    int p;
    const Seat *me;
    Legal l;
    int pot, toCall, bb;
    int winPot;             // what a call can win: the pot, minus any part of a bet bigger than your stack
    float effBB, need;
    int pos;
    bool hu;
    OppRange opp[MAXP];
    int nopp;
} Ctx;

static void why(Advice *a, const char *fmt, ...) {
    if (a->nwhy >= NWHY) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->why[a->nwhy++], sizeof a->why[0], fmt, ap);
    va_end(ap);
}

static const char *money(const Game *g, int v) {
    static char buf[8][48];
    static int k = 0;
    char *b = buf[k++ & 7], c[24];
    fmtChips(v, c, sizeof c);
    float bbs = (float)v / g->bbAmt;
    if (fabsf(bbs - roundf(bbs)) < 0.05f) snprintf(b, 48, "%s (%.0f BB)", c, bbs);
    else snprintf(b, 48, "%s (%.1f BB)", c, bbs);
    return b;
}

static int roundAmt(const Game *g, float x) {
    int unit = g->bbAmt >= 2 ? g->bbAmt / 2 : 1;
    int v = (int)(x / unit + 0.5f) * unit;
    return v < unit ? unit : v;
}

// clamps a raise-to to the legal window; big commitments become all-in
static int legalTo(const Ctx *c, int to) {
    int all = c->me->stack + c->me->bet;
    if (to >= 0.45f * all) to = all;
    if (to > c->l.maxTo) to = c->l.maxTo;
    if (to < c->l.minTo) to = c->l.minTo;
    return to;
}

static int ord(const Game *g, int p) { return (p - g->button - 1 + 2 * g->n) % g->n; }

static void setQ(Advice *a, int fold, int check, int call, int raise) {
    a->q[A_FOLD] = fold; a->q[A_CHECK] = check; a->q[A_CALL] = call; a->q[A_RAISE] = raise;
}

static int clampLegal(const Ctx *c, int to) {
    if (to > c->l.maxTo) to = c->l.maxTo;
    if (to < c->l.minTo) to = c->l.minTo;
    return to;
}

static void recommend(const Ctx *c, Advice *a, int act, int amount) {
    if (act == A_RAISE && !c->l.canRaise) act = c->l.canCheck ? A_CHECK : A_CALL;
    if (amount > 0) amount = clampLegal(c, amount);
    a->act = act;
    a->amount = act == A_RAISE ? amount : 0;
    if (amount > 0) a->aggAmount = amount;
    char m[48];
    bool allin = act == A_RAISE && amount >= c->me->stack + c->me->bet;
    switch (act) {
    case A_FOLD: snprintf(a->head, sizeof a->head, "Fold"); break;
    case A_CHECK: snprintf(a->head, sizeof a->head, "Check"); break;
    case A_CALL: snprintf(a->head, sizeof a->head, "Call %s", money(c->g, c->toCall)); break;
    default:
        snprintf(m, sizeof m, "%s", money(c->g, amount));
        if (allin) snprintf(a->head, sizeof a->head, "All-in for %s", m);
        else snprintf(a->head, sizeof a->head, "%s %s", c->g->curBet == 0 ? "Bet" : "Raise to", m);
    }
}

static void sizeWindow(const Ctx *c, Advice *a, int rec) {
    rec = clampLegal(c, rec);
    a->aggAmount = rec;
    int all = c->me->stack + c->me->bet;
    a->lo = clampLegal(c, rec >= all ? (int)(all * 0.7f) : (int)(rec * 0.7f));
    a->hi = clampLegal(c, (int)(rec * 1.5f) + c->bb / 2);
    if (a->hi < rec) a->hi = rec;
    if (a->lo > rec) a->lo = rec;
}

static const char *posLong(int pos) {
    static const char *N[6] = { "Under the Gun (first to act)", "the Hijack", "the Cutoff", "the Button (last to act after the flop)", "the Small Blind", "the Big Blind" };
    return N[pos];
}

// ---------------------------------------------------------------- opponent ranges

void estimateRanges(const Game *g, int p, OppRange *out, int *n) {
    initCharts();
    *n = 0;
    for (int q = 0; q < g->n; q++) {
        const Seat *s = &g->s[q];
        if (q == p || !s->inHand) continue;
        OppRange *o = &out[(*n)++];
        memset(o, 0, sizeof *o);
        int pos = posIndex(g, q);
        switch (s->pf) {
        case PF_OPEN: o->top = rfiFraction(g, pos); break;
        case PF_3BET: o->top = g->variant == V_OMAHA ? 0.05f : 0.06f; break;
        case PF_CALL: o->top = pos == POS_BB ? 0.55f : 0.28f; o->floor = 0.035f; break;
        case PF_LIMP: o->top = 0.55f; o->floor = 0.06f; break;
        default: o->top = 1.0f; break;
        }
        if (s->allin && (s->pf == PF_OPEN || s->pf == PF_3BET) && g->street == ST_PREFLOP) {    // short-stack shove
            float bbs = (float)s->total / g->bbAmt;
            int sp = g->n == 2 ? POS_SB : pos;
            bool limped = false;
            for (int k = 0; k < g->n; k++) limped |= g->s[k].pf == PF_LIMP;
            if (s->pf == PF_OPEN && g->variant == V_HOLDEM && sp <= POS_SB && bbs <= 25 && !limped) {
                static Range shove[MAXP];               // first in: the solved push range for that seat and depth
                pushRange(sp, bbs, &shove[q]);
                o->set = &shove[q];
                o->top = rangeFraction(&shove[q]);
            } else {
                float wide = fminf(0.6f, fmaxf(0.08f, 0.3f * 10.0f / fmaxf(bbs, 1)));
                o->top = s->pf == PF_OPEN ? wide : fmaxf(o->top, wide);
            }
        }
        if (s->st.hands >= 15 && !o->set) {       // what we've seen from them
            float vpip = (float)s->st.vpip / s->st.hands, pfr = (float)s->st.pfr / s->st.hands;
            if (s->pf == PF_CALL || s->pf == PF_LIMP) o->top = 0.5f * o->top + 0.5f * fminf(1, fmaxf(0.1f, vpip));
            else if (s->pf == PF_OPEN) o->top = 0.5f * o->top + 0.5f * fminf(1, fmaxf(0.06f, pfr * 1.5f));
            else if (s->pf == PF_3BET) o->top = fmaxf(o->top, fminf(0.25f, pfr * 0.4f));
        }
        if (s->pf >= PF_OPEN && (s->callMask & 1))       // raised, then just called a re-raise
            o->top = s->pf == PF_OPEN ? o->top * 0.4f : 0.04f;
        o->aggMask = s->aggMask & ~1;
        o->callMask = s->callMask & ~1;
        o->raiseMask = s->raiseMask & ~1;
    }
}

// ---------------------------------------------------------------- hand descriptions

static void madeHandText(const Game *g, int p, char *buf, int n) {
    const Seat *me = &g->s[p];
    int v = evalPlayer(g->variant, me->hole, g->board, g->nboard), cat = handCat(v);
    char hn[64];
    handName(v, hn, sizeof hn);
    if (g->variant == V_OMAHA) {
        int bv = evalHand(g->board, g->nboard);
        if (cat == HC_TWOPAIR && handCat(bv) == HC_PAIR && ((v >> 12 & 15) == handRank1(bv) || handRank1(v) == handRank1(bv)))
            snprintf(buf, n, "One pair of %s (the other pair is on the board)", rankPlural(handRank1(v) == handRank1(bv) ? (v >> 12 & 15) : handRank1(v)));
        else if (cat == HC_HIGH) snprintf(buf, n, "No made hand yet");
        else snprintf(buf, n, "%s", hn);
        return;
    }
    int h1 = cardRank(me->hole[0]), h2 = cardRank(me->hole[1]);
    int br[5], nb = g->nboard, top = 0, second = -1;
    for (int i = 0; i < nb; i++) { br[i] = cardRank(g->board[i]); if (br[i] > top) top = br[i]; }
    for (int i = 0; i < nb; i++) if (br[i] < top && br[i] > second) second = br[i];
    int bv = evalHand(g->board, nb);
    if (cat >= HC_STRAIGHT && nb == 5 && bv == v) { snprintf(buf, n, "%s (the board - you only tie)", hn); return; }
    if (cat >= HC_STRAIGHT) { snprintf(buf, n, "%s", hn); return; }
    bool boardPaired = handCat(bv) >= HC_PAIR;
    if (cat == HC_TRIPS && handCat(bv) == HC_TRIPS) { snprintf(buf, n, "Trips on the board - everyone has them, your kickers decide"); return; }
    if (cat == HC_TRIPS) {
        if (h1 == h2) snprintf(buf, n, "Set of %s (very strong, well disguised)", rankPlural(h1));
        else snprintf(buf, n, "Trips (%s)", rankPlural(handRank1(v)));
        return;
    }
    if (cat == HC_TWOPAIR) {
        int a = handRank1(v), b = (v >> 12) & 15;
        bool useA = h1 == a || h2 == a, useB = h1 == b || h2 == b;
        if (useA && useB && h1 != h2) snprintf(buf, n, "Two pair (%s and %s), both cards used", rankPlural(a), rankPlural(b));
        else if (h1 == h2) snprintf(buf, n, "Pocket %s on a paired board", rankPlural(h1));
        else if (useA || useB) snprintf(buf, n, "Pair of %s on a paired board", rankPlural(useA ? a : b));
        else snprintf(buf, n, "Two pair on the board - you play your kicker");
        return;
    }
    if (cat == HC_PAIR) {
        int pr = handRank1(v);
        if (h1 == h2) {
            if (h1 > top) snprintf(buf, n, "Overpair (pocket %s)", rankPlural(h1));
            else snprintf(buf, n, "Underpair (pocket %s below the %s)", rankPlural(h1), rankName(top));
            return;
        }
        if (h1 != pr && h2 != pr) { snprintf(buf, n, "No pair of your own (the board is paired)"); return; }
        int kick = h1 == pr ? h2 : h1;
        const char *kq = kick >= 11 ? "top kicker" : kick >= 8 ? "good kicker" : "weak kicker";
        if (pr == top) snprintf(buf, n, "Top pair, %s (%s)", kq, rankPlural(pr));
        else if (pr == second) snprintf(buf, n, "Middle pair (%s)", rankPlural(pr));
        else snprintf(buf, n, "Bottom / weak pair (%s)", rankPlural(pr));
        (void)boardPaired;
        return;
    }
    if (h1 > top && h2 > top) snprintf(buf, n, "No pair, two overcards");
    else if (h1 > top || h2 > top) snprintf(buf, n, "No pair, one overcard");
    else snprintf(buf, n, "No pair (%s high)", rankName(h1 > h2 ? h1 : h2));
}

void describeMadeHand(const Game *g, int p, char *buf, int n) {
    if (g->nboard < 3) { buf[0] = 0; return; }
    madeHandText(g, p, buf, n);
    const Seat *me = &g->s[p];
    int cat = handCat(evalPlayer(g->variant, me->hole, g->board, g->nboard));
    if (cat >= HC_TRIPS) {
        int better = betterCombos(g->variant, me->hole, g->board, g->nboard);
        size_t len = strlen(buf);
        if (better == 0) snprintf(buf + len, n - len, " - the nuts");
        else if (g->variant == V_OMAHA || cat >= HC_STRAIGHT) snprintf(buf + len, n - len, " - not the nuts (%d combos beat you)", better);
    }
}

static void boardNotes(const Game *g, char *buf, int n) {
    int sc[4] = { 0 }, m = 0, cnt[13] = { 0 }, maxSuit = 0;
    for (int i = 0; i < g->nboard; i++) { sc[cardSuit(g->board[i])]++; m |= 1 << cardRank(g->board[i]); cnt[cardRank(g->board[i])]++; }
    for (int s = 0; s < 4; s++) if (sc[s] > maxSuit) maxSuit = sc[s];
    bool straight = false, paired = false;
    for (int lo = -1; lo <= 8; lo++) if (__builtin_popcount(m & (lo < 0 ? 0x100F : 0x1F << lo)) >= 3) straight = true;
    for (int r = 0; r < 13; r++) if (cnt[r] >= 2) paired = true;
    bool om = g->variant == V_OMAHA;
    buf[0] = 0;
    if (maxSuit >= 3) snprintf(buf + strlen(buf), n - strlen(buf), "%s%s", buf[0] ? "; " : "", om ? "three of a suit - flushes are possible" : maxSuit >= 4 ? "four of a suit - any one card of that suit makes a flush" : "three of a suit - flushes are possible");
    else if (maxSuit == 2 && g->nboard < 5) snprintf(buf + strlen(buf), n - strlen(buf), "%sflush draws are possible", buf[0] ? "; " : "");
    if (straight) snprintf(buf + strlen(buf), n - strlen(buf), "%s%s", buf[0] ? "; " : "", g->nboard < 5 ? "connected - straights and straight draws are out there" : "a straight is possible");
    if (paired) snprintf(buf + strlen(buf), n - strlen(buf), "%spaired - full houses are possible", buf[0] ? "; " : "");
}

static bool wetBoard(const Game *g) {
    int sc[4] = { 0 }, m = 0;
    for (int i = 0; i < g->nboard; i++) { sc[cardSuit(g->board[i])]++; m |= 1 << cardRank(g->board[i]); }
    for (int s = 0; s < 4; s++) if (sc[s] >= 2) return true;
    for (int lo = -1; lo <= 8; lo++) {
        int w = lo < 0 ? 0x100F : 0x1F << lo;
        if (__builtin_popcount(m & w) >= 3) return true;
    }
    return false;
}

// ---------------------------------------------------------------- preflop

static const char *TextOr(int others) {
    static char b[48];
    snprintf(b, sizeof b, ", plus %d other player%s in the pot", others, others == 1 ? "" : "s");
    return b;
}

static int oppIndex(const Ctx *c, int seat) {
    int k = 0;
    for (int q = 0; q < seat; q++) if (q != c->p && c->g->s[q].inHand) k++;
    return k;
}

static void preflopByEquity(Ctx *c, Advice *a, int trials, Rng *rng) {
    const Game *g = c->g;
    // only the players already in for the full bet (or all-in); anyone who still owes chips mostly folds
    OppRange in[MAXP];
    int n = 0, k = 0;
    for (int q = 0; q < g->n; q++) {
        if (q == c->p || !g->s[q].inHand) continue;
        if (g->s[q].allin || g->s[q].bet >= g->curBet) in[n++] = c->opp[k];
        k++;
    }
    if (n == 0) { memcpy(in, c->opp, sizeof in); n = c->nopp; }
    int raiser = g->lastRaiser >= 0 ? g->lastRaiser : g->opener;
    float rtop = raiser >= 0 ? c->opp[oppIndex(c, raiser)].top : c->opp[0].top;
    float eq = equityCalc(g->variant, c->me->hole, g->board, 0, in, n, trials, rng);
    a->equity = eq;
    a->hasEquity = true;
    bool shove = raiser >= 0 && g->s[raiser].allin;
    snprintf(a->spot, sizeof a->spot, shove ? "Facing an all-in from %s" : "Big preflop decision - calling commits most of your stack", raiser >= 0 ? g->s[raiser].name : "");
    why(a, "%s, so this is a pure equity decision: compare your equity with the pot odds.", shove ? "They're all-in - no more betting can follow" : "Calling puts most of your stack in");
    why(a, "You need %.0f%% equity. Against their likely range (top ~%.0f%% of hands%s) you have about %.0f%%.", c->need * 100, rtop * 100,
        n > 1 ? TextOr(n - 1) : "", eq * 100);
    int allTo = c->me->stack + c->me->bet;
    sizeWindow(c, a, allTo);
    if (eq >= c->need) {
        recommend(c, a, A_CALL, 0);
        setQ(a, eq >= c->need + 0.1f ? Q_BLUNDER : Q_MISTAKE, -1, Q_BEST, c->l.canRaise ? Q_GOOD : -1);
        why(a, "You have enough equity: calling wins money in the long run even when you lose this pot.");
    } else {
        recommend(c, a, A_FOLD, 0);
        setQ(a, Q_BEST, -1, eq >= c->need - 0.05f ? Q_INACC : eq < c->need * 0.6f ? Q_BLUNDER : Q_MISTAKE, Q_MISTAKE);
        why(a, "Not enough equity for the price. Folding loses nothing more; calling loses chips on average.");
    }
}

// first in with a short stack: the solved (Nash equilibrium) push/fold ranges from tools/nash.c
static void pushFold(Ctx *c, Advice *a, int cls, float top) {
    int sp = c->hu ? POS_SB : c->pos;
    Range pr;
    pushRange(sp, c->effBB, &pr);
    float jam = rangeFraction(&pr), maxBB = PUSH_HALF_BB[sp][cls] / 2.0f;
    bool push = pushHand(sp, cls, c->effBB);
    char cn[4];
    hcClassName(cls, cn);
    int all = c->me->stack + c->me->bet;
    snprintf(a->spot, sizeof a->spot, "Short stack (%.0f BB) - push or fold", c->effBB);
    why(a, "With %.0f BB or less, a normal raise commits you anyway. Good players simply move all-in or fold - it denies your opponents the chance to outplay you after the flop.", c->effBB);
    why(a, "These ranges are solved: at the Nash equilibrium for push/fold, %s shoves the top %.0f%% of hands at %.0f BB%s.",
        c->hu ? "the small blind heads-up" : posLong(c->pos), jam * 100, c->effBB, c->pos == POS_SB && !c->hu ? " (only the big blind is left, so it's very wide)" : "");
    if (maxBB <= 0) why(a, "%s is never a profitable shove from this seat - fold.", cn);
    else if (maxBB >= 25) why(a, "%s is a shove from here at any stack up to 25 BB.", cn);
    else why(a, "%s is a shove from here with up to %.1f BB, a fold with more.", cn, maxBB);
    (void)top;
    sizeWindow(c, a, all);
    if (push) {
        recommend(c, a, A_RAISE, legalTo(c, all));
        setQ(a, maxBB >= c->effBB * 1.4f ? Q_MISTAKE : Q_INACC, -1, Q_MISTAKE, Q_BEST);
    } else {
        recommend(c, a, A_FOLD, 0);
        setQ(a, Q_BEST, -1, Q_MISTAKE, maxBB >= c->effBB * 0.75f ? Q_INACC : Q_MISTAKE);
    }
}

// short stack facing a single open: all-in ("re-steal") if it shows a profit, worked out from fold equity
// and equity when called. Returns false when shoving loses money (the caller decides between call and fold).
static bool restealShove(Ctx *c, Advice *a, int raiser, int trials, Rng *rng, float *evOut) {
    const Game *g = c->g;
    const Seat *me = c->me, *op = &g->s[raiser];
    int all = me->stack + me->bet, oppAll = op->stack + op->bet, eff = all < oppAll ? all : oppAll;
    OppRange cr = c->opp[oppIndex(c, raiser)];
    float openTop = cr.top;
    float callPot = c->pot + (eff - me->bet) + (eff - op->bet);
    // they call with the hands that have the equity they need against a typical re-steal range (~top 15%);
    // fitted to range-vs-range equities: need 30% -> 69% of hands, 35% -> 32%, 40% -> 15%, 45% -> 7.5%
    float oppNeed = (eff - op->bet) / callPot;
    float callAll = fminf(1.0f, 0.075f * expf(14.6f * (0.45f - oppNeed)));
    cr.top = fminf(openTop, callAll);      // the part of their opening range that calls a shove
    cr.set = NULL;
    float k = cr.top / openTop;
    float eqC = equityCalc(g->variant, me->hole, g->board, 0, &cr, 1, trials, rng);
    float pf = 1 - k;
    float ev = pf * c->pot + (1 - pf) * (eqC * callPot - (eff - me->bet));
    *evOut = ev / c->bb;
    a->equity = eqC;
    a->hasEquity = true;
    snprintf(a->spot, sizeof a->spot, "Short stack (%.0f BB) facing an open from %s", c->effBB, posName(g, raiser));
    why(a, "With %.0f BB, calling an open leaves you too short to play well after the flop. The standard play is all-in (a re-steal) or fold.", c->effBB);
    why(a, "%s opens about the top %.0f%% from %s and would call your shove with roughly the top %.0f%%. They fold %.0f%% of the time, and when called you have %.0f%% equity.",
        op->name, openTop * 100, posName(g, raiser), cr.top * 100, pf * 100, eqC * 100);
    why(a, "Shoving %s: %+.1f BB on average compared with folding.", *evOut > 0 ? "wins" : "loses", *evOut);
    if (ev <= 0.1f * c->bb) return false;
    sizeWindow(c, a, all);
    recommend(c, a, A_RAISE, legalTo(c, all));
    setQ(a, *evOut > 1.0f ? Q_MISTAKE : Q_INACC, -1, c->pos == POS_BB ? Q_INACC : Q_MISTAKE, Q_BEST);
    return true;
}

// grades for a "raise / call / fold" chart decision
static void chartGrade(Ctx *c, Advice *a, bool inR, bool inC, float top, float rFrac, float cFrac, int raiseTo, bool raiseIsBluff) {
    sizeWindow(c, a, raiseTo);
    int chk = c->l.canCheck ? Q_BEST : -1;
    if (inR) {
        recommend(c, a, A_RAISE, raiseTo);
        a->bluff = raiseIsBluff;
        if (raiseIsBluff) { a->alt = A_FOLD; a->altFreq = 0.35f; }
        setQ(a, raiseIsBluff ? Q_INACC : top <= 0.05f ? Q_BLUNDER : Q_MISTAKE, c->l.canCheck ? Q_GOOD : -1,
             raiseIsBluff ? Q_INACC : top <= 0.03f ? Q_INACC : Q_GOOD, Q_BEST);
    } else if (inC) {
        recommend(c, a, c->l.canCheck ? A_CHECK : A_CALL, 0);
        setQ(a, top > cFrac * 0.8f + rFrac ? Q_INACC : Q_MISTAKE, chk, Q_BEST, top <= rFrac * 2.0f ? Q_INACC : Q_MISTAKE);
    } else {
        recommend(c, a, c->l.canCheck ? A_CHECK : A_FOLD, 0);
        float edge = rFrac + cFrac;
        setQ(a, Q_BEST, chk, top <= edge * 1.3f ? Q_INACC : top > 0.6f && c->toCall > 2 * c->bb ? Q_BLUNDER : Q_MISTAKE,
             top <= edge * 1.3f ? Q_INACC : Q_MISTAKE);
    }
}

static void preflopHoldem(Ctx *c, Advice *a, int trials, Rng *rng) {
    const Game *g = c->g;
    const Seat *me = c->me;
    int cls = hcClass(me->hole[0], me->hole[1]);
    float top = hcTop[cls];
    char cn[4];
    hcClassName(cls, cn);
    int nr = g->nRaises, limpers = 0, callers = 0, bb = c->bb;
    for (int q = 0; q < g->n; q++)
        if (q != c->p && g->s[q].inHand) { limpers += g->s[q].pf == PF_LIMP; callers += g->s[q].pf == PF_CALL; }
    int lr = g->lastRaiser;
    bool commit = nr > 0 && (c->toCall >= 0.45f * (me->stack + me->bet) || (lr >= 0 && g->s[lr].allin));
    if (commit) { preflopByEquity(c, a, trials, rng); return; }

    if (nr == 0) {
        if (c->pos == POS_BB) {
            Range *iso = c->hu ? &R_ISOHU : &R_ISO;
            snprintf(a->spot, sizeof a->spot, limpers ? "Big blind - %d limper%s, no raise" : "Big blind - the small blind just called", limpers, limpers == 1 ? "" : "s");
            why(a, "Nobody raised, so you can see the flop for free. Check with most hands.");
            why(a, "Raise for value with strong hands (%s): limpers have capped, weak ranges.", c->hu ? "77+, A8s+, ATo+ ..." : "99+, AJs+, KQs, AQo+");
            int to = legalTo(c, roundAmt(g, bb * (c->hu ? 3.0f : 4.0f) + limpers * bb));
            sizeWindow(c, a, to);
            if (iso->in[cls]) { recommend(c, a, A_RAISE, to); setQ(a, Q_BLUNDER, Q_GOOD, -1, Q_BEST); }
            else { recommend(c, a, A_CHECK, 0); setQ(a, Q_BLUNDER, Q_BEST, -1, top <= 0.3f ? Q_INACC : Q_MISTAKE); }
            return;
        }
        if (limpers == 0) {
            if (c->effBB <= 12 && c->l.maxTo >= c->me->stack + c->me->bet) { pushFold(c, a, cls, top); return; }
            Range *R = c->hu ? &R_HU : &R_RFI[c->pos];
            float frac = rangeFraction(R);
            snprintf(a->spot, sizeof a->spot, "Folded to you in %s - unopened pot", posName(g, c->p));
            int to = legalTo(c, roundAmt(g, bb * (c->pos == POS_SB && !c->hu ? 3.0f : c->effBB <= 25 ? 2.2f : 2.5f)));
            why(a, "You are in %s. A solid range opens about %.0f%% of hands from here%s.", posLong(c->pos), frac * 100,
                c->pos <= POS_HJ ? " - tight, because many players still act behind you" : c->pos == POS_BTN ? " - wide, because you'll have position on the blinds" : "");
            if (R->in[cls]) why(a, "%s is in that range: raise to %s. Raising (not limping) wins the blinds right away often and builds a pot with the initiative.", cn, money(g, to));
            else why(a, "%s is not in the opening range here. Fold - in the long run it loses money against the players still to act.", cn);
            chartGrade(c, a, R->in[cls], false, top, frac, 0, to, false);
            if (R->in[cls]) a->q[A_CALL] = Q_INACC;     // open-limp
            return;
        }
        // limpers in front
        Range *iso = &R_RFI[c->pos == POS_SB ? POS_CO : c->pos > 0 ? c->pos - 1 : 0];
        int to = legalTo(c, roundAmt(g, bb * (c->pos == POS_SB ? 4.0f : 3.0f) + limpers * bb));
        snprintf(a->spot, sizeof a->spot, "%d limper%s in front of you", limpers, limpers == 1 ? "" : "s");
        why(a, "Limpers usually hold weak hands. Isolate them with a raise to about %s (3 BB + 1 BB per limper) using a slightly tighter range than a normal open.", money(g, to));
        why(a, "Speculative hands (small pairs, suited aces and connectors) can over-limp: they're cheap and make strong hands that win big multiway pots.");
        bool ov = R_OVL.in[cls];
        chartGrade(c, a, iso->in[cls], ov, top, rangeFraction(iso), rangeFraction(&R_OVL), to, false);
        if (!iso->in[cls] && ov) a->q[A_FOLD] = Q_GOOD;
        return;
    }

    int opPos = posIndex(g, g->opener < 0 ? g->lastRaiser : g->opener);
    bool late = opPos >= POS_CO && opPos <= POS_SB;
    int raiser = g->lastRaiser >= 0 ? g->lastRaiser : g->opener;
    bool ip = ord(g, c->p) > ord(g, raiser);
    if (nr == 1 && callers == 0 && g->limit == BL_NOLIMIT && c->effBB <= 20 && g->s[raiser].stack > 0 && c->l.maxTo >= me->stack + me->bet) {
        float ev;
        if (restealShove(c, a, raiser, trials, rng, &ev)) return;
        // shoving loses: the big blind can still call at a good price with a playable hand, the rest fold
        bool inC = c->pos == POS_BB && top <= (c->hu ? 0.55f : 0.35f) && (float)g->curBet / bb <= 3.0f;
        sizeWindow(c, a, me->stack + me->bet);
        if (inC) {
            recommend(c, a, A_CALL, 0);
            setQ(a, Q_INACC, -1, Q_BEST, Q_MISTAKE);
            why(a, "Shoving doesn't pay, but you're in the big blind closing the action: you need only %.0f%% equity, so call and see a flop.", c->need * 100);
        } else {
            recommend(c, a, A_FOLD, 0);
            setQ(a, Q_BEST, -1, c->pos == POS_BB ? Q_INACC : Q_MISTAKE, ev > -1.0f ? Q_INACC : Q_MISTAKE);
            why(a, "%s isn't strong enough to shove, and calling short-stacked out of the big blind loses money. Fold and wait for a better spot.", cn);
        }
        return;
    }
    if (nr == 1) {
        int k = c->pos == POS_BB ? 2 : c->pos == POS_SB ? 1 : 0;
        Range *r3 = c->hu ? &R_HU3B : &R_3B[late][k];
        bool inC = c->hu ? top <= 0.70f && !r3->in[cls] : R_CALL[late][k].in[cls];
        float cFrac = c->hu ? 0.7f : rangeFraction(&R_CALL[late][k]);
        float openBB = (float)g->curBet / bb;
        if (openBB > 5 && top > 0.12f) inC = false;        // oversized open: tighten up
        int to = legalTo(c, roundAmt(g, (ip ? 3.0f : 4.0f) * g->curBet + callers * g->curBet));
        snprintf(a->spot, sizeof a->spot, "Facing a %.1f BB open from %s%s", openBB, posName(g, raiser), callers ? " (with callers)" : "");
        why(a, "%s opened from %s, %s.", g->s[raiser].name, posName(g, raiser),
            late ? "a late position where players open wide - you can fight back more often" : "an early position, so their range is strong - stay tight");
        if (k == 2) why(a, "In the big blind you already have 1 BB in and close the action: calling costs %s to win %s, so you need only %.0f%% equity. Defend wide.", money(g, c->toCall), money(g, c->winPot), c->need * 100);
        else if (k == 1) why(a, "From the small blind you'll be out of position with the big blind still behind - prefer 3-bet or fold over calling.");
        else if (ip) why(a, "You will have position after the flop, which lets you call with hands that play well (pairs, suited broadways and connectors).");
        why(a, "3-bet to ~%s with value hands (QQ+, AK) plus a few bluffs with blockers like A5s; %s.", money(g, to), callers ? "add 1x the open per caller (a squeeze)" : "use 3x in position, 4x out of position");
        bool bluff = r3->in[cls] && top > 0.08f;
        chartGrade(c, a, r3->in[cls], inC, top, rangeFraction(r3), cFrac, to, bluff);
        if (bluff) why(a, "%s is a 3-bet bluff candidate: it blocks AA/AK-type hands and has playability if called. Mixing it in keeps your 3-bets balanced.", cn);
        else if (r3->in[cls]) why(a, "%s is strong enough to 3-bet for value.", cn);
        else if (inC) why(a, "%s is a good call here.", cn);
        else why(a, "%s is too weak against this open - fold.", cn);
        return;
    }
    if (nr == 2) {
        Range *r4, *rc;
        int to;
        if (me->pf == PF_OPEN) {
            r4 = &R_V3B4;
            rc = ip ? (late ? &R_V3BLATE : &R_V3BIP) : (late ? &R_V3BLOOP : &R_V3BOOP);
            to = legalTo(c, roundAmt(g, (ip ? 2.2f : 2.5f) * g->curBet));
            snprintf(a->spot, sizeof a->spot, "Your open was 3-bet by %s", g->s[raiser].name);
            why(a, "You opened and %s re-raised. A 3-bet range is strong, so continue with roughly the top 8-12%% of hands.", g->s[raiser].name);
            why(a, "4-bet (to ~%s) with QQ+, AK and a few A5s bluffs. Call with hands that play well %s.", money(g, to), ip ? "in position (mid pairs, suited broadways)" : "out of position (only strong ones: TT-JJ, AQ, AJs, KQs)");
        } else {
            r4 = &R_C3B4; rc = &R_C3BC;
            to = legalTo(c, roundAmt(g, 2.3f * g->curBet));
            snprintf(a->spot, sizeof a->spot, "Facing a raise and a 3-bet");
            why(a, "There's an open and a 3-bet in front of you - two strong ranges. Only premium hands continue: 4-bet KK+/AKs, call with QQ-JJ, AK, AQs.");
        }
        bool in4 = r4->in[cls], inC = rc->in[cls];
        float f4 = rangeFraction(r4), fc = rangeFraction(rc);
        if (c->hu && me->pf == PF_OPEN) {          // heads-up ranges are far wider: continue with about half the opens
            in4 = in4 || top <= 0.03f;
            inC = !in4 && top <= 0.38f;
            f4 = 0.035f; fc = 0.35f;
            why(a, "Heads-up both ranges are much wider: the button opens ~80%% and the big blind 3-bets ~15%%, so keep playing roughly half your opens.");
        }
        bool bluff = in4 && top > 0.05f;
        chartGrade(c, a, in4, inC, top, f4, fc, to, bluff);
        return;
    }
    int all = me->stack + me->bet;
    int to = c->effBB <= 60 ? clampLegal(c, all) : legalTo(c, roundAmt(g, 2.2f * g->curBet));
    snprintf(a->spot, sizeof a->spot, "Facing a 4-bet or more");
    why(a, "A 4-bet range is very narrow (QQ+, AK, a few bluffs). Continue only with KK+/AK%s.", c->effBB <= 60 ? " and just get it all in" : "");
    bool inJ = R_V4J.in[cls] || (c->effBB <= 60 && R_V4C.in[cls]);
    chartGrade(c, a, inJ, R_V4C.in[cls] && !inJ, top, rangeFraction(&R_V4J), rangeFraction(&R_V4C), to, false);
}

static void preflopOmaha(Ctx *c, Advice *a, int trials, Rng *rng) {
    const Game *g = c->g;
    const Seat *me = c->me;
    float top = omahaTop(me->hole);
    int nr = g->nRaises, limpers = 0, bb = c->bb;
    for (int q = 0; q < g->n; q++) if (q != c->p && g->s[q].inHand) limpers += g->s[q].pf == PF_LIMP;
    bool pl = g->limit == BL_POTLIMIT;
    int lr = g->lastRaiser;
    bool commit = nr > 0 && (c->toCall >= 0.45f * (me->stack + me->bet) || (lr >= 0 && g->s[lr].allin));
    if (commit) { preflopByEquity(c, a, trials, rng); return; }
    why(a, "Omaha hand quality comes from how the four cards work together: suited aces (nut-flush draws), pairs and connected 'rundowns' are good; disconnected 'danglers' are bad.");
    int potTo = c->l.maxTo;
    if (nr == 0) {
        if (c->pos == POS_BB) {
            snprintf(a->spot, sizeof a->spot, "Big blind - nobody raised");
            int to = legalTo(c, pl ? potTo : roundAmt(g, bb * 4.0f + limpers * bb));
            sizeWindow(c, a, to);
            why(a, "Check and see a free flop; raise only the very best hands (top ~6%%).");
            if (top <= 0.06f) { recommend(c, a, A_RAISE, to); setQ(a, Q_BLUNDER, Q_GOOD, -1, Q_BEST); }
            else { recommend(c, a, A_CHECK, 0); setQ(a, Q_BLUNDER, Q_BEST, -1, top <= 0.2f ? Q_INACC : Q_MISTAKE); }
            return;
        }
        if (limpers == 0) {
            float frac = c->hu ? 0.65f : OM_RFI[c->pos];
            int to = legalTo(c, pl ? potTo : roundAmt(g, bb * 3.0f));
            snprintf(a->spot, sizeof a->spot, "Folded to you in %s - unopened pot", posName(g, c->p));
            why(a, "From %s, open about the top %.0f%% of Omaha hands%s. Your hand is in the top %.0f%%.", posLong(c->pos), frac * 100,
                pl ? " - and raise the pot (the standard PLO open)" : "", top * 100);
            chartGrade(c, a, top <= frac, false, top, frac, 0, to, false);
            if (top <= frac) a->q[A_CALL] = Q_INACC;
            return;
        }
        int to = legalTo(c, pl ? potTo : roundAmt(g, bb * 3.0f + limpers * bb));
        snprintf(a->spot, sizeof a->spot, "%d limper%s in front of you", limpers, limpers == 1 ? "" : "s");
        why(a, "Raise the strongest ~12%% of hands to isolate the limpers; over-limp hands up to ~30%% that play well multiway (suited, connected).");
        chartGrade(c, a, top <= 0.12f, top <= 0.30f, top, 0.12f, 0.18f, to, false);
        return;
    }
    int raiser = g->lastRaiser >= 0 ? g->lastRaiser : g->opener;
    int opPos = posIndex(g, g->opener >= 0 ? g->opener : raiser);
    bool late = opPos >= POS_CO && opPos <= POS_SB;
    bool ip = ord(g, c->p) > ord(g, raiser);
    int to = legalTo(c, pl ? potTo : roundAmt(g, (ip ? 3.0f : 3.5f) * g->curBet));
    if (nr == 1) {
        float r3 = 0.035f, call;
        if (c->pos == POS_BB) call = late ? 0.45f : 0.32f;
        else if (c->pos == POS_SB) call = late ? 0.16f : 0.10f;
        else call = late ? 0.24f : 0.17f;
        snprintf(a->spot, sizeof a->spot, "Facing an open from %s", posName(g, raiser));
        why(a, "Re-raise (3-bet the pot) with the top ~3.5%% - mostly AAxx and great double-suited rundowns. Call up to the top ~%.0f%% here; fold the rest.", call * 100);
        why(a, "Omaha equities run close, so position matters even more than in Hold'em: %s", ip ? "you'll act last - good for calling." : "you'll be out of position - be careful calling.");
        chartGrade(c, a, top <= r3, top <= call, top, r3, call, to, false);
        return;
    }
    if (nr == 2) {
        float r4 = me->pf == PF_OPEN ? 0.012f : 0.008f, call = me->pf == PF_OPEN ? (ip ? 0.09f : 0.06f) : 0.03f;
        snprintf(a->spot, sizeof a->spot, "Facing a 3-bet");
        why(a, "A 3-bet in PLO is usually aces. 4-bet only top AAxx (~1%%); call with strong, well-connected hands (top ~%.0f%%) that can flop big against aces.", call * 100);
        chartGrade(c, a, top <= r4, top <= call, top, r4, call, to, false);
        return;
    }
    snprintf(a->spot, sizeof a->spot, "Facing a 4-bet or more");
    why(a, "A 4-bet means aces almost every time. Continue only with your best AAxx hands.");
    chartGrade(c, a, top <= 0.012f, false, top, 0.012f, 0, to, false);
}

// ---------------------------------------------------------------- postflop

static void postflop(Ctx *c, Advice *a, int trials, Rng *rng) {
    const Game *g = c->g;
    const Seat *me = c->me;
    static const char *SN[4] = { "", "Flop", "Turn", "River" };
    float eq = equityCalc(g->variant, me->hole, g->board, g->nboard, c->opp, c->nopp, trials, rng);
    a->equity = eq;
    a->hasEquity = true;
    int oc[3];
    countOuts(g->variant, me->hole, g->board, g->nboard, oc);
    a->outs = oc[0] + oc[1];
    a->boatOuts = oc[2];
    bool wet = wetBoard(g);
    int street = g->street, nopp = c->nopp;
    float spr = (float)(gameEffStack(g, c->p) - me->bet) / (c->pot > 0 ? c->pot : 1);
    a->spr = spr;
    bool aggr = street == ST_FLOP ? g->pfRaiser == c->p : (me->aggMask >> (street - 1) & 1);
    bool strongDraw = a->outs >= 8 && street < ST_RIVER;
    float raiseEq = nopp == 1 ? (street == ST_RIVER ? 0.75f : 0.70f) : 0.58f;
    // A value bet needs to be ahead of the hands that CALL it, not of everything they might hold:
    // run the equity again against each range filtered as if they called (or continued against a raise).
    OppRange what[MAXP];
    for (int k = 0; k < nopp; k++) {
        what[k] = c->opp[k];
        if (c->l.canCheck) what[k].callMask |= 1 << street;
        else what[k].contMask |= 1 << street;
    }
    float eqIf = equityCalc(g->variant, me->hole, g->board, g->nboard, what, nopp, trials, rng);
    bool ip = inPosition(g, c->p);
    float valueNeed = nopp == 1 ? (ip ? 0.5f : 0.53f) : 1.0f / (nopp + 1) + 0.1f;

    if (a->outs || a->boatOuts) {
        char k[64] = "";
        if (oc[0]) strcat(k, "flush draw");
        if (oc[1]) {
            if (k[0]) strcat(k, " + ");
            strcat(k, oc[1] >= 9 ? "wrap straight draw" : oc[1] >= 8 ? "open-ended straight draw" : "gutshot straight draw");
        }
        if (oc[2]) { if (k[0]) strcat(k, " + "); strcat(k, "full-house outs"); }
        int tot = a->outs + a->boatOuts, unseen = 52 - holeCount(g) - g->nboard;
        float hit = street == ST_FLOP ? 1 - (float)(unseen - tot) * (unseen - tot - 1) / ((float)unseen * (unseen - 1)) : (float)tot / unseen;
        snprintf(a->draws, sizeof a->draws, "%s - %d outs (%.0f%% %s)", k, tot, hit * 100, street == ST_FLOP ? "by the river" : "on the river");
    }
    char rangeTxt[120] = "";
    if (nopp == 1) {
        const OppRange *o = &c->opp[0];
        snprintf(rangeTxt, sizeof rangeTxt, "~top %.0f%% preflop%s", o->top * 100,
                 (o->aggMask | o->callMask) ? ", narrowed by how they've played since" : "");
    } else snprintf(rangeTxt, sizeof rangeTxt, "%d opponents' likely ranges", nopp);

    why(a, "Your hand: %s.%s%s", a->hand, a->draws[0] ? " Draw: " : "", a->draws);
    char bn[160];
    boardNotes(g, bn, sizeof bn);
    if (bn[0]) why(a, "Board: %s.", bn);
    for (int k = 0; k < nopp; k++)
        if (c->opp[k].raiseMask >> street & 1) { why(a, "Careful: a raise after the flop is a strong signal - most players raise with two pair or better, or a big draw."); break; }
    why(a, "Equity vs %s: %.0f%%.%s", rangeTxt, eq * 100, nopp > 1 ? " Multiway pots need stronger hands - someone usually has something." : "");
    if (a->draws[0] && street < ST_RIVER) why(a, "Rule of 4 and 2: outs x 4 on the flop (two cards to come), outs x 2 on the turn, is roughly your chance to hit.");

    if (c->l.canCheck) {
        float f = street == ST_FLOP ? (wet ? 0.66f : g->variant == V_OMAHA ? 0.5f : 0.33f) : street == ST_TURN ? (wet ? 0.75f : 0.66f) : 0.75f;
        int to = legalTo(c, roundAmt(g, f * c->pot));
        int small = legalTo(c, roundAmt(g, 0.33f * c->pot));
        snprintf(a->spot, sizeof a->spot, "%s - no bet yet, %d opponent%s", SN[street], nopp, nopp == 1 ? "" : "s");
        const char *sz = f < 0.4f ? "1/3 pot" : f < 0.7f ? "2/3 pot" : "3/4 pot";
        if (eqIf >= valueNeed) {
            sizeWindow(c, a, to);
            recommend(c, a, A_RAISE, to);
            bool trap = eq >= 0.85f && !wet && street < ST_RIVER;
            setQ(a, Q_BLUNDER, trap || eqIf < valueNeed + 0.06f ? Q_GOOD : Q_INACC, -1, Q_BEST);
            why(a, "Against the hands that would call a bet you still have %.0f%% equity, so worse hands pay you. Bet for value - %s on this %s board%s.",
                eqIf * 100, sz, wet ? "wet" : "dry", wet ? ", big enough to charge the draws" : "");
            if (trap) why(a, "Checking to trap is also fine here: the board is dry, so you give few free cards.");
            if (spr < 1.5f) why(a, "The stack-to-pot ratio is only %.1f - you're happy to get all the chips in.", spr);
        } else if (strongDraw) {
            int big = legalTo(c, roundAmt(g, fmaxf(f, 0.66f) * c->pot));
            sizeWindow(c, a, big);
            recommend(c, a, A_RAISE, big);
            a->bluff = true;
            a->alt = A_CHECK;
            a->altFreq = 0.35f;
            setQ(a, Q_BLUNDER, Q_GOOD, -1, Q_BEST);
            why(a, "Semi-bluff: bet your strong draw. You win when they fold now, and when called you still have %d outs. Betting draws also balances your value bets.", a->outs);
        } else if (aggr && nopp == 1 && street == ST_FLOP && (eq >= 0.30f || !wet)) {
            sizeWindow(c, a, small);
            recommend(c, a, A_RAISE, small);
            a->bluff = eq < 0.5f;
            a->alt = A_CHECK;
            a->altFreq = 0.3f;
            setQ(a, Q_BLUNDER, Q_GOOD, -1, Q_BEST);
            why(a, "Continuation bet: you raised preflop, so your range is stronger on this board. A small c-bet (1/3 pot) works with your whole range %s.", wet ? "when you have some equity" : "on dry boards like this one");
        } else if (aggr && nopp == 1 && street == ST_TURN && (eq >= 0.40f || a->outs >= 4)) {
            sizeWindow(c, a, to);
            recommend(c, a, A_RAISE, to);
            a->bluff = eq < 0.5f;
            a->alt = A_CHECK;
            a->altFreq = 0.4f;
            setQ(a, Q_BLUNDER, Q_GOOD, -1, Q_BEST);
            why(a, "Barrel the turn: you've been the aggressor and still have decent equity, so keep the pressure on their capped range.");
        } else if (street == ST_RIVER && nopp == 1 && eq < 0.25f && aggr) {
            sizeWindow(c, a, to);
            recommend(c, a, A_CHECK, 0);
            a->alt = A_RAISE;
            a->bluff = true;
            a->altFreq = 0.4f;
            a->aggAmount = to;
            setQ(a, Q_BLUNDER, Q_BEST, -1, Q_GOOD);
            why(a, "Your hand can't win at showdown, but you told a strong story: a bluff candidate. With a 3/4-pot bet, about %.0f%% of your river bets should be bluffs "
                "(bet / (pot + 2 x bet)). Bluff with hands that block their calls; give up with the rest.", 0.75f / (1 + 2 * 0.75f) * 100);
        } else if (nopp == 1 && ip && !aggr && street < ST_RIVER && eq >= 0.3f) {
            sizeWindow(c, a, small);
            recommend(c, a, A_CHECK, 0);
            a->alt = A_RAISE;
            a->altFreq = 0.35f;
            a->bluff = eqIf < 0.5f;
            a->aggAmount = small;
            setQ(a, Q_BLUNDER, Q_BEST, -1, Q_GOOD);
            why(a, "They checked to you, which usually caps their range. Checking behind to see a free card is fine; a small stab (~1/3 pot) with some equity is a good mix.");
        } else {
            sizeWindow(c, a, to);
            recommend(c, a, A_CHECK, 0);
            setQ(a, Q_BLUNDER, Q_BEST, -1, nopp > 1 && eq < 0.3f ? Q_MISTAKE : Q_INACC);
            if (eq >= 0.35f) why(a, "Medium strength: check. Against the hands that call you'd have only %.0f%% - betting gets called by better and folds out worse. Keep the pot small and get to showdown.", eqIf * 100);
            else why(a, "Weak hand without a story to tell%s: check and give up unless you improve.", nopp > 1 ? " in a multiway pot" : "");
        }
        return;
    }

    // facing a bet
    int prior = c->pot - c->toCall;
    float betFrac = (float)c->toCall / (prior > 0 ? prior : 1);
    float mdf = prior > 0 ? (float)prior / c->pot : 0.5f;
    bool allinCall = c->toCall >= me->stack;
    float implied = !allinCall && street < ST_RIVER && a->outs >= 4 ? fminf(0.08f, 0.02f * spr) : 0;
    int otherCalls = 0;
    for (int q = 0; q < g->n; q++) if (q != c->p && q != g->lastRaiser) otherCalls += g->s[q].bet;
    int rto = legalTo(c, roundAmt(g, 3.0f * g->curBet + otherCalls));
    sizeWindow(c, a, rto);
    if (g->nRaises >= 2) snprintf(a->spot, sizeof a->spot, "%s - facing a raise to %s", SN[street], money(g, g->curBet));
    else snprintf(a->spot, sizeof a->spot, "%s - facing a %s bet (%.0f%% of the pot)", SN[street], money(g, c->toCall), betFrac * 100);
    why(a, "Pot odds: call %s to win %s -> you need %.0f%% equity.", money(g, c->toCall), money(g, c->winPot), c->need * 100);
    bool valueRaise = nopp == 1 ? eqIf >= 0.55f && eq >= c->need : eq >= raiseEq;
    if (valueRaise && c->l.canRaise) {
        recommend(c, a, A_RAISE, rto);
        setQ(a, Q_BLUNDER, -1, Q_GOOD, Q_BEST);
        if (nopp == 1) why(a, "Even against the hands that would continue after a raise you have %.0f%% equity. Raise for value (to ~3x their bet) - %s.", eqIf * 100,
                           wet ? "and don't give the draws a cheap card" : "calling to trap is also OK on a dry board");
        else why(a, "You're well ahead of their betting ranges. Raise for value (to ~3x their bet) - %s.", wet ? "and don't give the draws a cheap card" : "calling to trap is also OK on a dry board");
    } else if (eq >= c->need + 0.03f) {
        recommend(c, a, A_CALL, 0);
        int rq = strongDraw && nopp == 1 && c->l.canRaise ? Q_GOOD : (nopp == 1 ? eqIf >= 0.45f : eq >= raiseEq - 0.1f) ? Q_INACC : Q_MISTAKE;
        if (rq == Q_GOOD) { a->alt = A_RAISE; a->altFreq = 0.3f; a->bluff = true; }
        setQ(a, eq >= c->need + 0.12f ? Q_BLUNDER : Q_MISTAKE, -1, Q_BEST, c->l.canRaise ? rq : -1);
        why(a, "Your equity beats the price: calling is profitable.%s", rq == Q_GOOD ? " A semi-bluff raise is a fine alternative with this draw." : "");
    } else if (eq + implied >= c->need) {
        recommend(c, a, A_CALL, 0);
        setQ(a, Q_INACC, -1, Q_BEST, c->l.canRaise ? (strongDraw && nopp == 1 ? Q_GOOD : Q_MISTAKE) : -1);
        why(a, "Your raw equity is slightly short, but implied odds make up the gap: with %.1f pot-sized stacks behind you can win more when you hit.", spr);
    } else if (eq >= c->need - 0.03f) {
        recommend(c, a, A_FOLD, 0);
        a->alt = A_CALL;
        a->altFreq = 0.4f;
        setQ(a, Q_BEST, -1, Q_GOOD, c->l.canRaise ? Q_MISTAKE : -1);
        why(a, "Very close spot: your equity is about what the pot odds require. Folding and calling are both reasonable.");
    } else {
        recommend(c, a, A_FOLD, 0);
        int cq = eq >= c->need * 0.75f ? Q_INACC : eq < c->need * 0.5f ? Q_BLUNDER : Q_MISTAKE;
        int rq = strongDraw && nopp == 1 ? Q_INACC : eq < 0.15f ? Q_BLUNDER : Q_MISTAKE;
        setQ(a, Q_BEST, -1, cq, c->l.canRaise ? rq : -1);
        why(a, "Not enough equity for the price - fold. Every call here loses chips on average.");
    }
    if (nopp == 1 && street >= ST_TURN && a->act == A_FOLD)
        why(a, "Theory note: to stop a %.0f%%-pot bet from profiting with any two cards, a player must defend %.0f%% of their range (MDF). Your hand is in the part of your range that folds.", betFrac * 100, mdf * 100);
}

// ---------------------------------------------------------------- quiz-mode checklist

static void lineText(const Game *g, const Seat *s, char *buf, int n) {
    static const char *PF[5] = { "saw a free flop", "limped", "called a raise", "raised", "re-raised" };
    static const char *SN[4] = { "", "flop", "turn", "river" };
    int len = snprintf(buf, n, "%s%s", PF[s->pf], s->pf ? " preflop" : "");
    for (int st = ST_FLOP; st <= g->street && st <= ST_RIVER && len < n; st++) {
        int bit = 1 << st;
        bool now = st == g->street;
        const char *v = s->raiseMask & bit ? "raised" : s->aggMask & bit ? "bet" : s->callMask & bit ? "called" : !now || s->acted ? "checked" : NULL;
        if (v) len += snprintf(buf + len, n - len, ", %s the %s", v, SN[st]);
    }
}

int thinkPrompts(const Game *g, int p, char out[NTHINK][220]) {
    initCharts();
    const Seat *me = &g->s[p];
    Legal l;
    gameLegal(g, &l);
    int n = 0, pot = gamePot(g), behind = 0, live = 0;
    int winPot = gameCallPot(g, p) - l.toCall;      // what a call can win
    for (int q = 0; q < g->n; q++) if (q != p && g->s[q].inHand) live++;
    float eff = (float)gameEffStack(g, p) / g->bbAmt;
    bool ip = inPosition(g, p);
    if (g->street == ST_PREFLOP) {
        for (int q = 0; q < g->n; q++)
            if (q != p && g->s[q].inHand && !g->s[q].acted && !g->s[q].allin) behind++;
        if (behind == 0)
            snprintf(out[n++], 220, "Position: you're in %s and close the action - nobody can raise behind you.", posLong(posIndex(g, p)));
        else
            snprintf(out[n++], 220, "Position: you're in %s, with %d player%s still to act behind you. %s",
                     posLong(posIndex(g, p)), behind, behind == 1 ? "" : "s",
                     behind >= 4 ? "Any of them can wake up with a big hand - how tight should you be?" :
                     behind >= 2 ? "Will you have position on them after the flop?" : "Only one player left - how wide can you play?");
        snprintf(out[n++], 220, "Stack depth: %.0f BB effective.%s", eff,
                 eff <= 12 ? " That's short - is this a push-or-fold spot?" : eff <= 25 ? " Fairly short: no room for fancy plays." : "");
        if (g->nRaises == 0) {
            int limpers = 0;
            for (int q = 0; q < g->n; q++) if (q != p && g->s[q].inHand && g->s[q].pf == PF_LIMP) limpers++;
            if (l.canCheck) snprintf(out[n++], 220, "Nobody raised - you can see the flop for free. Is your hand strong enough to raise for value?");
            else if (limpers) snprintf(out[n++], 220, "%d player%s limped in. Limpers usually hold weak hands - raise to isolate, over-limp, or fold?", limpers, limpers == 1 ? "" : "s");
            else snprintf(out[n++], 220, "Everyone folded to you. Is your hand in a solid opening range for this seat? (Raise or fold - don't limp.)");
        } else {
            int r = g->lastRaiser >= 0 ? g->lastRaiser : g->opener;
            const char *what = g->nRaises == 1 ? "opened" : g->nRaises == 2 ? "3-bet" : "4-bet";
            snprintf(out[n++], 220, "%s (%s) %s to %s. What range of hands does a player raise from that seat?",
                     g->s[r].name, posName(g, r), what, money(g, g->curBet));
            snprintf(out[n++], 220, "The price: %s to call into a pot of %s. What equity do you need, and are you in position after the flop?",
                     money(g, l.toCall), money(g, winPot));
        }
        char cls[64];
        handClassText(g->variant, me->hole, cls, sizeof cls);
        snprintf(out[n++], 220, "Your hand: %s. Does it make strong hands after the flop (pairs, suited, connected), or mostly second-best ones?", cls);
        return n;
    }
    static const char *SN[4] = { "", "flop", "turn", "river" };
    char bn[160];
    boardNotes(g, bn, sizeof bn);
    const char *pfr = g->pfRaiser == p ? "you" : g->pfRaiser >= 0 ? g->s[g->pfRaiser].name : "nobody";
    snprintf(out[n++], 220, "Board: %s. Preflop raiser: %s. Whose range hits this board harder?",
             bn[0] ? bn : "dry - few draws", pfr);
    int shown = 0;
    for (int q = 0; q < g->n && shown < 2; q++) {
        if (q == p || !g->s[q].inHand) continue;
        char ln[150];
        lineText(g, &g->s[q], ln, sizeof ln);
        snprintf(out[n++], 220, "%s (%s) %s. What hands play like that?", g->s[q].name, posName(g, q), ln);
        shown++;
    }
    if (live > shown) snprintf(out[n - 1] + strlen(out[n - 1]), 220 - strlen(out[n - 1]), " (+%d more opponent%s)", live - shown, live - shown == 1 ? "" : "s");
    char hd[110];
    describeMadeHand(g, p, hd, sizeof hd);
    int oc[3];
    int outs = countOuts(g->variant, me->hole, g->board, g->nboard, oc);
    if (outs && g->street < ST_RIVER) snprintf(out[n++], 220, "Your hand: %s, with %d outs to improve. How often will you hit, and how much will you win when you do?", hd, outs);
    else snprintf(out[n++], 220, "Your hand: %s. Which worse hands would pay you, and which better hands are you afraid of?", hd);
    float spr = (float)(gameEffStack(g, p) - me->bet) / (pot > 0 ? pot : 1);
    if (l.toCall > 0)
        snprintf(out[n++], 220, "%s to call into a pot of %s. What equity do you need - and do you have it against their range?", money(g, l.toCall), money(g, winPot));
    else
        snprintf(out[n++], 220, "Pot %s, stack-to-pot %.1f, you're %s. If you bet: is it for value (worse hands call) or as a bluff (better hands fold)? What size does that job?",
                 money(g, pot), spr, ip ? "in position" : "out of position");
    if (g->street < ST_RIVER)
        snprintf(out[n++], 220, "Plan ahead: what will you do on the %s if you're raised, or if a scary card comes?", SN[g->street + 1]);
    return n;
}

// ---------------------------------------------------------------- entry point

void advise(const Game *g, int p, Advice *a, int trials, Rng *rng) {
    initCharts();
    memset(a, 0, sizeof *a);
    a->alt = -1;
    for (int i = 0; i < 4; i++) a->q[i] = -1;
    if (g->toAct != p) return;
    Ctx c = { 0 };
    c.g = g;
    c.p = p;
    c.me = &g->s[p];
    gameLegal(g, &c.l);
    c.pot = gamePot(g);
    c.toCall = c.l.toCall;
    c.bb = g->bbAmt;
    c.effBB = (float)gameEffStack(g, p) / g->bbAmt;
    c.winPot = gameCallPot(g, p) - c.toCall;
    c.need = c.toCall > 0 ? (float)c.toCall / (c.winPot + c.toCall) : 0;
    c.pos = posIndex(g, p);
    c.hu = g->n == 2;
    estimateRanges(g, p, c.opp, &c.nopp);
    a->need = c.need;
    a->nopp = c.nopp;
    a->preflop = g->street == ST_PREFLOP;
    a->openBet = g->curBet == 0;
    a->top = handTop(g->variant, c.me->hole);
    if (a->preflop) {
        char cls[64];
        handClassText(g->variant, c.me->hole, cls, sizeof cls);
        snprintf(a->hand, sizeof a->hand, "%s - top %.1f%% of starting hands", cls, a->top * 100 < 0.1f ? 0.1f : a->top * 100);
        if (g->variant == V_OMAHA) preflopOmaha(&c, a, trials / 3, rng);
        else preflopHoldem(&c, a, trials, rng);
        // position & hand line first
        if (a->nwhy < NWHY) {
            memmove(a->why[1], a->why[0], sizeof a->why[0] * a->nwhy);
            snprintf(a->why[0], sizeof a->why[0], "Your hand: %s. Position: %s, effective stack %.0f BB.", a->hand, posName(g, p), c.effBB);
            a->nwhy++;
        }
    } else {
        describeMadeHand(g, p, a->hand, sizeof a->hand);
        postflop(&c, a, g->variant == V_OMAHA ? trials / 3 : trials, rng);
    }
    if (a->act == A_RAISE && a->amount <= 0) a->amount = c.l.minTo;
    if (a->aggAmount <= 0) a->aggAmount = c.l.minTo;
    if (!c.l.canRaise) a->q[A_RAISE] = -1;
    if (c.l.canCheck) { a->q[A_CALL] = -1; if (a->q[A_FOLD] >= 0 && a->q[A_FOLD] < Q_MISTAKE) a->q[A_FOLD] = Q_MISTAKE; }
    else a->q[A_CHECK] = -1;
    if (a->q[a->act] < 0 || a->q[a->act] > Q_BEST) a->q[a->act] = Q_BEST;
}
