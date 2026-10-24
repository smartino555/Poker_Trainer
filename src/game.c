// game.c - dealing, betting rounds (no-limit, pot-limit, fixed limit), stud bring-ins, draws,
// side pots, hi-lo splits and showdown
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

static void event(Game *g, int type, int seat, int a, int b) {
    g->ev[g->evSeq % MAXEV] = (GameEvent){ type, seat, a, b };
    g->evSeq++;
}

// verb agreement for the log: "You call" / "Mia calls"
static const char *vb(const Game *g, int p, const char *you, const char *they) { return p == g->human ? you : they; }

static const VariantInfo *VI(const Game *g) { return varInfo(g->variant); }
static bool isStud(const Game *g) { return VI(g)->family == FAM_STUD; }
bool gameIsFixed(const Game *g) { return g->limit == BL_FIXED; }

int holeCount(const Game *g) { return g->ncards; }

const char *streetName(const Game *g, int street) {
    if (street < 0 || street >= VI(g)->rounds) return "Showdown";
    return VI(g)->streets[street];
}

int gamePot(const Game *g) {
    int t = 0;
    for (int i = 0; i < g->n; i++) t += g->s[i].total;
    return t;
}

int gameCallPot(const Game *g, int p) {
    const Seat *me = &g->s[p];
    int call = g->curBet - me->bet;
    if (g->bringInPending && p == g->bringInSeat) call = g->bringIn;
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

int gameUpCards(const Game *g, int p, int *out) {
    const Seat *s = &g->s[p];
    int n = 0;
    for (int k = 0; k < s->nhole; k++) if (s->upMask >> k & 1) out[n++] = s->hole[k];
    return n;
}

// stud: the best hand showing acts first (the lowest in Razz). Straights and flushes don't count with
// fewer than five cards up; ties go to the first player clockwise from the dealer.
int gameBestShowing(const Game *g) {
    int best = -1, bv = -1;
    for (int k = 1; k <= g->n; k++) {
        int q = (g->button + k) % g->n;
        if (!g->s[q].inHand) continue;
        int up[MAXH], n = gameUpCards(g, q, up);
        int v = showingValue(g->variant, up, n);
        if (v > bv) { bv = v; best = q; }
    }
    return best;
}

int gameSeatValue(const Game *g, int p, int *low) {
    const Seat *s = &g->s[p];
    if (low) *low = 0;
    if (s->nhole == 0) return 0;
    if (varHasBoard(g->variant) && g->nboard < 3) return 0;
    return evalCards(g->variant, s->hole, s->nhole, g->board, g->nboard, low);
}

int gameCapLeft(const Game *g) {
    if (!gameIsFixed(g) || g->capOff) return -1;
    int left = 4 - g->nBets;
    return left < 0 ? 0 : left;
}

// ---------------------------------------------------------------- setup

// stud's forced bets scale with the stakes: an ante of about a fifth and a bring-in of about a third of the small bet
static void applyStakes(Game *g) {
    g->smallBet = g->bbAmt;
    g->bigBet = 2 * g->bbAmt;
    g->ante = isStud(g) ? (g->bbAmt + 2) / 5 : 0;
    if (isStud(g) && g->ante < 1) g->ante = 1;
    g->bringIn = isStud(g) ? (g->bbAmt + 1) / 3 : 0;
    if (isStud(g) && g->bringIn < 1) g->bringIn = 1;
}

void gameSetup(Game *g, const GameConfig *c) {
    memset(g, 0, sizeof *g);
    g->variant = c->variant;
    g->limit = c->limit;
    g->mix = c->mix;
    if (g->mix) {
        const MixGame *m;
        if (mixGames(g->mix, &m) > 0) { g->variant = m[0].variant; g->limit = m[0].limit; }
        else g->mix = MIX_NONE;
    }
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
    g->toAct = -1;
    applyStakes(g);
}

static int nextActor(const Game *g, int from) {
    for (int k = 1; k <= g->n; k++) {
        int q = (from + k) % g->n;
        const Seat *s = &g->s[q];
        if (s->inHand && !s->allin && (!s->acted || s->bet < g->curBet)) return q;
    }
    return -1;
}

static int nextDrawer(const Game *g, int from) {
    for (int k = 1; k <= g->n; k++) {
        int q = (from + k) % g->n;
        if (g->s[q].inHand && !g->s[q].acted) return q;
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

static void postAnte(Game *g, int p, int amt) {      // dead money: in the pot, but not part of the street's bet
    Seat *s = &g->s[p];
    if (amt > s->stack) amt = s->stack;
    s->stack -= amt;
    s->total += amt;
    if (s->stack == 0) s->allin = true;
}

static int dealCard(Game *g, int p);

static void dealTo(Game *g, int p, bool up) {
    Seat *s = &g->s[p];
    if (up) s->upMask |= 1 << s->nhole;
    s->hole[s->nhole++] = dealCard(g, p);
}

// Stud bring-in: the lowest up card (Razz: the highest, aces low). A tie on rank goes by suit,
// clubs lowest, then diamonds, hearts, spades.
static int findBringIn(const Game *g, bool *suitTie) {
    bool razz = g->variant == V_RAZZ;
    int best = -1, bestKey = 0;
    for (int q = 0; q < g->n; q++) {
        if (!g->s[q].inHand) continue;
        int c = g->s[q].hole[2];
        int key = razz ? ((cardRank(c) + 1) % 13) * 4 + cardSuit(c) : -(cardRank(c) * 4 + cardSuit(c));
        // stud: lowest rank, then lowest suit -> biggest key; razz: highest (ace-low) rank, then highest suit
        if (best < 0 || key > bestKey) { best = q; bestKey = key; }
    }
    *suitTie = false;
    for (int q = 0; q < g->n; q++)
        if (q != best && g->s[q].inHand && best >= 0 && cardRank(g->s[q].hole[2]) == cardRank(g->s[best].hole[2])) *suitTie = true;
    return best;
}

static void rotateMix(Game *g) {
    const MixGame *m;
    int cnt = mixGames(g->mix, &m);
    if (cnt <= 0) return;
    if (++g->mixHands < g->n) return;         // one orbit of each game
    g->mixHands = 0;
    g->mixIdx = (g->mixIdx + 1) % cnt;
    g->variant = m[g->mixIdx].variant;
    g->limit = m[g->mixIdx].limit;
    gameLog(g, "=== Next game: %s %s ===", limitName(g->limit), varInfo(g->variant)->name);
    event(g, EV_GAME, -1, g->variant, g->limit);
}

static void startRound(Game *g);

void gameNewHand(Game *g) {
    if (g->mix && g->handNo > 0) rotateMix(g);
    if (g->handNo > 0) g->button = (g->button + 1) % g->n;
    g->handNo++;
    applyStakes(g);
    const VariantInfo *vi = VI(g);
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
        s->value = s->low = 0;
        s->nhole = 0;
        s->upMask = 0;
        s->drew[0] = s->drew[1] = s->drew[2] = -1;
        if (s->inHand) s->st.hands++;
    }
    g->deckN = 0;
    for (int c = 0; c < 52; c++) if (vi->deck == 52 || cardRank(c) >= 4) g->deck[g->deckN++] = c;
    shuffle(g->deck, g->deckN, &g->rng);
    for (int i = 0; i < g->npreset; i++)             // rigged deck (tests): move the preset cards to the top
        for (int j = i; j < g->deckN; j++)
            if (g->deck[j] == g->preset[i]) { g->deck[j] = g->deck[i]; g->deck[i] = g->preset[i]; break; }
    g->npreset = 0;
    g->dpos = 0;
    g->ndisc = 0;
    g->nboard = 0;
    g->street = ST_PREFLOP;
    g->phase = PH_BET;
    g->drawNo = 0;
    g->handOver = g->showdown = false;
    g->npots = 0;
    g->winText[0] = 0;
    g->nActs = 0;
    g->pfCounted = false;
    g->lastRaiser = g->pfRaiser = g->opener = -1;
    g->nRaises = 0;
    g->bringInPending = false;
    g->bringInSeat = -1;
    g->openPair = false;
    event(g, EV_HAND, -1, g->handNo, 0);
    char a[24], b[24];
    if (isStud(g)) {
        char c[24];
        fmtChips(g->ante, a, sizeof a);
        fmtChips(g->smallBet, b, sizeof b);
        fmtChips(g->bigBet, c, sizeof c);
        gameLog(g, "--- Hand #%d  (%s/%s, ante %s) ---", g->handNo, b, c, a);
        g->sbSeat = g->bbSeat = -1;
        for (int i = 0; i < g->n; i++) if (g->s[i].inHand) postAnte(g, i, g->ante);
        event(g, EV_ANTES, -1, g->ante, 0);
        for (int k = 0; k < 3; k++)
            for (int i = 1; i <= g->n; i++) {
                int q = (g->button + i) % g->n;
                if (g->s[q].inHand) dealTo(g, q, k == 2);
            }
        g->ncards = 3;
        event(g, EV_DEAL, -1, 2, 1);
        startRound(g);
        return;
    }
    int hc = vi->nhole;
    for (int k = 0; k < hc; k++)
        for (int i = 1; i <= g->n; i++) {
            int q = (g->button + i) % g->n;
            if (g->s[q].inHand) dealTo(g, q, false);
        }
    g->ncards = hc;
    if (g->n == 2) { g->sbSeat = g->button; g->bbSeat = (g->button + 1) % 2; }
    else { g->sbSeat = (g->button + 1) % g->n; g->bbSeat = (g->button + 2) % g->n; }
    fmtChips(g->sbAmt, a, sizeof a);
    fmtChips(g->bbAmt, b, sizeof b);
    if (gameIsFixed(g)) {
        char c[24];
        fmtChips(g->bigBet, c, sizeof c);
        gameLog(g, "--- Hand #%d  (%s/%s limit, blinds %s/%s) ---", g->handNo, b, c, a, b);
    } else gameLog(g, "--- Hand #%d  (%s/%s) ---", g->handNo, a, b);
    putIn(g, g->sbSeat, g->sbAmt);
    putIn(g, g->bbSeat, g->bbAmt);
    event(g, EV_BLINDS, g->sbSeat, g->bbSeat, 0);
    event(g, EV_DEAL, -1, hc, 0);
    g->curBet = g->bbAmt;
    g->minRaise = g->bbAmt;
    g->betUnit = g->smallBet;
    g->nBets = 1;                 // the big blind is the first bet: three raises are left before the cap
    g->capOff = gameLive(g) == 2;
    g->toAct = nextActor(g, g->bbSeat);
    event(g, EV_STREET, g->toAct, 0, -1);
}

// ---------------------------------------------------------------- the deck

static void reshuffle(Game *g, int p) {
    // the drawing player never gets their own discards from this draw back
    int keep = 0, got = 0;
    g->deckN = 0;
    g->dpos = 0;
    for (int i = 0; i < g->ndisc; i++) {
        if (g->discOwner[i] == p && g->discDraw[i] == g->drawNo) {
            g->disc[keep] = g->disc[i]; g->discOwner[keep] = g->discOwner[i]; g->discDraw[keep] = g->discDraw[i];
            keep++;
        } else { g->deck[g->deckN++] = g->disc[i]; got++; }
    }
    g->ndisc = keep;
    shuffle(g->deck, g->deckN, &g->rng);
    gameLog(g, "The deck is used up: %d discards are shuffled into a new deck.", got);
    event(g, EV_RESHUFFLE, p, got, 0);
}

static int dealCard(Game *g, int p) {
    if (g->dpos >= g->deckN) reshuffle(g, p);
    return g->deck[g->dpos++];
}

// ---------------------------------------------------------------- betting

void gameLegal(const Game *g, Legal *l) {
    memset(l, 0, sizeof *l);
    if (g->toAct < 0 || g->handOver) return;
    const Seat *s = &g->s[g->toAct];
    if (g->phase == PH_DRAW) { l->draw = true; return; }
    int all = s->stack + s->bet;
    bool othersCanAct = false;
    for (int q = 0; q < g->n; q++)
        if (q != g->toAct && g->s[q].inHand && !g->s[q].allin) othersCanAct = true;
    if (g->bringInPending && g->toAct == g->bringInSeat) {
        l->bringIn = true;
        l->toCall = g->bringIn < s->stack ? g->bringIn : s->stack;
        l->canRaise = all > g->bringIn && othersCanAct;
        l->minTo = l->maxTo = g->smallBet < all ? g->smallBet : all;
        return;
    }
    l->toCall = g->curBet - s->bet;
    if (l->toCall > s->stack) l->toCall = s->stack;
    l->canCheck = l->toCall == 0;
    bool reopened = !s->acted || g->curBet - s->faced >= g->minRaise;
    l->canRaise = all > g->curBet && othersCanAct && reopened;
    if (gameIsFixed(g)) {
        if (!g->capOff && g->nBets >= 4) l->canRaise = false;           // capped: a bet and three raises
        int target = g->curBet < g->betUnit ? g->betUnit : g->curBet + g->betUnit;
        l->minTo = l->maxTo = target;
        if (g->openPair && g->curBet == 0) l->maxTo = g->bigBet;         // 4th street open pair: small or big bet
        if (l->minTo > all) l->minTo = all;
        if (l->maxTo > all) l->maxTo = all;
        return;
    }
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
        else g->s[top].bet = 0;
        if (g->s[top].stack > 0) g->s[top].allin = false;
        if (g->s[top].inHand && gameLive(g) > 1) {     // (a pot won without a fight is announced as the win)
            char b[24];
            fmtChips(extra, b, sizeof b);
            gameLog(g, "Uncalled %s returned to %s.", b, top == g->human ? "you" : g->s[top].name);
            event(g, EV_UNCALLED, top, extra, 0);
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
    g->pots[0] = (PotResult){ pot, pot, 0, 1 << w, 0, 1 << w };
    char b[24];
    fmtChips(pot, b, sizeof b);
    snprintf(g->winText, sizeof g->winText, "%s %s %s", g->s[w].name, w == g->human ? "win" : "wins", b);
    gameLog(g, "%s.", g->winText);
    event(g, EV_WALK, w, pot, 0);
    for (int i = 0; i < g->n; i++) g->s[i].bet = 0;
    g->toAct = -1;
    g->handOver = true;
}

// splits amt between the winners in mask; odd chips go to the first winners left of the button
static void payOut(Game *g, int mask, int amt) {
    int w[MAXP], nw = 0;
    for (int j = 1; j <= g->n; j++) {
        int i = (g->button + j) % g->n;
        if (mask >> i & 1) w[nw++] = i;
    }
    if (!nw) return;
    int share = amt / nw, odd = amt - share * nw;
    for (int j = 0; j < nw; j++) {
        int got = share + (j < odd ? 1 : 0);
        g->s[w[j]].stack += got;
        g->s[w[j]].won += got;
    }
}

static void doShowdown(Game *g) {
    refundUncalled(g);
    g->showdown = true;
    g->street = ST_SHOWDOWN;
    bool split = varIsSplit(g->variant);
    for (int i = 0; i < g->n; i++) {
        Seat *s = &g->s[i];
        s->bet = 0;
        if (!s->inHand) continue;
        s->shown = true;
        s->value = evalCards(g->variant, s->hole, s->nhole, g->board, g->nboard, &s->low);
        char hn[64], ln[40] = "";
        handNameV(g->variant, s->value, hn, sizeof hn);
        if (split) { if (s->low) lowName(s->low, ln, sizeof ln); else snprintf(ln, sizeof ln, "no low"); }
        if (split) gameLog(g, "%s %s %s / %s.", s->name, vb(g, i, "show", "shows"), hn, ln);
        else gameLog(g, "%s %s %s.", s->name, vb(g, i, "show", "shows"), hn);
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
        int best = -1, bestLow = 0, elig = 0, hiMask = 0, loMask = 0;
        for (int i = 0; i < g->n; i++) {
            if (!g->s[i].inHand || g->s[i].total < L) continue;
            elig |= 1 << i;
            if (g->s[i].value > best) best = g->s[i].value;
            if (split && g->s[i].low > bestLow) bestLow = g->s[i].low;
        }
        for (int i = 0; i < g->n; i++) {
            if (!(elig >> i & 1)) continue;
            if (g->s[i].value == best) hiMask |= 1 << i;
            if (bestLow && g->s[i].low == bestLow) loMask |= 1 << i;
        }
        int loAmt = loMask ? amt / 2 : 0, hiAmt = amt - loAmt;     // the odd chip goes to the high hand
        payOut(g, hiMask, hiAmt);
        payOut(g, loMask, loAmt);
        if (g->npots < MAXP) {
            g->pots[g->npots] = (PotResult){ amt, hiAmt, loAmt, hiMask, loMask, elig };
            g->potAmt[g->npots++] = amt;
        }
        given += amt;
    }
    // headline: the main-pot winner(s)
    const PotResult *mp = &g->pots[0];
    char hn[64], ln[40], b[24];
    int nHi = __builtin_popcount(mp->hiMask), nLo = __builtin_popcount(mp->loMask);
    int hiW = __builtin_ctz(mp->hiMask), loW = mp->loMask ? __builtin_ctz(mp->loMask) : -1;
    handNameV(g->variant, g->s[hiW].value, hn, sizeof hn);
    if (split && mp->loMask) {
        lowName(g->s[loW].low, ln, sizeof ln);
        if (nHi == 1 && nLo == 1 && hiW == loW) {
            fmtChips(g->s[hiW].won, b, sizeof b);
            snprintf(g->winText, sizeof g->winText, "%s %s %s - high and low (%s, %s)", g->s[hiW].name, hiW == g->human ? "scoop" : "scoops", b, hn, ln);
        } else
            snprintf(g->winText, sizeof g->winText, "High: %s (%s)  -  Low: %s (%s)",
                     nHi > 1 ? "split" : g->s[hiW].name, hn, nLo > 1 ? "split" : g->s[loW].name, ln);
    } else if (nHi > 1) snprintf(g->winText, sizeof g->winText, "Split pot - %s", hn);
    else {
        fmtChips(g->s[hiW].won, b, sizeof b);
        snprintf(g->winText, sizeof g->winText, "%s %s %s with %s%s", g->s[hiW].name, hiW == g->human ? "win" : "wins", b, hn,
                 split ? " (no low qualifies)" : "");
    }
    bool named = nHi == 1 && (!split || !mp->loMask || (nLo == 1 && loW == hiW));   // the headline gave the amount
    for (int i = 0; i < g->n; i++)
        if (g->s[i].won > 0 && !(named && i == hiW)) {
            fmtChips(g->s[i].won, b, sizeof b);
            gameLog(g, "%s %s %s.", g->s[i].name, vb(g, i, "collect", "collects"), b);
        }
    gameLog(g, "%s.", g->winText);
    event(g, EV_SHOWDOWN, -1, g->npots, 0);
    g->toAct = -1;
    g->handOver = true;
}

static void countPreflopStats(Game *g) {
    if (g->pfCounted) return;
    g->pfCounted = true;
    for (int i = 0; i < g->n; i++) { g->s[i].st.vpip += g->s[i].vpipThis; g->s[i].st.pfr += g->s[i].pfrThis; }
}

static void doDraw(Game *g, int p, int mask) {
    Seat *s = &g->s[p];
    mask &= (1 << s->nhole) - 1;
    int n = __builtin_popcount(mask);
    for (int k = 0; k < s->nhole; k++) {           // discard first, so a reshuffle can't hand a card back
        if (!(mask >> k & 1)) continue;
        g->disc[g->ndisc] = s->hole[k];
        g->discOwner[g->ndisc] = p;
        g->discDraw[g->ndisc] = g->drawNo;
        g->ndisc++;
    }
    for (int k = 0; k < s->nhole; k++) if (mask >> k & 1) s->hole[k] = dealCard(g, p);
    s->drew[g->drawNo] = n;
    s->lastAct = A_DRAW;
    s->acted = true;
    if (n == 0) gameLog(g, "%s %s.", s->name, vb(g, p, "stand pat", "stands pat"));
    else gameLog(g, "%s %s %d.", s->name, vb(g, p, "draw", "draws"), n);
    event(g, EV_DRAW, p, n, 0);
    g->nActs++;
    g->toAct = nextDrawer(g, p);
}

void gameAct(Game *g, Action a) {
    int p = g->toAct;
    if (p < 0 || g->handOver) return;
    Seat *s = &g->s[p];
    if (g->phase == PH_DRAW) { doDraw(g, p, a.type == A_DRAW ? a.amount : 0); return; }
    Legal l;
    gameLegal(g, &l);
    if (a.type == A_DRAW) a.type = A_CHECK;
    char b[24];
    bool pre = g->street == ST_PREFLOP;
    if (l.bringIn) {                                // the bring-in is forced: post it, or complete
        bool complete = a.type == A_RAISE && l.canRaise;
        putIn(g, p, complete ? l.minTo : l.toCall);
        g->curBet = s->bet;
        g->bringInPending = false;
        fmtChips(s->bet, b, sizeof b);
        if (complete) {
            g->nBets = 1;
            g->nRaises = 1;
            g->lastRaiser = g->pfRaiser = g->opener = p;
            g->minRaise = g->betUnit;
            s->aggMask |= 1;
            s->pf = PF_OPEN;
            s->vpipThis = s->pfrThis = true;
            s->st.bets++;
            s->lastAct = A_RAISE;
            s->lastBet = true;
            gameLog(g, "%s %s to %s%s.", s->name, vb(g, p, "complete", "completes"), b, s->allin ? " (all-in)" : "");
        } else {
            s->lastAct = A_CALL;
            gameLog(g, "%s %s for %s%s.", s->name, vb(g, p, "bring in", "brings in"), b, s->allin ? " (all-in)" : "");
        }
        event(g, EV_BRINGIN_ACT, p, complete, s->bet);
        if (s->allin) event(g, EV_ALLIN, p, s->total, 0);
        s->acted = true;
        s->faced = g->curBet;
        g->nActs++;
        g->toAct = nextActor(g, p);
        return;
    }
    if (a.type == A_FOLD && l.canCheck) a.type = A_CHECK;
    if (a.type == A_CHECK && !l.canCheck) a.type = A_CALL;
    if (a.type == A_CALL && l.canCheck) a.type = A_CHECK;
    if (a.type == A_RAISE && !l.canRaise) a.type = l.canCheck ? A_CHECK : A_CALL;
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
        if (s->allin) event(g, EV_ALLIN, p, s->total, 0);
        s->callMask |= 1 << g->street;
        s->st.calls++;
        if (pre) {
            if (s->pf < PF_OPEN) s->pf = g->nRaises == 0 ? PF_LIMP : PF_CALL;
            s->vpipThis = true;
        }
        break;
    case A_RAISE: {
        int to = a.amount;
        bool fixed = gameIsFixed(g);
        if (fixed && g->openPair && g->curBet == 0 && l.maxTo > l.minTo) {
            to = to >= l.maxTo ? l.maxTo : l.minTo;
            if (to == g->bigBet) g->betUnit = g->bigBet;   // once the big bet is made, raises are big bets too
        } else if (fixed) to = l.maxTo;
        if (to < l.minTo) to = l.minTo;
        if (to > l.maxTo) to = l.maxTo;
        int size = to - g->curBet;
        bool wasBet = g->curBet == 0;
        bool completing = !wasBet && g->curBet < g->betUnit;     // stud: completing the bring-in
        int full = fixed ? (g->curBet < g->betUnit ? g->betUnit : g->curBet + g->betUnit) - g->curBet : g->minRaise;
        putIn(g, p, to - s->bet);
        if (size >= full) {
            g->minRaise = fixed ? g->betUnit : size;
            for (int q = 0; q < g->n; q++) if (q != p) g->s[q].acted = false;
            g->nBets++;
        } else if (!wasBet) event(g, EV_SHORTALLIN, p, 0, 0);
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
        gameLog(g, "%s %s %s%s.", s->name, completing ? vb(g, p, "complete to", "completes to") : wasBet ? vb(g, p, "bet", "bets") : vb(g, p, "raise to", "raises to"),
                b, s->allin ? " (all-in)" : "");
        if (s->allin) event(g, EV_ALLIN, p, s->total, 0);
        if (fixed && !g->capOff && g->nBets == 4 && size >= full) event(g, EV_CAP, p, 0, 0);
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

// a new betting round: reset the bets and find who acts first
static void startRound(Game *g) {
    const VariantInfo *vi = VI(g);
    g->curBet = 0;
    g->minRaise = g->bbAmt;
    g->betUnit = gameIsFixed(g) && g->street >= vi->bigFrom ? g->bigBet : g->smallBet;
    if (gameIsFixed(g)) g->minRaise = g->betUnit;
    g->lastRaiser = -1;
    g->nRaises = 0;
    g->nBets = 0;
    g->capOff = gameLive(g) == 2;
    g->openPair = false;
    int best = -1;
    if (isStud(g)) {
        best = gameBestShowing(g);
        if (g->variant == V_STUD && g->street == 1 && gameIsFixed(g))
            for (int q = 0; q < g->n && !g->openPair; q++) {
                if (!g->s[q].inHand) continue;
                int up[MAXH], n = gameUpCards(g, q, up);
                for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++)
                    if (cardRank(up[i]) == cardRank(up[j]) && !g->openPair) { g->openPair = true; event(g, EV_OPENPAIR, q, 0, 0); }
            }
    }
    int able = 0;
    for (int i = 0; i < g->n; i++) able += g->s[i].inHand && !g->s[i].allin;
    if (isStud(g) && g->street == 0) {             // 3rd street: the bring-in acts first
        bool tie;
        int bi = findBringIn(g, &tie);
        g->bringInSeat = bi;
        if (able <= 1) {
            for (int i = 0; i < g->n; i++) if (g->s[i].inHand) g->s[i].shown = true;
            g->toAct = -1;
            event(g, EV_STREET, -1, 0, -1);
            return;
        }
        event(g, EV_BRINGIN, bi, g->s[bi].hole[2], tie);
        if (g->s[bi].allin) g->toAct = nextActor(g, bi);   // all-in from the ante: the next player starts
        else { g->bringInPending = true; g->toAct = bi; }
        event(g, EV_STREET, g->toAct, 0, -1);
        return;
    }
    if (able <= 1) {
        for (int i = 0; i < g->n; i++) if (g->s[i].inHand) g->s[i].shown = true;
        g->toAct = -1;
        event(g, EV_STREET, -1, g->street, best);
        return;
    }
    if (isStud(g)) g->toAct = g->s[best].allin ? nextActor(g, best) : best;
    else g->toAct = nextActor(g, g->button);
    event(g, EV_STREET, g->toAct, g->street, best);
}

void gameAdvance(Game *g) {
    if (g->handOver || g->toAct >= 0) return;
    const VariantInfo *vi = VI(g);
    if (g->phase == PH_DRAW) {                       // everyone has drawn: the next betting round
        g->phase = PH_BET;
        g->drawNo++;
        g->street++;
        for (int i = 0; i < g->n; i++) { g->s[i].acted = false; g->s[i].lastAct = -1; }
        char pb[24];
        fmtChips(gamePot(g), pb, sizeof pb);
        gameLog(g, "%s (pot %s)", vi->streets[g->street], pb);
        startRound(g);
        return;
    }
    if (g->street == ST_PREFLOP) countPreflopStats(g);
    refundUncalled(g);          // a bet nobody could match goes back now, not at showdown
    for (int i = 0; i < g->n; i++) { g->s[i].bet = 0; g->s[i].acted = false; g->s[i].faced = -1; g->s[i].lastAct = -1; }
    if (g->street >= vi->rounds - 1) { doShowdown(g); return; }
    char pb[24];
    fmtChips(gamePot(g), pb, sizeof pb);
    if (vi->family == FAM_DRAW) {
        g->phase = PH_DRAW;
        g->curBet = 0;
        for (int i = 0; i < g->n; i++) g->s[i].acted = false;
        static const char *DN[3] = { "First draw", "Second draw", "Third draw" };
        gameLog(g, "%s (pot %s)", vi->draws == 1 ? "The draw" : DN[g->drawNo], pb);
        event(g, EV_DRAWS, -1, g->drawNo, 0);
        g->toAct = nextDrawer(g, g->button);
        return;
    }
    g->street++;
    if (vi->family == FAM_STUD) {
        bool up = g->street < 4;                     // 4th to 6th street face up, 7th face down
        for (int i = 1; i <= g->n; i++) {
            int q = (g->button + i) % g->n;
            if (g->s[q].inHand) dealTo(g, q, up);
        }
        g->ncards++;
        gameLog(g, "%s (pot %s)", vi->streets[g->street], pb);
    } else {
        int add = g->street == ST_FLOP ? 3 : 1;
        for (int k = 0; k < add; k++) g->board[g->nboard++] = dealCard(g, -1);
        char bs[24] = "", c[3];
        for (int i = 0; i < g->nboard; i++) { cardStr(g->board[i], c); strcat(bs, c); strcat(bs, " "); }
        gameLog(g, "%s: %s(pot %s)", vi->streets[g->street], bs, pb);
    }
    startRound(g);
}
