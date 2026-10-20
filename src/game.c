// game.c - dealing, betting rounds, side pots and showdown
#include "game.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *BOT_NAMES[3][5] = {
    { "Lucky Lou", "Penny", "Doug", "Rosie", "Hank" },     // easy
    { "Mia", "Carlos", "Priya", "Owen", "Tess" },          // medium
    { "Viktor", "Shay", "Nadia", "Ronin", "Ines" },        // hard
};

void fmtChips(int v, char *buf, int n) {
    char d[24];
    snprintf(d, sizeof d, "%d", v < 0 ? -v : v);
    int len = (int)strlen(d), o = 0;
    char out[40];
    if (v < 0) out[o++] = '-';
    out[o++] = '$';
    for (int i = 0; i < len; i++) {
        out[o++] = d[i];
        if ((len - i - 1) % 3 == 0 && i < len - 1) out[o++] = ',';
    }
    out[o] = 0;
    snprintf(buf, n, "%s", out);
}

void gameLog(Game *g, const char *fmt, ...) {
    if (g->nlog == LOGN) { memmove(g->log[0], g->log[1], sizeof g->log[0] * (LOGN - 1)); g->nlog--; }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->log[g->nlog++], sizeof g->log[0], fmt, ap);
    va_end(ap);
}

// verb agreement for the log: "You call" / "Mia calls"
static const char *vb(const Game *g, int p, const char *you, const char *they) { return p == g->human ? you : they; }

int holeCount(const Game *g) { return g->variant == V_OMAHA ? 4 : 2; }

int gamePot(const Game *g) {
    int t = 0;
    for (int i = 0; i < g->n; i++) t += g->s[i].total;
    return t;
}

int gameCallPot(const Game *g, int p) {
    const Seat *me = &g->s[p];
    int call = g->curBet - me->bet;
    if (call > me->stack) call = me->stack;
    if (call < 0) call = 0;
    int cap = me->total + call, t = 0;     // nobody can win more from each player than they put in themselves
    for (int q = 0; q < g->n; q++) t += q == p ? cap : g->s[q].total < cap ? g->s[q].total : cap;
    return t;
}

int gameLive(const Game *g) {
    int c = 0;
    for (int i = 0; i < g->n; i++) c += g->s[i].inHand;
    return c;
}

int gameEffStack(const Game *g, int p) {
    int mine = g->s[p].stack + g->s[p].bet, other = 0;
    for (int q = 0; q < g->n; q++)
        if (q != p && g->s[q].inHand && g->s[q].stack + g->s[q].bet > other) other = g->s[q].stack + g->s[q].bet;
    return mine < other ? mine : other;
}

int posIndex(const Game *g, int p) {
    int d = (p - g->button + g->n) % g->n;
    if (g->n == 2) return d == 0 ? POS_BTN : POS_BB;
    if (d == 0) return POS_BTN;
    if (d == 1) return POS_SB;
    if (d == 2) return POS_BB;
    return POS_CO - (g->n - d - 1);
}

const char *posName(const Game *g, int p) {
    static const char *N[6] = { "UTG", "HJ", "CO", "BTN", "SB", "BB" };
    return N[posIndex(g, p)];
}

bool inPosition(const Game *g, int p) {
    int mine = (p - g->button - 1 + 2 * g->n) % g->n;
    for (int q = 0; q < g->n; q++)
        if (q != p && g->s[q].inHand && (q - g->button - 1 + 2 * g->n) % g->n > mine) return false;
    return true;
}

void gameSetup(Game *g, const GameConfig *c) {
    memset(g, 0, sizeof *g);
    g->variant = c->variant;
    g->limit = c->limit;
    g->n = c->n < 2 ? 2 : c->n > MAXP ? MAXP : c->n;
    g->human = c->human;
    g->sbAmt = c->sb;
    g->bbAmt = c->bb;
    g->buyin = c->buyin;
    rngSeed(&g->rng, c->seed);
    int used[3] = { 0 };
    for (int i = 0; i < g->n; i++) {
        Seat *s = &g->s[i];
        s->level = c->levels[i];
        if (i == g->human) snprintf(s->name, sizeof s->name, "You");
        else {
            int lv = s->level < LV_EASY ? LV_MEDIUM : s->level;
            snprintf(s->name, sizeof s->name, "%s", BOT_NAMES[lv - 1][used[lv - 1]++ % 5]);
        }
        s->stack = g->buyin;
        s->buyins = 1;
    }
    g->button = rngInt(&g->rng, g->n);
    g->handNo = 0;
    g->handOver = true;
}

static int nextActor(const Game *g, int from) {
    for (int k = 1; k <= g->n; k++) {
        int q = (from + k) % g->n;
        const Seat *s = &g->s[q];
        if (s->inHand && !s->allin && (!s->acted || s->bet < g->curBet)) return q;
    }
    return -1;
}

static void putIn(Game *g, int p, int amt) {
    Seat *s = &g->s[p];
    if (amt > s->stack) amt = s->stack;
    s->stack -= amt;
    s->bet += amt;
    s->total += amt;
    if (s->stack == 0) s->allin = true;
}

void gameNewHand(Game *g) {
    if (g->handNo > 0) g->button = (g->button + 1) % g->n;
    g->handNo++;
    for (int i = 0; i < g->n; i++) {
        Seat *s = &g->s[i];
        if (s->stack <= 0 && i != g->human) {
            s->stack = g->buyin;
            s->buyins++;
            memset(&s->st, 0, sizeof s->st);
            gameLog(g, "%s reloads to a fresh stack.", s->name);
        }
        s->bet = s->total = 0;
        s->inHand = s->stack > 0;
        s->allin = s->acted = s->shown = false;
        s->faced = -1;
        s->lastAct = -1;
        s->pf = PF_NONE;
        s->aggMask = s->callMask = s->raiseMask = 0;
        s->vpipThis = s->pfrThis = false;
        s->won = 0;
        s->value = 0;
        if (s->inHand) s->st.hands++;
    }
    for (int i = 0; i < 52; i++) g->deck[i] = i;
    shuffle(g->deck, 52, &g->rng);
    g->dpos = 0;
    int hc = holeCount(g);
    for (int k = 0; k < hc; k++)
        for (int i = 0; i < g->n; i++) g->s[(g->button + 1 + i) % g->n].hole[k] = g->deck[g->dpos++];
    g->nboard = 0;
    g->street = ST_PREFLOP;
    g->handOver = g->showdown = false;
    g->npots = 0;
    g->winText[0] = 0;
    g->nActs = 0;
    g->pfCounted = false;
    if (g->n == 2) { g->sbSeat = g->button; g->bbSeat = (g->button + 1) % 2; }
    else { g->sbSeat = (g->button + 1) % g->n; g->bbSeat = (g->button + 2) % g->n; }
    char a[24], b[24];
    fmtChips(g->sbAmt, a, sizeof a);
    fmtChips(g->bbAmt, b, sizeof b);
    gameLog(g, "--- Hand #%d  (%s/%s) ---", g->handNo, a, b);
    putIn(g, g->sbSeat, g->sbAmt);
    putIn(g, g->bbSeat, g->bbAmt);
    g->curBet = g->bbAmt;
    g->minRaise = g->bbAmt;
    g->lastRaiser = g->pfRaiser = g->opener = -1;
    g->nRaises = 0;
    g->toAct = nextActor(g, g->bbSeat);
}

void gameLegal(const Game *g, Legal *l) {
    memset(l, 0, sizeof *l);
    if (g->toAct < 0) return;
    const Seat *s = &g->s[g->toAct];
    l->toCall = g->curBet - s->bet;
    if (l->toCall > s->stack) l->toCall = s->stack;
    l->canCheck = l->toCall == 0;
    int all = s->stack + s->bet;
    bool othersCanAct = false;
    for (int q = 0; q < g->n; q++)
        if (q != g->toAct && g->s[q].inHand && !g->s[q].allin) othersCanAct = true;
    bool reopened = !s->acted || g->curBet - s->faced >= g->minRaise;
    l->canRaise = all > g->curBet && othersCanAct && reopened;
    l->minTo = g->curBet + g->minRaise;
    l->maxTo = all;
    if (g->limit == BL_POTLIMIT) {
        int potTo = g->curBet + gamePot(g) + (g->curBet - s->bet);
        if (potTo < l->maxTo) l->maxTo = potTo;
    }
    if (l->minTo > all) l->minTo = all;
    if (l->maxTo < l->minTo) l->maxTo = l->minTo;
}

// returns the uncalled part of the biggest contribution to its owner
static void refundUncalled(Game *g) {
    int top = -1, second = 0;
    for (int i = 0; i < g->n; i++)
        if (top < 0 || g->s[i].total > g->s[top].total) top = i;
    for (int i = 0; i < g->n; i++)
        if (i != top && g->s[i].total > second) second = g->s[i].total;
    int extra = g->s[top].total - second;
    if (extra > 0) {
        g->s[top].total -= extra;
        g->s[top].stack += extra;
        if (g->s[top].bet >= extra) g->s[top].bet -= extra;
        if (g->s[top].stack > 0) g->s[top].allin = false;
        if (g->s[top].inHand && gameLive(g) > 1) {     // (a pot won without a fight is announced as the win)
            char b[24];
            fmtChips(extra, b, sizeof b);
            gameLog(g, "Uncalled %s returned to %s.", b, top == g->human ? "you" : g->s[top].name);
        }
    }
}

static void awardUncontested(Game *g) {
    refundUncalled(g);
    int w = -1;
    for (int i = 0; i < g->n; i++) if (g->s[i].inHand) w = i;
    int pot = gamePot(g);
    g->s[w].stack += pot;
    g->s[w].won = pot;
    g->npots = 1;
    g->potAmt[0] = pot;
    char b[24];
    fmtChips(pot, b, sizeof b);
    snprintf(g->winText, sizeof g->winText, "%s %s %s", g->s[w].name, w == g->human ? "win" : "wins", b);
    gameLog(g, "%s.", g->winText);
    for (int i = 0; i < g->n; i++) g->s[i].bet = 0;
    g->toAct = -1;
    g->handOver = true;
}

static void doShowdown(Game *g) {
    refundUncalled(g);
    g->showdown = true;
    g->street = ST_SHOWDOWN;
    for (int i = 0; i < g->n; i++) {
        Seat *s = &g->s[i];
        s->bet = 0;
        if (!s->inHand) continue;
        s->shown = true;
        s->value = evalPlayer(g->variant, s->hole, g->board, 5);
        char hn[64];
        handName(s->value, hn, sizeof hn);
        gameLog(g, "%s %s %s.", s->name, vb(g, i, "show", "shows"), hn);
    }
    int levels[MAXP], nl = 0;
    for (int i = 0; i < g->n; i++) {
        if (!g->s[i].inHand) continue;
        int t = g->s[i].total, k = 0;
        while (k < nl && levels[k] < t) k++;
        if (k < nl && levels[k] == t) continue;
        memmove(&levels[k + 1], &levels[k], sizeof(int) * (nl - k));
        levels[k] = t;
        nl++;
    }
    int prev = 0, given = 0, potTotal = gamePot(g);
    g->npots = 0;
    for (int k = 0; k < nl; k++) {
        int L = levels[k], amt = 0;
        for (int i = 0; i < g->n; i++) {
            int t = g->s[i].total;
            amt += (t < L ? t : L) - (t < prev ? t : prev);
        }
        if (k == nl - 1) amt = potTotal - given;    // anything folded above the top live level
        prev = L;
        if (amt <= 0) continue;
        int best = -1, w[MAXP], nw = 0;
        for (int i = 0; i < g->n; i++)
            if (g->s[i].inHand && g->s[i].total >= L && g->s[i].value > best) best = g->s[i].value;
        for (int j = 1; j <= g->n; j++) {             // seat order from the button's left gets odd chips
            int i = (g->button + j) % g->n;
            if (g->s[i].inHand && g->s[i].total >= L && g->s[i].value == best) w[nw++] = i;
        }
        int share = amt / nw, odd = amt - share * nw;
        for (int j = 0; j < nw; j++) {
            int got = share + (j < odd ? 1 : 0);
            g->s[w[j]].stack += got;
            g->s[w[j]].won += got;
        }
        if (g->npots < MAXP) g->potAmt[g->npots++] = amt;
        given += amt;
    }
    // headline: the main-pot winner(s)
    int mainBest = -1, nWin = 0, last = -1;
    for (int i = 0; i < g->n; i++) if (g->s[i].inHand && g->s[i].value > mainBest) mainBest = g->s[i].value;
    for (int i = 0; i < g->n; i++) if (g->s[i].inHand && g->s[i].value == mainBest) { nWin++; last = i; }
    char hn[64], b[24];
    handName(mainBest, hn, sizeof hn);
    if (nWin > 1) snprintf(g->winText, sizeof g->winText, "Split pot - %s", hn);
    else {
        fmtChips(g->s[last].won, b, sizeof b);
        snprintf(g->winText, sizeof g->winText, "%s %s %s with %s", g->s[last].name, last == g->human ? "win" : "wins", b, hn);
    }
    for (int i = 0; i < g->n; i++)
        if (g->s[i].won > 0 && !(nWin == 1 && i == last)) {
            fmtChips(g->s[i].won, b, sizeof b);
            gameLog(g, "%s %s %s.", g->s[i].name, vb(g, i, "collect", "collects"), b);
        }
    gameLog(g, "%s.", g->winText);
    g->toAct = -1;
    g->handOver = true;
}

static void countPreflopStats(Game *g) {
    if (g->pfCounted) return;
    g->pfCounted = true;
    for (int i = 0; i < g->n; i++) { g->s[i].st.vpip += g->s[i].vpipThis; g->s[i].st.pfr += g->s[i].pfrThis; }
}

void gameAct(Game *g, Action a) {
    int p = g->toAct;
    if (p < 0 || g->handOver) return;
    Seat *s = &g->s[p];
    Legal l;
    gameLegal(g, &l);
    if (a.type == A_FOLD && l.canCheck) a.type = A_CHECK;
    if (a.type == A_CHECK && !l.canCheck) a.type = A_CALL;
    if (a.type == A_CALL && l.canCheck) a.type = A_CHECK;
    if (a.type == A_RAISE && !l.canRaise) a.type = l.canCheck ? A_CHECK : A_CALL;
    char b[24];
    bool pre = g->street == ST_PREFLOP;
    switch (a.type) {
    case A_FOLD:
        s->inHand = false;
        gameLog(g, "%s %s.", s->name, vb(g, p, "fold", "folds"));
        break;
    case A_CHECK:
        gameLog(g, "%s %s.", s->name, vb(g, p, "check", "checks"));
        break;
    case A_CALL:
        putIn(g, p, l.toCall);
        fmtChips(l.toCall, b, sizeof b);
        gameLog(g, "%s %s %s%s.", s->name, vb(g, p, "call", "calls"), b, s->allin ? " (all-in)" : "");
        s->callMask |= 1 << g->street;
        s->st.calls++;
        if (pre) {
            if (s->pf < PF_OPEN) s->pf = g->nRaises == 0 ? PF_LIMP : PF_CALL;
            s->vpipThis = true;
        }
        break;
    case A_RAISE: {
        int to = a.amount;
        if (to < l.minTo) to = l.minTo;
        if (to > l.maxTo) to = l.maxTo;
        int size = to - g->curBet;
        bool wasBet = g->curBet == 0;
        putIn(g, p, to - s->bet);
        if (size >= g->minRaise) {
            g->minRaise = size;
            for (int q = 0; q < g->n; q++) if (q != p) g->s[q].acted = false;
        }
        g->curBet = s->bet;
        g->lastRaiser = p;
        g->nRaises++;
        s->aggMask |= 1 << g->street;
        if (!wasBet && !pre) s->raiseMask |= 1 << g->street;
        s->st.bets++;
        if (pre) {
            g->pfRaiser = p;
            if (g->opener < 0) g->opener = p;
            s->pf = g->nRaises == 1 ? PF_OPEN : PF_3BET;
            s->vpipThis = s->pfrThis = true;
        }
        s->lastBet = wasBet;
        fmtChips(s->bet, b, sizeof b);
        gameLog(g, "%s %s %s%s.", s->name, wasBet ? vb(g, p, "bet", "bets") : vb(g, p, "raise to", "raises to"), b, s->allin ? " (all-in)" : "");
        break;
    }
    }
    s->lastAct = a.type;
    s->acted = true;
    s->faced = g->curBet;
    g->nActs++;

    if (gameLive(g) == 1) {
        countPreflopStats(g);
        awardUncontested(g);
        return;
    }
    g->toAct = nextActor(g, p);
}

void gameAdvance(Game *g) {
    if (g->handOver || g->toAct >= 0) return;
    if (g->street == ST_PREFLOP) countPreflopStats(g);
    refundUncalled(g);          // a bet nobody could match goes back now, not at showdown
    for (int i = 0; i < g->n; i++) { g->s[i].bet = 0; g->s[i].acted = false; g->s[i].faced = -1; g->s[i].lastAct = -1; }
    if (g->street >= ST_RIVER) { doShowdown(g); return; }
    static const char *SN[4] = { "", "Flop", "Turn", "River" };
    int add = g->street == ST_PREFLOP ? 3 : 1;
    for (int k = 0; k < add; k++) g->board[g->nboard++] = g->deck[g->dpos++];
    g->street++;
    char bs[24] = "", c[3];
    for (int i = 0; i < g->nboard; i++) { cardStr(g->board[i], c); strcat(bs, c); strcat(bs, " "); }
    char pb[24];
    fmtChips(gamePot(g), pb, sizeof pb);
    gameLog(g, "%s: %s (pot %s)", SN[g->street], bs, pb);
    g->curBet = 0;
    g->minRaise = g->bbAmt;
    g->lastRaiser = -1;
    g->nRaises = 0;
    int able = 0;
    for (int i = 0; i < g->n; i++) able += g->s[i].inHand && !g->s[i].allin;
    if (able <= 1) {
        for (int i = 0; i < g->n; i++) if (g->s[i].inHand) g->s[i].shown = true;
        g->toAct = -1;
        return;
    }
    g->toAct = nextActor(g, g->button);
}
