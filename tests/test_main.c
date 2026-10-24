// test_main.c - unit, scenario and fuzz tests for the engine, coach and bots
// build & run: make test
#include "../src/ai.h"
#include "../src/range.h"
#include "../src/arrange.h"
#include "../src/pushfold.h"
#include "../src/glossary.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int C(const char *s) {   // "As" -> card
    const char *R = "23456789TJQKA", *S = "cdhs";
    return CARD((int)(strchr(R, s[0]) - R), (int)(strchr(S, s[1]) - S));
}
static void cards(const char *s, int *out, int n) {   // "As Kd Qh"
    for (int i = 0; i < n; i++) out[i] = C(s + 3 * i);
}
static int ev(const char *s, int n) { int c[7]; cards(s, c, n); return evalHand(c, n); }

// ---------------------------------------------------------------- evaluator

static void testEvaluator(void) {
    printf("evaluator\n");
    long cnt[9] = { 0 };
    int c[5];
    for (c[0] = 0; c[0] < 52; c[0]++) for (c[1] = c[0] + 1; c[1] < 52; c[1]++) for (c[2] = c[1] + 1; c[2] < 52; c[2]++)
    for (c[3] = c[2] + 1; c[3] < 52; c[3]++) for (c[4] = c[3] + 1; c[4] < 52; c[4]++) cnt[handCat(evalHand(c, 5))]++;
    static const long EXPECT[9] = { 1302540, 1098240, 123552, 54912, 10200, 5108, 3744, 624, 40 };
    for (int i = 0; i < 9; i++) CHECK(cnt[i] == EXPECT[i], "%s count %ld, expected %ld", catName(i), cnt[i], EXPECT[i]);

    CHECK(ev("Ah 2c 3d 4s 5h", 5) < ev("2c 3d 4s 5h 6h", 5), "wheel must lose to six-high straight");
    CHECK(handCat(ev("Ah 2c 3d 4s 5h", 5)) == HC_STRAIGHT, "wheel is a straight");
    CHECK(handCat(ev("Ah Kc Qd Js 9h", 5)) == HC_HIGH, "AKQJ9 is not a straight");
    CHECK(handCat(ev("Qh Kc Ad 2s 3h", 5)) == HC_HIGH, "no wrap-around straights");
    CHECK(ev("Ah Kh 9h 7h 2h", 5) > ev("Ah Kh 9h 7h 2c As 5d", 7) - 1, "flush beats pair of aces");
    CHECK(ev("Ah Kh 9h 7h 3h", 5) > ev("Ah Kh 9h 7h 2h", 5), "flush kicker");
    CHECK(ev("Kc Kd Ks 2h 2c", 5) > ev("Qc Qd Qs Ah Ac", 5), "full house compares trips first");
    CHECK(ev("Kc Kd 2s 2h Ac", 5) > ev("Kc Kd 2s 2h Qc", 5), "two pair kicker");
    CHECK(ev("9c 9d 9s 9h Ac", 5) > ev("9c 9d 9s 9h Kc", 5), "quads kicker");
    CHECK(ev("Ac Ad Ks Kh Qc Qd 2s", 7) == ev("Ac Ad Ks Kh Qd", 5), "best two pair of three pairs");
    CHECK(ev("Ac Ad As Kh Kc Kd 2s", 7) == ev("Ac Ad As Kh Kc", 5), "two trips make a full house");
    CHECK(handCat(ev("Th Jh Qh Kh Ah", 5)) == HC_STFLUSH, "royal flush");
    CHECK(handCat(ev("2h 3h 4h 5h 6h 7c 8c", 7)) == HC_STFLUSH && handRank1(ev("2h 3h 4h 5h 6h 7c 8c", 7)) == 4, "straight flush high card, not the higher plain straight");
    CHECK(handCat(ev("Ah 2h 3h 4h 5h", 5)) == HC_STFLUSH && handRank1(ev("Ah 2h 3h 4h 5h", 5)) == 3, "steel wheel is five high");

    // Omaha: exactly two hole cards + three board cards
    int h[4], b[5];
    cards("Ah 2c 3d 4s", h, 4); cards("Kh Qh Jh Th 9c", b, 5);
    CHECK(handCat(evalOmaha(h, b, 5)) != HC_FLUSH && handCat(evalOmaha(h, b, 5)) != HC_STFLUSH, "Omaha: one heart in hand is no flush");
    cards("Ah 2h 3d 4s", h, 4);
    CHECK(handCat(evalOmaha(h, b, 5)) == HC_FLUSH, "Omaha: A2 of hearts makes an ace-high flush, not the board's royal");
    cards("Ah Th 3d 4s", h, 4);
    CHECK(handCat(evalOmaha(h, b, 5)) == HC_STFLUSH, "Omaha: Ah Th + KQJ of hearts is a royal");
    cards("2c 3d 4s 7h", h, 4); cards("Ac Kd Qs Jh Tc", b, 5);
    CHECK(handCat(evalOmaha(h, b, 5)) == HC_HIGH, "Omaha: board straight doesn't play");
    cards("Ac Ad As 2c", h, 4); cards("Kh Kd 7s 4c 9h", b, 5);
    CHECK(handCat(evalOmaha(h, b, 5)) == HC_TWOPAIR, "Omaha: can't use three aces from the hand");

    char buf[80];
    handName(ev("Kc Kd 9s 9h 2c", 5), buf, sizeof buf);
    CHECK(strcmp(buf, "Two Pair, Kings and Nines") == 0, "hand name '%s'", buf);
    handName(ev("Th Jh Qh Kh Ah", 5), buf, sizeof buf);
    CHECK(strcmp(buf, "Royal Flush") == 0, "hand name '%s'", buf);
    fmtChips(1234567, buf, sizeof buf);
    CHECK(strcmp(buf, "$1,234,567") == 0, "fmtChips '%s'", buf);
    fmtChips(-500, buf, sizeof buf);
    CHECK(strcmp(buf, "-$500") == 0, "fmtChips '%s'", buf);
    fmtChips(100, buf, sizeof buf);
    CHECK(strcmp(buf, "$100") == 0, "fmtChips '%s'", buf);
}

// ---------------------------------------------------------------- ranges

static float frac(const char *s) { Range r; rangeParse(&r, s); return roundf(rangeFraction(&r) * 1326); }

static void testRanges(void) {
    printf("ranges\n");
    int combos[NCLS] = { 0 }, total = 0;
    for (int a = 0; a < 52; a++)
        for (int b = a + 1; b < 52; b++) {
            int cls = hcClass(a, b);
            CHECK(cls == hcClass(b, a), "hcClass symmetric");
            combos[cls]++;
            total++;
            char nm[4];
            hcClassName(cls, nm);
            int r1 = cardRank(a), r2 = cardRank(b);
            int hi = r1 > r2 ? r1 : r2, lo = r1 > r2 ? r2 : r1;
            CHECK(nm[0] == RANKCH[hi] && nm[1] == RANKCH[lo], "class name %s for %c%c", nm, RANKCH[hi], RANKCH[lo]);
            if (hi != lo) CHECK(nm[2] == (cardSuit(a) == cardSuit(b) ? 's' : 'o'), "suitedness of %s", nm);
        }
    CHECK(total == 1326, "1326 combos");
    for (int i = 0; i < NCLS; i++) CHECK(combos[i] == hcCombos(i), "class %d combos %d vs %d", i, combos[i], hcCombos(i));

    CHECK(frac("22+") == 78, "22+ = 78 combos, got %.0f", frac("22+"));
    CHECK(frac("AKs") == 4 && frac("AKo") == 12 && frac("AK") == 16, "AK combos");
    CHECK(frac("A2s+") == 48, "A2s+ = 48, got %.0f", frac("A2s+"));
    CHECK(frac("ATs-AQs") == 12, "ATs-AQs = 12, got %.0f", frac("ATs-AQs"));
    CHECK(frac("AQs-ATs") == 12, "reversed span");
    CHECK(frac("22-JJ") == 60, "22-JJ = 60, got %.0f", frac("22-JJ"));
    CHECK(frac("JJ-QQ") == 12, "JJ-QQ = 12, got %.0f", frac("JJ-QQ"));
    CHECK(frac("K9o+") == 48, "K9o+ = 48, got %.0f", frac("K9o+"));
    CHECK(frac("A5s-A3s") == 12, "A5s-A3s = 12");
    Range r;
    CHECK(!rangeParse(&r, "ZZ,AKs"), "bad token reported");
    CHECK(r.in[hcClass(C("As"), C("Ks"))], "valid tokens still parsed");

    rangesInit();
    CHECK(hcTop[hcClass(C("As"), C("Ad"))] < 0.01f, "AA is the top hand");
    CHECK(hcTop[hcClass(C("7s"), C("2d"))] > 0.9f, "72o near the bottom");
    CHECK(hcTop[hcClass(C("As"), C("Ks"))] < hcTop[hcClass(C("Js"), C("Td"))], "AKs above JTo");
    float last = 0;
    for (int i = 0; i < NCLS; i++) if (hcTop[i] > last) last = hcTop[i];
    CHECK(fabsf(last - 1.0f) < 1e-4f, "percentiles reach 100%%");
    int h1[4], h2[4];
    cards("Ah Ad Kh Kd", h1, 4); cards("2c 7d 9h 4s", h2, 4);
    CHECK(omahaTop(h1) < 0.01f && omahaTop(h2) > 0.8f, "Omaha AAKKds top, 2794 rainbow bottom (%.3f %.3f)", omahaTop(h1), omahaTop(h2));
    for (int i = 0; i < chartCount(); i++) {
        const ChartDef *cd = chartGet(i);
        CHECK(rangeParse(&r, cd->raise), "chart %s raise range parses", cd->name);
        if (cd->call && cd->call[0]) {
            Range rc;
            CHECK(rangeParse(&rc, cd->call), "chart %s call range parses", cd->name);
            for (int k = 0; k < NCLS; k++) CHECK(!(r.in[k] && rc.in[k]), "chart %s: class %d in both raise and call", cd->name, k);
        }
    }
}

// ---------------------------------------------------------------- game rules

static GameConfig cfgN(int n, int variant, int limit, int stack) {
    GameConfig c = { 0 };
    c.variant = variant; c.limit = limit; c.n = n; c.sb = 1; c.bb = 2; c.buyin = stack; c.human = -1; c.seed = 42;
    for (int i = 0; i < n; i++) c.levels[i] = LV_MEDIUM;
    return c;
}

// rig the deck: holes[i] for seat i (space separated), then the board
static void rig(Game *g, const char **holes, const char *board) {
    bool used[52] = { 0 };
    int hc = holeCount(g);
    for (int i = 0; i < g->n; i++) {
        if (!holes[i]) continue;
        cards(holes[i], g->s[i].hole, hc);
        for (int k = 0; k < hc; k++) used[g->s[i].hole[k]] = true;
    }
    int b[5];
    cards(board, b, 5);
    int n = 0;
    for (int k = 0; k < 5; k++) { g->deck[n++] = b[k]; used[b[k]] = true; }
    for (int c = 0; c < 52; c++) if (!used[c]) g->deck[n++] = c;
    g->dpos = 0;
}

// every chip at the table: stacks plus whatever is still in the middle (nothing once the pot is paid out)
static int chips(const Game *g) { int t = 0; for (int i = 0; i < g->n; i++) t += g->s[i].stack + (g->handOver ? 0 : g->s[i].total); return t; }
static void act(Game *g, int type, int amt) { gameAct(g, (Action){ type, amt }); }
static void runOut(Game *g) { for (int k = 0; k < 10 && !g->handOver; k++) gameAdvance(g); }

static void testRules(void) {
    printf("betting rules\n");
    Game g;
    GameConfig cf = cfgN(3, V_HOLDEM, BL_NOLIMIT, 200);
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    CHECK(g.sbSeat == 1 && g.bbSeat == 2 && g.toAct == 0, "3-handed blinds/order: sb %d bb %d toAct %d", g.sbSeat, g.bbSeat, g.toAct);
    CHECK(g.s[1].bet == 1 && g.s[2].bet == 2 && gamePot(&g) == 3, "blinds posted");
    Legal l;
    gameLegal(&g, &l);
    CHECK(l.toCall == 2 && l.minTo == 4 && l.maxTo == 200 && l.canRaise, "UTG legal: call %d min %d max %d", l.toCall, l.minTo, l.maxTo);
    act(&g, A_RAISE, 3);                      // under the minimum -> clamped to 4
    CHECK(g.s[0].bet == 4 && g.curBet == 4 && g.minRaise == 2, "min-raise clamp: bet %d", g.s[0].bet);
    act(&g, A_RAISE, 10);                     // SB raises to 10 (raise of 6)
    CHECK(g.minRaise == 6, "min raise tracks the last raise size: %d", g.minRaise);
    gameLegal(&g, &l);
    CHECK(l.minTo == 16, "next min raise to 16, got %d", l.minTo);
    act(&g, A_FOLD, 0);                       // BB folds
    act(&g, A_CALL, 0);                       // UTG calls
    CHECK(g.toAct == -1, "round closes after the call");
    gameAdvance(&g);
    CHECK(g.street == ST_FLOP && g.nboard == 3 && g.toAct == 1, "flop: SB first to act (toAct %d)", g.toAct);
    CHECK(g.curBet == 0 && g.s[0].bet == 0, "bets reset on the flop");

    // BB option in a limped pot
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    act(&g, A_CALL, 0); act(&g, A_CALL, 0);
    CHECK(g.toAct == 2, "BB gets the option after limps");
    gameLegal(&g, &l);
    CHECK(l.canCheck && l.canRaise, "BB can check or raise");
    act(&g, A_CHECK, 0);
    CHECK(g.toAct == -1, "BB check closes preflop");

    // heads-up: button = small blind, acts first preflop and last after the flop
    GameConfig hu = cfgN(2, V_HOLDEM, BL_NOLIMIT, 200);
    gameSetup(&g, &hu);
    g.button = 1;
    gameNewHand(&g);
    CHECK(g.sbSeat == 1 && g.bbSeat == 0 && g.toAct == 1, "HU: button posts SB and acts first (toAct %d)", g.toAct);
    CHECK(posIndex(&g, 1) == POS_BTN && posIndex(&g, 0) == POS_BB, "HU positions");
    act(&g, A_CALL, 0);
    CHECK(g.toAct == 0, "HU: BB acts after the SB limps");
    act(&g, A_CHECK, 0);
    gameAdvance(&g);
    CHECK(g.toAct == 0, "HU: BB first after the flop");
    CHECK(inPosition(&g, 1) && !inPosition(&g, 0), "HU: button in position");

    // everyone folds to the big blind: BB gets the pot back, nothing else moves
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    int before = chips(&g);
    act(&g, A_FOLD, 0); act(&g, A_FOLD, 0);
    CHECK(g.handOver && g.s[2].stack == 201 && g.s[1].stack == 199 && g.s[0].stack == 200, "walk: stacks %d %d %d", g.s[0].stack, g.s[1].stack, g.s[2].stack);
    CHECK(chips(&g) == before, "walk conserves chips");

    // uncalled bet is returned
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    act(&g, A_RAISE, 50); act(&g, A_FOLD, 0); act(&g, A_FOLD, 0);
    CHECK(g.s[0].stack == 203 && g.s[0].won == 5, "uncalled raise refunded: stack %d won %d", g.s[0].stack, g.s[0].won);

    // positions for every table size
    for (int n = 2; n <= 6; n++) {
        GameConfig c = cfgN(n, V_HOLDEM, BL_NOLIMIT, 200);
        gameSetup(&g, &c);
        g.button = 0;
        gameNewHand(&g);
        int seen[6] = { 0 };
        for (int p = 0; p < n; p++) seen[posIndex(&g, p)]++;
        CHECK(seen[POS_BTN] == 1 && seen[POS_BB] == 1, "%d-handed: one BTN, one BB", n);
        if (n >= 3) CHECK(seen[POS_SB] == 1 && posIndex(&g, g.sbSeat) == POS_SB && posIndex(&g, g.bbSeat) == POS_BB, "%d-handed blinds", n);
        if (n == 6) CHECK(posIndex(&g, 3) == POS_UTG && posIndex(&g, 4) == POS_HJ && posIndex(&g, 5) == POS_CO, "6-max names");
        for (int p = 0; p < n; p++) for (int q = p + 1; q < n; q++) CHECK(posIndex(&g, p) != posIndex(&g, q), "%d-handed: unique positions", n);
    }
}

static void testShortAllin(void) {
    printf("short all-in does not reopen betting\n");
    Game g;
    GameConfig cf = cfgN(3, V_HOLDEM, BL_NOLIMIT, 1000);
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    g.s[1].stack = 119;            // SB: 1 posted + 119 = 120 total
    act(&g, A_RAISE, 100);         // UTG raises to 100
    act(&g, A_RAISE, 1000);        // SB all-in to 120: a raise of only 20 (< 98)
    CHECK(g.s[1].allin && g.curBet == 120, "SB all-in for 120 (curBet %d)", g.curBet);
    Legal l;
    CHECK(g.toAct == 2, "BB to act");
    gameLegal(&g, &l);
    CHECK(l.canRaise, "BB has not acted yet, so may raise");
    act(&g, A_CALL, 0);
    CHECK(g.toAct == 0, "back to UTG");
    gameLegal(&g, &l);
    CHECK(!l.canRaise && l.toCall == 20, "UTG may only call/fold after a short all-in (canRaise %d toCall %d)", l.canRaise, l.toCall);
    int stack = g.s[0].stack;
    act(&g, A_RAISE, 500);         // illegal raise turns into a call
    CHECK(g.s[0].stack == stack - 20 && g.toAct == -1, "illegal raise becomes a call");
}

static bool logHas(const Game *g, const char *needle) {
    for (int i = 0; i < g->nlog; i++) if (strstr(g->log[i], needle)) return true;
    return false;
}

static void testUncalled(void) {
    printf("uncalled bets and the pot you can win\n");
    Game g;
    GameConfig cf = cfgN(2, V_HOLDEM, BL_NOLIMIT, 200);
    // heads-up: the button shoves 200, the big blind calls all-in for 50
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    g.s[1].stack = 48;
    int before = chips(&g);
    act(&g, A_RAISE, 200);
    act(&g, A_CALL, 0);
    CHECK(g.toAct == -1 && gamePot(&g) == 250, "before the flop the shove is still out there (pot %d)", gamePot(&g));
    gameAdvance(&g);
    CHECK(gamePot(&g) == 100 && g.s[0].stack == 150 && !g.s[0].allin, "the 150 nobody could call comes back before the flop (pot %d, stack %d)", gamePot(&g), g.s[0].stack);
    CHECK(logHas(&g, "Uncalled $150 returned"), "the log says so");
    CHECK(g.toAct == -1 && g.nboard == 3, "no betting: the board runs out");
    runOut(&g);
    CHECK(g.handOver && chips(&g) == before && g.s[0].stack + g.s[1].stack == 250, "chips conserved after the refund");
    CHECK(g.s[0].stack == 150 || g.s[0].stack == 250 || g.s[0].stack == 200, "the button can only win or lose 50 (has %d)", g.s[0].stack);

    // postflop: a bet bigger than the caller's stack is trimmed when the round ends
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    g.s[1].stack = 58;              // 60 behind in all
    act(&g, A_CALL, 0); act(&g, A_CHECK, 0);
    gameAdvance(&g);
    act(&g, A_CHECK, 0);            // BB checks
    act(&g, A_RAISE, 150);          // button bets 150
    act(&g, A_CALL, 0);             // BB calls all-in for 58
    gameAdvance(&g);
    CHECK(gamePot(&g) == 120 && g.s[0].stack == 140, "turn: pot 120, the extra 92 is back (pot %d, stack %d)", gamePot(&g), g.s[0].stack);

    // nothing to return when the bet is matched
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    act(&g, A_RAISE, 6); act(&g, A_CALL, 0);
    gameAdvance(&g);
    CHECK(gamePot(&g) == 12 && !logHas(&g, "Uncalled"), "matched bets stay in (pot %d)", gamePot(&g));

    // gameCallPot: an all-in for less can't win the part of a bet it doesn't cover
    GameConfig c3 = cfgN(3, V_HOLDEM, BL_NOLIMIT, 1000);
    gameSetup(&g, &c3);
    g.button = 0;
    gameNewHand(&g);
    CHECK(gameCallPot(&g, 0) == gamePot(&g) + 2, "normal spot: pot + call (%d)", gameCallPot(&g, 0));
    g.s[2].stack = 40;              // BB: 42 in all
    act(&g, A_RAISE, 1000);         // button shoves 1000
    act(&g, A_FOLD, 0);             // SB folds 1
    CHECK(gameCallPot(&g, 2) == 42 + 42 + 1, "BB plays for 42 + 42 + 1 dead, not the whole shove (%d)", gameCallPot(&g, 2));
}

static void testPotLimit(void) {
    printf("pot limit\n");
    Game g;
    GameConfig cf = cfgN(4, V_OMAHA, BL_POTLIMIT, 1000);
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    Legal l;
    gameLegal(&g, &l);
    CHECK(l.maxTo == 7, "PLO UTG pot raise = 3.5 BB = 7, got %d", l.maxTo);
    act(&g, A_RAISE, 999);
    CHECK(g.s[3].bet == 7, "raise capped at the pot (%d)", g.s[3].bet);
    gameLegal(&g, &l);              // button facing 7: pot = 1+2+7 = 10, call 7 -> 17, raise to 7+17 = 24
    CHECK(l.maxTo == 24, "pot re-raise to 24, got %d", l.maxTo);
    act(&g, A_CALL, 0); act(&g, A_CALL, 0); act(&g, A_CALL, 0);
    gameAdvance(&g);
    gameLegal(&g, &l);
    CHECK(l.maxTo == 28 && l.minTo == 2, "flop pot bet = 28, min 2 (got %d / %d)", l.maxTo, l.minTo);
}

static void testSidePots(void) {
    printf("side pots and splits\n");
    Game g;
    GameConfig cf = cfgN(3, V_HOLDEM, BL_NOLIMIT, 1000);
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    // seat 0 (UTG/BTN in 3-handed: button acts first) has 50, seat 1 (SB) 100, seat 2 (BB) 300
    g.s[0].stack = 50; g.s[1].stack = 99; g.s[2].stack = 298;
    const char *holes[3] = { "Ah Ad", "Kh Kd", "Qh Qd" };
    rig(&g, holes, "2c 7s 9d Tc 3h");
    int before = chips(&g);
    act(&g, A_RAISE, 1000);     // seat 0 all-in 50
    act(&g, A_RAISE, 1000);     // seat 1 all-in 100
    act(&g, A_CALL, 0);         // seat 2 calls 100
    CHECK(g.toAct == -1, "everyone all-in / matched");
    runOut(&g);
    CHECK(g.handOver && g.showdown, "showdown reached");
    CHECK(g.s[0].stack == 150, "AA wins the main pot 150, has %d", g.s[0].stack);
    CHECK(g.s[1].stack == 100, "KK wins the side pot 100, has %d", g.s[1].stack);
    CHECK(g.s[2].stack == 200, "QQ keeps the 200 it never risked, has %d", g.s[2].stack);
    CHECK(chips(&g) == before, "side pots conserve chips");
    CHECK(g.npots == 2 && g.potAmt[0] == 150 && g.potAmt[1] == 100, "pots %d: %d %d", g.npots, g.potAmt[0], g.potAmt[1]);

    // chopped pot with an odd chip goes to the first seat left of the button
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    const char *h2[3] = { "2c 3d", "Ah Kd", "As Kc" };
    rig(&g, h2, "Qs Jh Td 8c 7s");
    act(&g, A_FOLD, 0);          // button folds
    act(&g, A_RAISE, 5);         // SB raises to 5
    act(&g, A_CALL, 0);          // BB calls: pot 10
    runOut(&g);
    while (!g.handOver) { if (g.toAct >= 0) act(&g, A_CHECK, 0); else gameAdvance(&g); }
    CHECK(g.handOver && g.s[1].won == 5 && g.s[2].won == 5, "split 10 -> %d / %d", g.s[1].won, g.s[2].won);
    // odd chip: the folded small blind makes the pot 5; the first winner left of the button gets 3
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    const char *h3[3] = { "Ah Kd", "2c 3d", "As Kc" };
    rig(&g, h3, "Qs Jh Td 8c 7s");
    act(&g, A_CALL, 0);          // button limps 2
    act(&g, A_FOLD, 0);          // SB folds 1 -> dead money
    act(&g, A_CHECK, 0);
    while (!g.handOver) { if (g.toAct >= 0) act(&g, A_CHECK, 0); else gameAdvance(&g); }
    CHECK(g.s[2].won == 3 && g.s[0].won == 2, "odd chip: seat 2 gets 3, button 2 (got %d / %d)", g.s[2].won, g.s[0].won);
    CHECK(strncmp(g.winText, "Split pot", 9) == 0, "headline says split: '%s'", g.winText);
}

static void testShowdownBoardPlays(void) {
    printf("board plays / kicker\n");
    Game g;
    GameConfig cf = cfgN(2, V_HOLDEM, BL_NOLIMIT, 200);
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    const char *h[2] = { "2c 3d", "4h 5s" };
    rig(&g, h, "Ah Kh Qh Jh Th");
    act(&g, A_CALL, 0); act(&g, A_CHECK, 0);
    while (!g.handOver) { if (g.toAct >= 0) act(&g, A_CHECK, 0); else gameAdvance(&g); }
    CHECK(g.s[0].stack == 200 && g.s[1].stack == 200, "royal on board splits: %d %d", g.s[0].stack, g.s[1].stack);
}

// random legal-ish actions: chip conservation, valid state, termination
static void testFuzz(int variant, int limit, int hands) {
    printf("fuzz %s %s (%d hands)\n", variant == V_OMAHA ? "Omaha" : "Hold'em", limit == BL_POTLIMIT ? "PL" : "NL", hands);
    Game *g = malloc(sizeof *g);
    Rng r;
    rngSeed(&r, 777 + variant * 10 + limit);
    int badState = 0, badChips = 0, stuck = 0;
    for (int n = 2; n <= 6; n++) {
        GameConfig cf = cfgN(n, variant, limit, 100 + 37 * n);
        cf.seed = 1000 + n;
        gameSetup(g, &cf);
        for (int h = 0; h < hands / 5; h++) {
            for (int i = 0; i < n; i++) if (rngInt(&r, 30) == 0) g->s[i].stack = 1 + rngInt(&r, 8);   // random short stacks
            gameNewHand(g);
            int start = chips(g), guard = 0;
            while (!g->handOver) {
                if (++guard > 400) { stuck++; break; }
                if (g->toAct < 0) {
                    gameAdvance(g);
                    if (!g->handOver) {        // every chip still in the middle can be called by someone
                        int top = 0, second = 0;
                        for (int i = 0; i < n; i++) {
                            int t = g->s[i].total;
                            if (t > top) { second = top; top = t; } else if (t > second) second = t;
                        }
                        if (top != second) badState++;
                    }
                    continue;
                }
                const Seat *s = &g->s[g->toAct];
                if (!s->inHand || s->allin) badState++;
                Legal l;
                gameLegal(g, &l);
                if (l.toCall < 0 || l.minTo > l.maxTo || l.maxTo > s->stack + s->bet || (l.canRaise && l.minTo <= g->curBet)) badState++;
                int t = rngInt(&r, 10);
                Action a = { t < 2 ? A_FOLD : t < 6 ? (l.canCheck ? A_CHECK : A_CALL) : A_RAISE, 0 };
                a.amount = rngInt(&r, 3) == 0 ? l.maxTo : l.minTo + rngInt(&r, l.maxTo - l.minTo + 1);
                if (rngInt(&r, 20) == 0) a.amount = rngInt(&r, 5000) - 100;    // garbage amounts must be clamped
                gameAct(g, a);
                int maxBet = 0;
                for (int i = 0; i < n; i++) {
                    if (g->s[i].stack < 0 || g->s[i].bet < 0) badState++;
                    if (g->s[i].bet > maxBet) maxBet = g->s[i].bet;
                }
                if (!g->handOver && maxBet > g->curBet) badState++;
                if (chips(g) != start) badChips++;
            }
            int after = 0;
            for (int i = 0; i < n; i++) after += g->s[i].stack;
            if (after != start) badChips++;
            if (g->showdown && g->nboard != 5) badState++;
        }
    }
    CHECK(badState == 0, "invalid states: %d", badState);
    CHECK(badChips == 0, "chip conservation failures: %d", badChips);
    CHECK(stuck == 0, "hands that never finished: %d", stuck);
    free(g);
}

// ---------------------------------------------------------------- equity, outs, nuts

static float eqVsHand(int variant, const char *hero, const char *vill, int trials) {
    // equity of hero vs one exact hand by enumerating random boards
    int hc = variant == V_OMAHA ? 4 : 2, h[4], v[4], b[5];
    cards(hero, h, hc); cards(vill, v, hc);
    bool used[52] = { 0 };
    for (int i = 0; i < hc; i++) used[h[i]] = used[v[i]] = true;
    int deck[52], nd = 0;
    for (int c = 0; c < 52; c++) if (!used[c]) deck[nd++] = c;
    Rng r;
    rngSeed(&r, 99);
    float w = 0;
    for (int t = 0; t < trials; t++) {
        for (int i = 0; i < 5; i++) { int j = i + rngInt(&r, nd - i), x = deck[i]; deck[i] = deck[j]; deck[j] = x; b[i] = deck[i]; }
        int a = evalPlayer(variant, h, b, 5), c = evalPlayer(variant, v, b, 5);
        w += a > c ? 1 : a == c ? 0.5f : 0;
    }
    return w / trials;
}

static void testEquity(void) {
    printf("equity / outs / nuts\n");
    CHECK(fabsf(eqVsHand(V_HOLDEM, "Ah As", "Kh Ks", 200000) - 0.82f) < 0.01f, "AA vs KK ~82%%");
    CHECK(fabsf(eqVsHand(V_HOLDEM, "Ah Ks", "Qd Qc", 200000) - 0.43f) < 0.01f, "AKo vs QQ ~43%%");
    Rng r;
    rngSeed(&r, 5);
    int h[4];
    OppRange any = { 1.0f, 0, 0, 0, 0 };
    cards("Ah As", h, 2);
    float e = equityCalc(V_HOLDEM, h, NULL, 0, &any, 1, 60000, &r);
    CHECK(fabsf(e - 0.852f) < 0.01f, "AA vs random = 85.2%%, got %.3f", e);
    cards("7h 2s", h, 2);
    e = equityCalc(V_HOLDEM, h, NULL, 0, &any, 1, 60000, &r);
    CHECK(fabsf(e - 0.346f) < 0.01f, "72o vs random = 34.6%%, got %.3f", e);
    OppRange anys[3] = { any, any, any };
    cards("Ah As", h, 2);
    e = equityCalc(V_HOLDEM, h, NULL, 0, anys, 3, 40000, &r);
    CHECK(fabsf(e - 0.64f) < 0.015f, "AA vs 3 random = 64%%, got %.3f", e);
    OppRange tight = { 0.03f, 0, 0, 0, 0 };
    cards("Kh Qs", h, 2);
    e = equityCalc(V_HOLDEM, h, NULL, 0, &tight, 1, 20000, &r);
    CHECK(e < 0.35f, "KQo vs top 3%% is a big dog, got %.3f", e);

    int b[5], oc[3];
    cards("Ah 7h", h, 2); cards("Kh 2h 9c", b, 3);
    countOuts(V_HOLDEM, h, b, 3, oc);
    CHECK(oc[0] == 9 && oc[1] == 0, "nut flush draw: 9 flush outs (got %d/%d/%d)", oc[0], oc[1], oc[2]);
    cards("9h 8c", h, 2); cards("7d 6s 2c", b, 3);
    countOuts(V_HOLDEM, h, b, 3, oc);
    CHECK(oc[1] == 8, "open-ender: 8 outs (got %d)", oc[1]);
    cards("9h 8c", h, 2); cards("6d 5s Kc", b, 3);
    countOuts(V_HOLDEM, h, b, 3, oc);
    CHECK(oc[1] == 4, "gutshot: 4 outs (got %d)", oc[1]);
    cards("9h 9c", h, 2); cards("9d 5s Kc", b, 3);
    countOuts(V_HOLDEM, h, b, 3, oc);
    CHECK(oc[2] == 7, "set: 7 outs to a full house or quads (got %d)", oc[2]);
    cards("Ah 7h", h, 2); cards("Kh 2h 9h", b, 3);
    CHECK(betterCombos(V_HOLDEM, h, b, 3) == 0, "nut flush is the nuts");
    cards("3h 4h", h, 2);
    CHECK(betterCombos(V_HOLDEM, h, b, 3) > 0, "low flush is not the nuts");
    int om[4];
    cards("Ah 2h 7c 8d", om, 4); cards("Kh 9h 4h", b, 3);
    CHECK(betterCombos(V_OMAHA, om, b, 3) == 0, "Omaha nut flush");
    cards("Th 9s 8d 2c", om, 4); cards("7h 6c Ks", b, 3);
    countOuts(V_OMAHA, om, b, 3, oc);
    CHECK(oc[1] >= 13, "Omaha wrap T98 on 76K: 13+ outs, got %d", oc[1]);
}

// ---------------------------------------------------------------- coach

// fully dealt game where seat 0 is the hero; returns after setting hole cards
static void setupSpot(Game *g, int n, int button, const char *hero) {
    GameConfig cf = cfgN(n, V_HOLDEM, BL_NOLIMIT, 200);
    cf.human = 0;
    cf.levels[0] = LV_HUMAN;
    gameSetup(g, &cf);
    g->button = button;
    gameNewHand(g);
    const char *holes[6] = { hero };
    rig(g, holes, "2c 2d 2h 3c 3d");
    // avoid duplicates with the other seats' hands
    bool used[52] = { 0 };
    for (int k = 0; k < 2; k++) used[g->s[0].hole[k]] = true;
    for (int k = 0; k < 5; k++) used[g->deck[k]] = true;
    int c = 0;
    for (int i = 1; i < n; i++)
        for (int k = 0; k < 2; k++) { while (used[c]) c++; g->s[i].hole[k] = c; used[c] = true; }
}

static void testCoachPreflop(void) {
    printf("coach preflop\n");
    Game g;
    Advice a;
    Rng r;
    rngSeed(&r, 3);
    // 6-max, hero UTG (button = 3 -> seat 0 is UTG)
    setupSpot(&g, 6, 3, "Ah Ad");
    CHECK(posIndex(&g, 0) == POS_UTG && g.toAct == 0, "hero UTG and first to act");
    advise(&g, 0, &a, 500, &r);
    CHECK(a.act == A_RAISE && a.amount == 5, "AA UTG: raise to 5 (got act %d amt %d)", a.act, a.amount);
    CHECK(a.q[A_FOLD] == Q_BLUNDER, "folding AA is a blunder");
    setupSpot(&g, 6, 3, "7h 2s");
    advise(&g, 0, &a, 500, &r);
    CHECK(a.act == A_FOLD && a.q[A_RAISE] >= Q_MISTAKE, "72o UTG: fold");
    setupSpot(&g, 6, 3, "Ts 9s");
    advise(&g, 0, &a, 500, &r);
    CHECK(a.act == A_RAISE, "T9s UTG is in the chart");
    // hero on the button, folded to
    setupSpot(&g, 6, 0, "Kc 8d");
    while (g.toAct != 0) act(&g, A_FOLD, 0);
    advise(&g, 0, &a, 500, &r);
    CHECK(a.act == A_RAISE, "K8o BTN open");
    // hero BB vs BTN open with 85s: defend
    setupSpot(&g, 6, 4, "8s 5s");
    CHECK(posIndex(&g, 0) == POS_BB, "hero BB");
    while (g.toAct != 4) act(&g, A_FOLD, 0);
    act(&g, A_RAISE, 5);
    act(&g, A_FOLD, 0);
    CHECK(g.toAct == 0, "BB to act");
    advise(&g, 0, &a, 500, &r);
    CHECK(a.act == A_CALL, "85s BB vs BTN: call (got %d: %s)", a.act, a.head);
    // short stack push/fold: 10 BB on the button with A5o
    setupSpot(&g, 6, 0, "As 5d");
    for (int i = 0; i < 6; i++) g.s[i].stack = 18;
    while (g.toAct != 0) act(&g, A_FOLD, 0);
    advise(&g, 0, &a, 500, &r);
    CHECK(a.act == A_RAISE && a.amount == g.s[0].stack + g.s[0].bet, "A5o BTN at 9 BB: jam (act %d amt %d)", a.act, a.amount);
}

static void testCoachPostflop(void) {
    printf("coach postflop\n");
    Game g;
    Advice a;
    Rng r;
    rngSeed(&r, 4);
    // heads-up, hero BB with the nut flush on the river facing a bet: raise
    GameConfig cf = cfgN(2, V_HOLDEM, BL_NOLIMIT, 400);
    cf.human = 0; cf.levels[0] = LV_HUMAN;
    gameSetup(&g, &cf);
    g.button = 1;
    gameNewHand(&g);
    const char *h[2] = { "Ah 7h", "Kc Qd" };
    rig(&g, h, "Kh 2h 9h 4c 8s");
    act(&g, A_RAISE, 5); act(&g, A_CALL, 0);
    gameAdvance(&g); act(&g, A_CHECK, 0); act(&g, A_RAISE, 6); act(&g, A_CALL, 0);
    gameAdvance(&g); act(&g, A_CHECK, 0); act(&g, A_RAISE, 15); act(&g, A_CALL, 0);
    gameAdvance(&g); act(&g, A_CHECK, 0); act(&g, A_RAISE, 30);
    CHECK(g.toAct == 0 && g.street == ST_RIVER, "hero facing a river bet");
    advise(&g, 0, &a, 3000, &r);
    CHECK(a.act == A_RAISE && a.q[A_FOLD] == Q_BLUNDER, "nut flush raises (act %d, eq %.2f)", a.act, a.equity);
    CHECK(strstr(a.hand, "nuts") != NULL, "hand text mentions the nuts: '%s'", a.hand);

    // pure air on the river facing a pot-sized bet: fold
    gameSetup(&g, &cf);
    g.button = 1;
    gameNewHand(&g);
    const char *h2[2] = { "6d 5c", "Kc Qd" };
    rig(&g, h2, "Kh Jh 9s 2c Ts");
    act(&g, A_RAISE, 5); act(&g, A_CALL, 0);
    gameAdvance(&g); act(&g, A_CHECK, 0); act(&g, A_RAISE, 6); act(&g, A_CALL, 0);
    gameAdvance(&g); act(&g, A_CHECK, 0); act(&g, A_CHECK, 0);
    gameAdvance(&g); act(&g, A_CHECK, 0); act(&g, A_RAISE, 22);
    advise(&g, 0, &a, 3000, &r);
    CHECK(a.act == A_FOLD && a.q[A_CALL] >= Q_MISTAKE, "air vs a pot bet folds (act %d, eq %.2f)", a.act, a.equity);

    // flush draw facing a small flop bet: continue (call or raise)
    gameSetup(&g, &cf);
    g.button = 1;
    gameNewHand(&g);
    const char *h3[2] = { "Ah 5h", "Kc Qd" };
    rig(&g, h3, "Kh 8h 2c 3s 4d");
    act(&g, A_RAISE, 5); act(&g, A_CALL, 0);
    gameAdvance(&g); act(&g, A_CHECK, 0); act(&g, A_RAISE, 3);
    advise(&g, 0, &a, 3000, &r);
    CHECK(a.act != A_FOLD && a.outs >= 9, "nut flush draw vs 1/3-pot bet continues (act %d, outs %d)", a.act, a.outs);

    // pot-limit: recommendations never exceed the pot
    GameConfig pl = cfgN(2, V_OMAHA, BL_POTLIMIT, 1000);
    pl.human = 0; pl.levels[0] = LV_HUMAN;
    gameSetup(&g, &pl);
    g.button = 1;
    gameNewHand(&g);
    const char *h4[2] = { "Ah As Kh Ks", "2c 3d 7s 8h" };
    rig(&g, h4, "Ac Kd 2h 5c 9s");
    act(&g, A_RAISE, 6);
    advise(&g, 0, &a, 900, &r);
    Legal l;
    gameLegal(&g, &l);
    CHECK(a.act == A_RAISE && a.amount <= l.maxTo && a.aggAmount <= l.maxTo, "PLO raise within the pot limit (%d <= %d)", a.amount, l.maxTo);
}

static void testCoachDetails(void) {
    printf("coach details\n");
    Game g;
    Advice a;
    Rng r;
    rngSeed(&r, 8);
    // pot-limit Hold'em facing a 4-bet: the suggested shove must respect the pot limit
    GameConfig cf = cfgN(3, V_HOLDEM, BL_POTLIMIT, 100);
    cf.human = 0; cf.levels[0] = LV_HUMAN;
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    const char *h[3] = { "Ah Ad", "Kc Qd", "7s 2c" };
    rig(&g, h, "2d 3d 4d 5c 9s");
    act(&g, A_RAISE, 6);         // hero (button) opens
    act(&g, A_RAISE, 18);        // SB 3-bets
    act(&g, A_FOLD, 0);
    act(&g, A_RAISE, 24);        // hero 4-bets small
    act(&g, A_RAISE, 60);        // SB 5-bets (pot-limit max is bigger)
    CHECK(g.toAct == 0, "hero faces a 5-bet");
    advise(&g, 0, &a, 500, &r);
    Legal l;
    gameLegal(&g, &l);
    CHECK(a.aggAmount <= l.maxTo && (a.act != A_RAISE || a.amount <= l.maxTo), "PL: suggested raise %d / %d within max %d", a.amount, a.aggAmount, l.maxTo);
    CHECK(a.act != A_FOLD, "AA never folds here");

    // trips on the board are shared - not "your" trips
    GameConfig hu = cfgN(2, V_HOLDEM, BL_NOLIMIT, 200);
    hu.human = 0; hu.levels[0] = LV_HUMAN;
    gameSetup(&g, &hu);
    g.button = 1;
    gameNewHand(&g);
    const char *h2[2] = { "Ah 5c", "Kc Qd" };
    rig(&g, h2, "9s 9d 9h 2c 7s");
    act(&g, A_CALL, 0); act(&g, A_CHECK, 0); gameAdvance(&g);
    char buf[120];
    describeMadeHand(&g, 0, buf, sizeof buf);
    CHECK(strstr(buf, "board") != NULL, "board trips described as the board's: '%s'", buf);

    // an opener who just calls a 3-bet has a much narrower range than their open
    GameConfig six = cfgN(6, V_HOLDEM, BL_NOLIMIT, 200);
    six.human = 0; six.levels[0] = LV_HUMAN;
    gameSetup(&g, &six);
    g.button = 2;                 // seat 5 = UTG? seats: 3 SB, 4 BB, 5 UTG, 0 HJ, 1 CO, 2 BTN
    gameNewHand(&g);
    CHECK(g.toAct == 5, "UTG to act");
    act(&g, A_FOLD, 0);           // UTG
    act(&g, A_FOLD, 0);           // hero HJ folds? no - hero raises below
    // seat 0 already acted (fold); rebuild: let CO open, BTN 3-bet, CO call
    gameSetup(&g, &six);
    g.button = 2;
    gameNewHand(&g);
    act(&g, A_FOLD, 0);           // UTG (5)
    act(&g, A_RAISE, 5);          // hero HJ (0) opens
    act(&g, A_RAISE, 15);         // CO (1) 3-bets
    act(&g, A_FOLD, 0); act(&g, A_FOLD, 0); act(&g, A_FOLD, 0);
    act(&g, A_CALL, 0);           // hero calls
    gameAdvance(&g);
    OppRange o[MAXP];
    int n;
    estimateRanges(&g, 1, o, &n);   // from the 3-bettor's point of view
    CHECK(n == 1 && o[0].top < 0.12f, "opener who called a 3-bet: top %.2f (open range was ~0.22)", o[0].top);
    g.s[0].st.hands = 40; g.s[0].st.vpip = 12; g.s[0].st.pfr = 10;     // HUD stats must not undo the narrowing
    estimateRanges(&g, 1, o, &n);
    CHECK(o[0].top < 0.15f, "with HUD stats: top %.2f", o[0].top);
}

// advice is well-formed in every spot the bots create
static float pushShare(int pos, float bb) { Range r; pushRange(pos, bb, &r); return rangeFraction(&r); }

static int clsOfName(const char *h) { int c[2]; cards(h, c, 2); return hcClass(c[0], c[1]); }

static void testSolver(void) {
    printf("push/fold solver + coach upgrades\n");
    // the generated equilibrium tables look like published Nash push/fold charts
    for (int pos = POS_UTG; pos <= POS_SB; pos++) {
        float last = 2;
        for (float bb = 3; bb <= 25; bb += 1) {
            float f = pushShare(pos, bb);
            CHECK(f <= last + 0.01f, "pos %d: push range shrinks as stacks grow (%.1f BB: %.3f > %.3f)", pos, bb, f, last);
            last = f;
        }
        CHECK(PUSH_HALF_BB[pos][clsOfName("Ah Ad")] == 50, "AA always shoves from pos %d", pos);
    }
    for (int pos = POS_UTG; pos < POS_SB; pos++)
        CHECK(pushShare(pos, 10) < pushShare(pos + 1, 10), "later seats shove wider at 10 BB (pos %d)", pos);
    float sb10 = pushShare(POS_SB, 10), utg10 = pushShare(POS_UTG, 10);
    CHECK(sb10 > 0.54f && sb10 < 0.63f, "SB vs BB at 10 BB shoves ~58%% (got %.1f%%)", sb10 * 100);
    CHECK(utg10 > 0.11f && utg10 < 0.18f, "UTG at 10 BB shoves ~14%% (got %.1f%%)", utg10 * 100);
    CHECK(!pushHand(POS_UTG, clsOfName("7h 2c"), 10), "72o never shoves UTG at 10 BB");
    CHECK(pushHand(POS_SB, clsOfName("Kh 2c"), 10), "K2o shoves from the SB at 10 BB");

    Game g;
    Advice a;
    Rng r;
    rngSeed(&r, 11);
    // the coach follows the table exactly: button, 8 BB, folded to
    const char *hands[] = { "As 5d", "Kh 9c", "Qs 8s", "9d 8d", "Jc 4h", "6h 6d", "Ts 3c", "7c 2d", "Ah Kd", "5s 4s" };
    for (int i = 0; i < 10; i++) {
        setupSpot(&g, 6, 0, hands[i]);
        for (int k = 0; k < 6; k++) g.s[k].stack = 16 - g.s[k].bet;
        while (g.toAct != 0) act(&g, A_FOLD, 0);
        advise(&g, 0, &a, 300, &r);
        bool push = pushHand(POS_BTN, clsOfName(hands[i]), 8);
        CHECK((a.act == A_RAISE) == push && (!push || a.amount == g.s[0].stack), "BTN 8 BB %s: %s (got %s)", hands[i], push ? "shove" : "fold", a.head);
    }

    // a short open-shove is read as the solved range for that seat and depth
    GameConfig hu = cfgN(2, V_HOLDEM, BL_NOLIMIT, 20);
    hu.human = 0; hu.levels[0] = LV_HUMAN;
    gameSetup(&g, &hu);
    g.button = 1;
    gameNewHand(&g);
    const char *h1[2] = { "Ac 9d", "Kc Qd" };
    rig(&g, h1, "2c 3d 7h 8s Jd");
    act(&g, A_RAISE, 20);
    CHECK(g.toAct == 0 && g.s[1].allin, "SB shoved 10 BB");
    OppRange o[MAXP];
    int n;
    estimateRanges(&g, 0, o, &n);
    CHECK(n == 1 && o[0].set != NULL && o[0].top > 0.5f && o[0].top < 0.65f, "shove range = solved SB range (top %.2f)", o[0].top);
    advise(&g, 0, &a, 3000, &r);
    CHECK(a.act == A_CALL, "A9o calls a 10 BB SB shove (got %s, eq %.2f need %.2f)", a.head, a.equity, a.need);
    gameSetup(&g, &hu);
    g.button = 1;
    gameNewHand(&g);
    const char *h2[2] = { "7c 2d", "Kc Qd" };
    rig(&g, h2, "2c 3d 7h 8s Jd");
    act(&g, A_RAISE, 20);
    advise(&g, 0, &a, 3000, &r);
    CHECK(a.act == A_FOLD, "72o folds to a 10 BB shove (got %s)", a.head);

    // 15 BB on the button facing a cutoff open: re-steal or fold, never flat
    const char *rs[3] = { "Ad 9c", "Ah As", "7d 2c" };
    for (int i = 0; i < 3; i++) {
        setupSpot(&g, 6, 0, rs[i]);
        for (int k = 0; k < 6; k++) g.s[k].stack = 30 - g.s[k].bet;
        act(&g, A_FOLD, 0); act(&g, A_FOLD, 0);        // UTG, HJ
        act(&g, A_RAISE, 5);                             // CO opens 2.5 BB
        CHECK(g.toAct == 0, "hero on the button faces the open");
        advise(&g, 0, &a, 2000, &r);
        if (i < 2) CHECK(a.act == A_RAISE && a.amount == 30, "%s at 15 BB re-steals all-in (got %s)", rs[i], a.head);
        else CHECK(a.act == A_FOLD && a.q[A_CALL] >= Q_MISTAKE, "72o folds (got %s)", a.head);
    }

    // facing a short all-in with players still to act: those players don't count as opponents at showdown
    setupSpot(&g, 6, 1, "Ac Jc");                       // hero (seat 0) in the cutoff
    CHECK(posIndex(&g, 0) == POS_CO, "hero CO");
    g.s[5].stack = 24 - g.s[5].bet;                     // the hijack has 12 BB
    act(&g, A_RAISE, 5);                                 // UTG opens
    act(&g, A_RAISE, 24);                                // HJ shoves 12 BB
    CHECK(g.toAct == 0 && g.s[5].allin, "hero faces open + shove");
    advise(&g, 0, &a, 3000, &r);
    CHECK(a.equity > 0.4f, "AJs vs open + 12 BB shove: equity counts only the two players in (got %.2f)", a.equity);
    // deep stack facing a short open-shove: an equity decision, not the 100 BB charts
    const char *dv[2] = { "Ad Qc", "7d 2c" };
    for (int i = 0; i < 2; i++) {
        setupSpot(&g, 6, 0, dv[i]);                      // hero on the button, 100 BB
        g.s[5].stack = 30 - g.s[5].bet;                  // cutoff has 15 BB
        act(&g, A_FOLD, 0); act(&g, A_FOLD, 0);
        act(&g, A_RAISE, 30);                            // CO shoves
        advise(&g, 0, &a, 3000, &r);
        if (i == 0) CHECK(a.act == A_CALL && strstr(a.spot, "all-in"), "AQo calls a 15 BB cutoff shove (got %s / %s)", a.head, a.spot);
        else CHECK(a.act == A_FOLD, "72o folds to it (got %s)", a.head);
    }

    // river, checked through: top pair bets for value, a weak pair checks (judged against the calling range)
    const char *rv[2] = { "Kd Qc", "3h 3c" };
    for (int i = 0; i < 2; i++) {
        GameConfig cf = cfgN(2, V_HOLDEM, BL_NOLIMIT, 400);
        cf.human = 0; cf.levels[0] = LV_HUMAN;
        gameSetup(&g, &cf);
        g.button = 0;
        gameNewHand(&g);
        const char *h[2] = { rv[i], "9c 9d" };
        rig(&g, h, "Kh 7s 2d 5c Tc");
        act(&g, A_RAISE, 5); act(&g, A_CALL, 0);
        for (int st = 0; st < 2; st++) { gameAdvance(&g); act(&g, A_CHECK, 0); act(&g, A_CHECK, 0); }
        gameAdvance(&g); act(&g, A_CHECK, 0);
        CHECK(g.street == ST_RIVER && g.toAct == 0, "hero to act on the river");
        advise(&g, 0, &a, 3000, &r);
        if (i == 0) CHECK(a.act == A_RAISE, "KQ on K725T bets for value (got %s)", a.head);
        else CHECK(a.act == A_CHECK, "33 checks behind (got %s)", a.head);
    }
}

static void testCoachShortCall(void) {
    printf("coach: pot odds when you can't cover the bet\n");
    Game g;
    Advice a;
    Rng r;
    rngSeed(&r, 17);
    // BB with 21 BB facing a 500 BB shove from the button: you need ~47%, not ~4%
    const char *hands[2] = { "7h 2c", "Ah Ad" };
    for (int i = 0; i < 2; i++) {
        setupSpot(&g, 3, 1, hands[i]);  // button = seat 1 -> seat 0 is the big blind
        CHECK(posIndex(&g, 0) == POS_BB, "hero is the BB");
        g.s[0].stack = 40;
        g.s[1].stack = 998;
        act(&g, A_RAISE, 1000);         // button shoves
        act(&g, A_FOLD, 0);             // SB folds
        CHECK(g.toAct == 0, "hero to act");
        advise(&g, 0, &a, 1500, &r);
        CHECK(fabsf(a.need - 40.0f / 85) < 0.01f, "%s: need %.0f%% (expected 47%%)", hands[i], a.need * 100);
        CHECK(a.act == (i ? A_CALL : A_FOLD), "%s: %s (got %s)", hands[i], i ? "call" : "fold", a.head);
    }
    // the coach's text quotes what you can win
    bool said = false;
    for (int k = 0; k < a.nwhy; k++) said |= strstr(a.why[k], "You need 47%") != NULL;
    CHECK(said, "the reason quotes 47%%");

    // postflop: 38 behind facing a 300 bet into 4 -> need 38 / (38 + 38 + 4) = 47.5%, not 38 / 342 = 11%
    setupSpot(&g, 2, 1, "Kh Qd");
    g.s[0].stack = 38;                  // BB: 38 behind after posting
    act(&g, A_CALL, 0); act(&g, A_CHECK, 0);
    gameAdvance(&g);
    g.board[0] = C("9s"); g.board[1] = C("5h"); g.board[2] = C("2c");
    act(&g, A_CHECK, 0);                // hero (BB) checks
    act(&g, A_RAISE, 300);              // button overbets
    CHECK(g.toAct == 0, "hero to act on the flop");
    advise(&g, 0, &a, 800, &r);
    CHECK(fabsf(a.need - 38.0f / 80) < 0.001f, "postflop need %.1f%% (expected 47.5%%)", a.need * 100);
    said = false;
    for (int k = 0; k < a.nwhy; k++) said |= strstr(a.why[k], "call $38 (19 BB) to win $42") != NULL;
    if (!said) for (int k = 0; k < a.nwhy; k++) printf("    why: %s\n", a.why[k]);
    CHECK(said, "pot odds text: call $38 to win $42 (the pot plus the part of the bet you can match)");
}

static void testCoachFuzz(int variant, int limit, int hands) {
    printf("coach fuzz %s %s (%d hands)\n", variant == V_OMAHA ? "Omaha" : "Hold'em", limit == BL_POTLIMIT ? "PL" : "NL", hands);
    Game *g = malloc(sizeof *g);
    Rng r;
    rngSeed(&r, 11 + variant);
    int bad = 0, n = 0, cut = 0, badThink = 0;
    for (int tbl = 2; tbl <= 6; tbl++) {
        GameConfig cf = cfgN(tbl, variant, limit, tbl % 2 ? 40 : 200);
        cf.human = 0; cf.levels[0] = LV_HUMAN;
        for (int i = 1; i < tbl; i++) cf.levels[i] = 1 + (i % 3);
        cf.seed = 50 + tbl;
        gameSetup(g, &cf);
        for (int h = 0; h < hands / 5; h++) {
            if (g->s[0].stack <= 0) { g->s[0].stack = cf.buyin; }
            gameNewHand(g);
            while (!g->handOver) {
                if (g->toAct < 0) { gameAdvance(g); continue; }
                if (g->toAct != 0) { gameAct(g, aiDecide(g, g->toAct, &r)); continue; }
                Advice a;
                advise(g, 0, &a, 300, &r);
                Legal l;
                gameLegal(g, &l);
                n++;
                bool ok = a.q[a.act] == Q_BEST && a.head[0] && a.nwhy > 0 && a.spot[0];
                ok = ok && !(a.act == A_CHECK && !l.canCheck) && !(a.act == A_CALL && l.canCheck) && !(a.act == A_FOLD && l.canCheck);
                ok = ok && !(a.act == A_RAISE && (!l.canRaise || a.amount < l.minTo || a.amount > l.maxTo));
                ok = ok && (!l.canRaise || (a.aggAmount >= l.minTo && a.aggAmount <= l.maxTo && a.lo <= a.hi));
                for (int k = 0; k < 4; k++) ok = ok && a.q[k] >= -1 && a.q[k] <= Q_BLUNDER;
                ok = ok && (l.canCheck ? a.q[A_CHECK] >= 0 && a.q[A_CALL] < 0 : a.q[A_CALL] >= 0 && a.q[A_CHECK] < 0);
                ok = ok && a.equity >= 0 && a.equity <= 1;
                ok = ok && (l.toCall == 0 ? a.need == 0 : fabsf(a.need - (float)l.toCall / gameCallPot(g, 0)) < 1e-4f);
                for (int k = 0; k < a.nwhy; k++)               // a reason that fills its buffer was cut off
                    if (strlen(a.why[k]) >= sizeof a.why[k] - 1) { if (cut++ < 3) printf("    truncated: '%s'\n", a.why[k]); }
                if (strlen(a.hand) >= sizeof a.hand - 1 || strlen(a.spot) >= sizeof a.spot - 1 || strlen(a.head) >= sizeof a.head - 1) cut++;
                char tp[NTHINK][220];
                int nt = thinkPrompts(g, 0, tp);
                bool tok = nt >= 3 && nt <= NTHINK;
                for (int k = 0; k < nt; k++) tok = tok && tp[k][0] && strlen(tp[k]) < sizeof tp[k] - 1 && !strstr(tp[k], "(null)");
                if (!tok) { if (badThink++ < 3) printf("    bad checklist (%d): '%s'\n", nt, nt > 0 ? tp[nt - 1] : ""); }
                if (!ok) {
                    bad++;
                    if (bad <= 5) printf("    bad advice: act %d amt %d agg %d [%d..%d] lo %d hi %d q %d %d %d %d head '%s'\n", a.act, a.amount, a.aggAmount, l.minTo, l.maxTo, a.lo, a.hi, a.q[0], a.q[1], a.q[2], a.q[3], a.head);
                }
                gameAct(g, (Action){ a.act, a.amount });
            }
        }
    }
    CHECK(bad == 0, "%d malformed advice out of %d", bad, n);
    CHECK(cut == 0, "%d coach texts cut off by their buffers", cut);
    CHECK(badThink == 0, "%d malformed think-it-through checklists", badThink);
    free(g);
}

static void testGlossary(void) {
    printf("glossary\n");
    int per[GC_N] = { 0 };
    for (int i = 0; i < glossCount(); i++) {
        const GlossEntry *e = glossGet(i);
        CHECK(e->term[0] && strlen(e->def) > 20 && e->cat >= 0 && e->cat < GC_N, "entry %d well formed", i);
        CHECK(glossLookup(e->term) == i, "'%s' is unique", e->term);
        per[e->cat < GC_N ? e->cat : 0]++;
        bool ascii = true;
        for (const char *p = e->def; *p; p++) ascii = ascii && (unsigned char)*p >= 32 && (unsigned char)*p < 127;
        CHECK(ascii, "'%s': ASCII only (the UI font has no other glyphs)", e->term);
    }
    for (int c = 0; c < GC_N; c++) CHECK(per[c] >= 4, "category %s has terms (%d)", glossCatName(c), per[c]);
    // every term the UI's tooltips ask for exists
    const char *ui[] = { "Equity", "Pot odds", "Starting-hand strength (top X%)", "Stack-to-pot ratio (SPR)", "Grades", "Coach recommends",
                         "Hints / Quiz mode", "Mixed strategy", "Leak", "Bot levels", "Heads-up", "HUD", "Under the Gun (UTG)", "Hijack (HJ)",
                         "Cutoff (CO)", "Button (BTN)", "Small blind (SB)", "Big blind (BB)" };
    for (int i = 0; i < (int)(sizeof ui / sizeof ui[0]); i++) CHECK(glossLookup(ui[i]) >= 0, "tooltip term '%s' exists", ui[i]);
    // all the jargon the coach uses is explained
    const char *jargon[] = { "MDF", "SPR", "stack-to-pot", "c-bet", "continuation bet", "3-bet", "4-bet", "semi-bluff", "implied odds", "pot odds", "equity",
                             "outs", "the nuts", "overpair", "underpair", "top pair", "gutshot", "open-ended", "wrap", "squeeze", "isolate", "limpers",
                             "push/fold", "Nash", "re-steal", "capped", "blockers", "multiway", "range", "position", "kicker", "trips", "set of", "rundowns",
                             "danglers", "double-suited", "initiative", "barrel", "trap", "stab", "dry", "wet", "showdown", "value", "bluff",
                             "effective stack", "close the action", "defend", "VPIP", "PFR", "rule of 4 and 2", "flush draw", "polarized", "balanced",
                             "pot-limit", "min-raise", "side pot", "all-in", "heads-up", "out of position", "in position", "free card", "check behind",
                             "1/3 pot", "overcards", "paired", "connected", "suited", "offsuit", "broadway", "premium", "solver", "exploit", "mixing" };
    for (int i = 0; i < (int)(sizeof jargon / sizeof jargon[0]); i++) {
        int f[4];
        CHECK(glossFind(jargon[i], f, 4) >= 1, "jargon '%s' has a glossary entry", jargon[i]);
    }
    // whole words only: no false hits inside other words
    int f[8];
    CHECK(glossFind("upset settlers", f, 8) == 0, "'set' doesn't match inside words");
    int n = glossFind("Semi-bluff: bet your draw. Theory note: MDF says defend 50%.", f, 8);
    CHECK(n >= 3 && f[0] == glossLookup("Semi-bluff"), "terms in order of mention (n=%d)", n);
    CHECK(glossMatches(glossLookup("VPIP"), "vpip") && !glossMatches(glossLookup("VPIP"), "zzzz"), "search");
}

static void testBots(void) {
    printf("bots\n");
    // every bot action is legal as sent (no silent conversions needed)
    Game *g = malloc(sizeof *g);
    Rng r;
    rngSeed(&r, 21);
    int illegal = 0, total = 0;
    for (int variant = 0; variant < 2; variant++) {
        GameConfig cf = cfgN(6, variant, variant ? BL_POTLIMIT : BL_NOLIMIT, 200);
        for (int i = 0; i < 6; i++) cf.levels[i] = 1 + i % 3;
        gameSetup(g, &cf);
        for (int h = 0; h < 600; h++) {
            gameNewHand(g);
            while (!g->handOver) {
                if (g->toAct < 0) { gameAdvance(g); continue; }
                Legal l;
                gameLegal(g, &l);
                Action a = aiDecide(g, g->toAct, &r);
                total++;
                if ((a.type == A_CHECK && !l.canCheck) || (a.type == A_CALL && l.canCheck) || (a.type == A_FOLD && l.canCheck) ||
                    (a.type == A_RAISE && (!l.canRaise || a.amount < l.minTo || a.amount > l.maxTo))) {
                    illegal++;
                    if (illegal <= 5) printf("    level %d: type %d amt %d (call %d check %d raise %d [%d..%d])\n", g->s[g->toAct].level, a.type, a.amount, l.toCall, l.canCheck, l.canRaise, l.minTo, l.maxTo);
                }
                gameAct(g, a);
            }
        }
    }
    CHECK(illegal == 0, "%d of %d bot actions were illegal as sent", illegal, total);
    free(g);
}

static void testArrange(void) {
    printf("card arranging\n");
    Arrange a;
    arrReset(&a, 4);
    CHECK(a.order[0] == 0 && a.order[3] == 3 && a.press < 0 && a.sel < 0, "reset to dealt order");
    arrMove(&a, 0, 3);
    CHECK(a.order[0] == 1 && a.order[1] == 2 && a.order[2] == 3 && a.order[3] == 0, "move first to last");
    arrMove(&a, 3, 0);
    CHECK(a.order[0] == 0 && a.order[1] == 1 && a.order[3] == 3, "move back");
    arrMove(&a, 1, 9); arrMove(&a, -1, 2);
    CHECK(a.order[1] == 1 && a.order[2] == 2, "out-of-range moves ignored");

    // drag slot 0 to slot 2: press, move past the threshold, release over the target
    arrReset(&a, 4);
    CHECK(!arrInput(&a, 0, true, true, 100, 100, -1) && a.press == 0 && !a.dragging, "press starts nothing yet");
    arrInput(&a, 0, false, true, 103, 101, -1);
    CHECK(!a.dragging, "small jitter is not a drag");
    arrInput(&a, 0, false, true, 140, 100, 1);
    CHECK(a.dragging, "moving past the threshold starts a drag");
    CHECK(arrInput(&a, 2, false, false, 230, 100, 2), "drop reports a change");
    CHECK(a.order[0] == 1 && a.order[1] == 2 && a.order[2] == 0 && a.order[3] == 3, "dragged card inserted at slot 2 (%d %d %d %d)", a.order[0], a.order[1], a.order[2], a.order[3]);
    CHECK(a.press < 0 && !a.dragging, "state cleared after the drop");
    // dropping back where it started changes nothing
    arrInput(&a, 1, true, true, 0, 0, -1);
    arrInput(&a, 1, false, true, 20, 0, 1);
    CHECK(!arrInput(&a, 1, false, false, 20, 0, 1) && a.order[1] == 2, "drop on own slot is a no-op");

    // click-swap: click slot 3, then slot 0
    arrReset(&a, 4);
    arrInput(&a, 3, true, true, 0, 0, -1);
    arrInput(&a, 3, false, false, 0, 0, -1);
    CHECK(a.sel == 3, "first click selects");
    arrInput(&a, 3, true, true, 0, 0, -1);
    arrInput(&a, 3, false, false, 0, 0, -1);
    CHECK(a.sel < 0, "clicking the same card deselects");
    arrInput(&a, 3, true, true, 0, 0, -1); arrInput(&a, 3, false, false, 0, 0, -1);
    arrInput(&a, 0, true, true, 0, 0, -1);
    CHECK(arrInput(&a, 0, false, false, 0, 0, -1), "second click swaps");
    CHECK(a.order[0] == 3 && a.order[3] == 0 && a.sel < 0, "slots 0 and 3 swapped");
    // press on a card, release somewhere else without dragging: nothing happens
    arrReset(&a, 4);
    arrInput(&a, 1, true, true, 0, 0, -1);
    CHECK(!arrInput(&a, -1, false, false, 2, 2, -1) && a.sel < 0, "release off the card is ignored");
    // pressing empty space does nothing
    CHECK(!arrInput(&a, -1, true, true, 0, 0, -1) && a.press < 0, "press on empty space ignored");
    // starting a drag cancels a pending click-swap selection
    arrInput(&a, 2, true, true, 0, 0, -1); arrInput(&a, 2, false, false, 0, 0, -1);
    arrInput(&a, 0, true, true, 0, 0, -1); arrInput(&a, 0, false, true, 50, 0, 1);
    CHECK(a.sel < 0, "drag clears the selection");
    arrInput(&a, 1, false, false, 50, 0, 1);

    // sorting
    int h[4];
    cards("7d As 7s Kd", h, 4);
    arrReset(&a, 4);
    arrSort(&a, h, SORT_RANK);
    CHECK(cardRank(h[a.order[0]]) == 12 && cardRank(h[a.order[1]]) == 11 && cardRank(h[a.order[2]]) == 5 && cardRank(h[a.order[3]]) == 5, "rank sort A K 7 7");
    arrSort(&a, h, SORT_SUIT);
    CHECK(cardSuit(h[a.order[0]]) == cardSuit(h[a.order[1]]) && cardSuit(h[a.order[2]]) == cardSuit(h[a.order[3]]), "suit sort groups suits");
    CHECK(cardRank(h[a.order[0]]) > cardRank(h[a.order[1]]) && cardRank(h[a.order[2]]) > cardRank(h[a.order[3]]), "high to low inside a suit");
    bool seen[4] = { 0 };
    for (int k = 0; k < 4; k++) seen[a.order[k]] = true;
    CHECK(seen[0] && seen[1] && seen[2] && seen[3], "sorting keeps every card exactly once");
    arrReset(&a, 2);
    cards("2c Ah", h, 2);
    arrSort(&a, h, SORT_RANK);
    CHECK(a.order[0] == 1 && a.order[1] == 0, "Hold'em: two cards sort too");
    arrSort(&a, h, SORT_NONE);
    CHECK(a.order[0] == 1, "SORT_NONE leaves the order alone");
}

// ---------------------------------------------------------------- every game: evaluators

static int nCards(const char *s) { return (int)(strlen(s) + 1) / 3; }
static int evV(int variant, const char *s) { int c[7]; int n = nCards(s); cards(s, c, n); return evalCards(variant, c, n, NULL, 0, NULL); }
static void nameV(int variant, const char *s, char *buf, int n) { handNameV(variant, evV(variant, s), buf, n); }

static void testEvalVariants(void) {
    printf("evaluators: short deck, A-5 low, 2-7 low, badugi, Omaha hi-lo\n");
    // short deck: every five-card hand from the 36-card deck, against combinatorial counts
    long cnt[9] = { 0 };
    int d[36], nd = 0, c[5];
    for (int x = 0; x < 52; x++) if (cardRank(x) >= 4) d[nd++] = x;
    for (int a = 0; a < 36; a++) for (int b = a + 1; b < 36; b++) for (int e = b + 1; e < 36; e++)
    for (int f = e + 1; f < 36; f++) for (int h = f + 1; h < 36; h++) {
        c[0] = d[a]; c[1] = d[b]; c[2] = d[e]; c[3] = d[f]; c[4] = d[h];
        cnt[handCat(evalShortDeck(c, 5))]++;
    }
    static const long SD[9] = { 122400, 193536, 36288, 16128, 6120, 480, 1728, 288, 24 };
    for (int i = 0; i < 9; i++) CHECK(cnt[i] == SD[i], "short deck %s: %ld, expected %ld", catName(i), cnt[i], SD[i]);
    int h5[5];
    cards("Ah 6c 7d 8s 9h", h5, 5);
    CHECK(handCat(evalShortDeck(h5, 5)) == HC_STRAIGHT && handRank1(evalShortDeck(h5, 5)) == 7, "short deck: A-6-7-8-9 is a nine-high straight");
    int fl[5], fh[5];
    cards("Ah Jh 9h 7h 6h", fl, 5); cards("Ks Kd Kc 6s 6d", fh, 5);
    CHECK(evalShortDeck(fl, 5) > evalShortDeck(fh, 5), "short deck: a flush beats a full house");
    cards("6s 7d 8c 9h Ts", h5, 5); cards("Ah Ad Ac Ks Qd", fh, 5);
    CHECK(evalShortDeck(h5, 5) > evalShortDeck(fh, 5), "short deck: a straight beats three of a kind (Wikipedia's ranking)");
    char buf[64];
    handNameV(V_SHORTDECK, evalShortDeck(fl, 5), buf, sizeof buf);
    CHECK(strcmp(buf, "Flush, Ace high") == 0, "short deck flush name '%s'", buf);

    // ace-to-five low (Razz, the low half of hi-lo games)
    CHECK(evV(V_RAZZ, "5h 4h 3h 2h Ah") == evV(V_RAZZ, "5c 4h 3d 2s Ah"), "A-5: flushes don't count");
    CHECK(evV(V_RAZZ, "5c 4h 3d 2s Ah") > evV(V_RAZZ, "6c 4h 3d 2s Ah"), "the wheel is the best low");
    CHECK(evV(V_RAZZ, "6c 4h 3d 2s Ah") > evV(V_RAZZ, "6c 5h 3d 2s Ah"), "6-4 beats 6-5");
    CHECK(evV(V_RAZZ, "Kc Qh Jd Ts 9h") > evV(V_RAZZ, "2c 2h 3d 4s 5h"), "any no-pair hand beats a pair");
    CHECK(evV(V_RAZZ, "3c 3h 4d 5s 6h") > evV(V_RAZZ, "4c 4h 2d 3s 5h"), "lower pair is the better low");
    nameV(V_RAZZ, "As 2d 3c 4h Kd Kc 6s", buf, sizeof buf);
    CHECK(strcmp(buf, "6-4-3-2-A low") == 0, "best five of seven: '%s'", buf);
    nameV(V_RAZZ, "As Ad 2c 2h 3d 3s 4c", buf, sizeof buf);
    CHECK(strcmp(buf, "Pair of Aces") == 0, "seven cards with four ranks make a paired low: '%s'", buf);
    int seen8 = 0;
    static unsigned char hit[1 << 20];
    memset(hit, 0, sizeof hit);
    for (c[0] = 0; c[0] < 52; c[0]++) for (c[1] = c[0] + 1; c[1] < 52; c[1]++) for (c[2] = c[1] + 1; c[2] < 52; c[2]++)
    for (c[3] = c[2] + 1; c[3] < 52; c[3]++) for (c[4] = c[3] + 1; c[4] < 52; c[4]++) {
        int v = evalLowA5(c, 5);
        if (lowQualifies8(v)) { int k = ((1 << 24) - v) & 0xFFFFF; if (!hit[k]) { hit[k] = 1; seen8++; } }
    }
    CHECK(seen8 == 56, "56 different 8-or-better lows (C(8,5)), got %d", seen8);
    int lo[5];
    cards("8c 7h 6d 5s 4h", lo, 5); CHECK(lowQualifies8(evalLowA5(lo, 5)), "8-7-6-5-4 qualifies");
    cards("9c 5h 4d 3s 2h", lo, 5); CHECK(!lowQualifies8(evalLowA5(lo, 5)), "9-5-4-3-2 doesn't qualify");
    cards("8c 8h 4d 3s 2h", lo, 5); CHECK(!lowQualifies8(evalLowA5(lo, 5)), "a pair doesn't qualify");

    // deuce-to-seven
    CHECK(evV(V_TD27, "7h 5d 4c 3s 2h") > evV(V_TD27, "7h 6d 4c 3s 2h"), "2-7: 7-5 beats 7-6");
    CHECK(evV(V_TD27, "Kh Qd Jc Ts 8h") > evV(V_TD27, "Ah 2d 3c 4s 5h"), "2-7: A-2-3-4-5 is ace-high, worse than king-high");
    nameV(V_TD27, "Ah 2d 3c 4s 5h", buf, sizeof buf);
    CHECK(strcmp(buf, "A-5-4-3-2 low") == 0, "2-7 wheel name '%s'", buf);
    CHECK(evV(V_TD27, "Kh Qd Jc Ts 8h") > evV(V_TD27, "6h 5d 4c 3s 2h"), "2-7: a straight counts against you");
    CHECK(evV(V_TD27, "8h 6d 4c 3s 2h") > evV(V_TD27, "7h 5h 4h 3h 2h"), "2-7: a flush counts against you");
    nameV(V_TD27, "6h 5d 4c 3s 2h", buf, sizeof buf);
    CHECK(strcmp(buf, "Straight, Six high") == 0, "2-7 straight name '%s'", buf);
    CHECK(evV(V_TD27, "Ah Ad 3c 4s 5h") < evV(V_TD27, "Ah Kd Qc Js 9h"), "2-7: a pair loses to any no-pair hand");

    // badugi (Wikipedia's example: A-spades 5-diamonds 9-diamonds 9-hearts is a three-card hand)
    nameV(V_BADUGI, "As 5d 9d 9h", buf, sizeof buf);
    CHECK(strcmp(buf, "3-card 9-5-A") == 0, "badugi example '%s'", buf);
    CHECK(evV(V_BADUGI, "Kc Qd Jh Ts") > evV(V_BADUGI, "As 2s 3d 4h"), "any badugi beats any three-card hand");
    CHECK(evV(V_BADUGI, "4c 3d 2h As") > evV(V_BADUGI, "5c 3d 2h As"), "A-2-3-4 is the best badugi");
    CHECK(evV(V_BADUGI, "7c 6d 2h As") > evV(V_BADUGI, "7c 6d 3h As"), "same top card: the next card decides");
    nameV(V_BADUGI, "4c 3d 2h As", buf, sizeof buf);
    CHECK(strcmp(buf, "Badugi 4-3-2-A") == 0, "badugi name '%s'", buf);
    nameV(V_BADUGI, "Kc Kd Ks Kh", buf, sizeof buf);
    CHECK(strcmp(buf, "1-card K") == 0, "four kings is the worst hand: '%s'", buf);

    // Omaha hi-lo: 2 + 3 for each half, and the two halves may use different hole cards
    int h[5], b[5], low;
    cards("Ah 2c Ks Kd", h, 4); cards("Kh 3d 4s 7c Jh", b, 5);
    int hv = evalCards(V_OMAHA8, h, 4, b, 5, &low);
    CHECK(handCat(hv) == HC_TRIPS && low > 0, "O8: three kings for high (KK) and a low with A-2");
    lowName(low, buf, sizeof buf);
    CHECK(strcmp(buf, "7-4-3-2-A low") == 0, "O8 low '%s'", buf);
    cards("Kh 3d 4s Qc Jh", b, 5);
    evalCards(V_OMAHA8, h, 4, b, 5, &low);
    CHECK(low == 0, "O8: only two low cards on the board - no low");
    cards("Ah 2c 3s 4d", h, 4); cards("5h 6d 7s Tc Jh", b, 5);
    evalCards(V_OMAHA8, h, 4, b, 5, &low);
    lowName(low, buf, sizeof buf);
    CHECK(strcmp(buf, "7-6-5-2-A low") == 0, "O8: the best two hole cards for low, '%s'", buf);
    // 5-card Omaha: still exactly two
    cards("Ah Kh Qh Jh Th", h, 5); cards("2h 3c 4d 9s 9c", b, 5);
    CHECK(handCat(evalCards(V_OMAHA5, h, 5, b, 5, NULL)) == HC_PAIR, "Big O: five hearts in hand + one on board is no flush");
    // stud hi-lo: any five for high, any five for low
    int st[7];
    cards("7c 7d 6h 4s 4c 3d 2h", st, 7);
    hv = evalCards(V_STUD8, st, 7, NULL, 0, &low);
    lowName(low, buf, sizeof buf);
    CHECK(handCat(hv) == HC_TWOPAIR && strcmp(buf, "7-6-4-3-2 low") == 0, "Wikipedia's stud hi-lo example: two pair and a 7-6 low ('%s')", buf);
}

// ---------------------------------------------------------------- stud

static void presetDeck(Game *g, const char *s) { int n = nCards(s); cards(s, g->preset, n); g->npreset = n; }

static bool hadEvent(const Game *g, int type, int seat) {
    int from = g->evSeq > MAXEV ? g->evSeq - MAXEV : 0;
    for (int i = from; i < g->evSeq; i++) {
        const GameEvent *e = &g->ev[i % MAXEV];
        if (e->type == type && (seat < 0 || e->seat == seat)) return true;
    }
    return false;
}

static void testStud(void) {
    printf("stud: antes, bring-in, completing, acting order, open pair, cap\n");
    Game g;
    Legal l;
    GameConfig cf = cfgN(3, V_STUD, BL_FIXED, 1000);
    cf.sb = 5; cf.bb = 10;
    gameSetup(&g, &cf);
    g.button = 0;
    // dealt to seats 1, 2, 0: down, down, up. Up cards: seat 1 2d, seat 2 2c, seat 0 Kh. 4th street: 2h, 9c, 3s
    presetDeck(&g, "Ah Ad As Kc Kd Ks 2d 2c Kh 2h 9c 3s");
    gameNewHand(&g);
    CHECK(g.ante == 2 && g.bringIn == 3 && g.smallBet == 10 && g.bigBet == 20, "stakes: ante %d bring-in %d %d/%d", g.ante, g.bringIn, g.smallBet, g.bigBet);
    CHECK(gamePot(&g) == 6 && g.s[0].bet == 0, "everyone antes 2 (pot %d)", gamePot(&g));
    CHECK(g.s[1].nhole == 3 && g.s[1].upMask == 4 && g.ncards == 3, "two down, one up");
    CHECK(g.toAct == 2 && g.bringInSeat == 2, "the 2 of clubs brings in: lowest card, clubs lowest suit (toAct %d)", g.toAct);
    CHECK(hadEvent(&g, EV_BRINGIN, 2), "bring-in event");
    gameLegal(&g, &l);
    CHECK(l.bringIn && l.toCall == 3 && l.minTo == 10 && !l.canCheck, "bring in 3 or complete to 10 (call %d min %d)", l.toCall, l.minTo);
    act(&g, A_FOLD, 0);                      // the bring-in is forced: a fold posts it
    CHECK(g.s[2].bet == 3 && g.curBet == 3 && g.toAct == 0, "brought in for 3, seat 0 next");
    gameLegal(&g, &l);
    CHECK(l.toCall == 3 && l.minTo == 10 && l.maxTo == 10 && l.canRaise, "facing the bring-in: call 3 or complete to 10");
    act(&g, A_RAISE, 999);
    CHECK(g.curBet == 10 && g.nBets == 1 && logHas(&g, "completes to $10"), "completed to 10");
    act(&g, A_RAISE, 0);                     // fixed limit: the amount is ignored
    CHECK(g.curBet == 20, "raise to 20 (%d)", g.curBet);
    act(&g, A_RAISE, 0);
    act(&g, A_RAISE, 0);
    CHECK(g.curBet == 40 && g.nBets == 4 && hadEvent(&g, EV_CAP, 0), "bet + 3 raises = capped at 40");
    gameLegal(&g, &l);
    CHECK(!l.canRaise && l.toCall == 20, "capped: seat 1 can only call 20");
    act(&g, A_RAISE, 0);                     // turns into a call
    act(&g, A_CALL, 0);
    CHECK(g.toAct == -1, "round over");
    gameAdvance(&g);
    CHECK(g.street == 1 && g.s[0].nhole == 4 && g.openPair, "4th street: seat 1 shows a pair of deuces");
    CHECK(g.toAct == 1, "the best hand showing acts first (toAct %d)", g.toAct);
    gameLegal(&g, &l);
    CHECK(l.minTo == 10 && l.maxTo == 20, "open pair: bet 10 or 20 (%d..%d)", l.minTo, l.maxTo);
    act(&g, A_RAISE, 20);
    gameLegal(&g, &l);
    CHECK(g.curBet == 20 && l.minTo == 40, "after the big bet, raises are big bets (%d)", l.minTo);
    act(&g, A_CALL, 0); act(&g, A_CALL, 0);
    gameAdvance(&g);
    CHECK(g.street == 2 && g.betUnit == 20 && !g.openPair, "5th street: big bets");
    for (int k = 0; k < 3; k++) { while (g.toAct >= 0) act(&g, A_CHECK, 0); gameAdvance(&g); }
    CHECK(g.handOver && g.showdown && g.s[0].nhole == 7 && g.s[0].upMask == 0x3C, "7th street comes face down; showdown with 7 cards");

    // Razz: the highest card brings in (the king of spades is the worst), the lowest hand acts first
    GameConfig rz = cfgN(3, V_RAZZ, BL_FIXED, 1000);
    rz.bb = 10;
    gameSetup(&g, &rz);
    g.button = 0;
    presetDeck(&g, "Ah 2d 3s 4c 5d 6s Kh Ks 5c 8h 9c 2s");
    gameNewHand(&g);
    CHECK(g.toAct == 2, "Razz: the king of spades brings in (toAct %d)", g.toAct);
    act(&g, A_CALL, 0); act(&g, A_CALL, 0); act(&g, A_CALL, 0);
    gameAdvance(&g);
    CHECK(g.toAct == 0, "Razz 4th street: 5-2 showing is the lowest and acts first (toAct %d)", g.toAct);

    // heads-up: no cap
    GameConfig hu = cfgN(2, V_STUD, BL_FIXED, 2000);
    hu.bb = 10;
    gameSetup(&g, &hu);
    g.button = 0;
    gameNewHand(&g);
    CHECK(g.capOff, "two players: no cap");
    act(&g, A_RAISE, 0);                     // the bring-in completes
    for (int k = 0; k < 6; k++) act(&g, A_RAISE, 0);
    CHECK(g.nBets == 7 && g.curBet == 70, "heads-up raising keeps going (%d bets, %d)", g.nBets, g.curBet);
}

// ---------------------------------------------------------------- draw games

static bool cardsOk(const Game *g) {
    int seen[52] = { 0 };
    for (int i = 0; i < g->n; i++)
        if (g->s[i].inHand) for (int k = 0; k < g->s[i].nhole; k++) seen[g->s[i].hole[k]]++;
    for (int i = 0; i < g->nboard; i++) seen[g->board[i]]++;
    for (int i = g->dpos; i < g->deckN; i++) seen[g->deck[i]]++;
    for (int i = 0; i < g->ndisc; i++) seen[g->disc[i]]++;
    for (int c = 0; c < 52; c++) if (seen[c] > 1) return false;
    return true;
}

static void testDraw(void) {
    printf("draw games: draws, order, big bets, reshuffling the discards\n");
    Game g;
    Legal l;
    GameConfig cf = cfgN(3, V_TD27, BL_FIXED, 1000);
    cf.sb = 5; cf.bb = 10;
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    CHECK(g.toAct == 0 && g.phase == PH_BET && g.s[0].nhole == 5, "3-handed: the button acts first before the draw");
    act(&g, A_CALL, 0); act(&g, A_CALL, 0); act(&g, A_CHECK, 0);
    CHECK(g.toAct == -1, "pre-draw betting closed");
    gameAdvance(&g);
    gameLegal(&g, &l);
    CHECK(g.phase == PH_DRAW && g.toAct == 1 && l.draw, "first draw: the player left of the button draws first (toAct %d)", g.toAct);
    int before[5];
    memcpy(before, g.s[1].hole, sizeof before);
    gameAct(&g, (Action){ A_DRAW, 3 });           // throw away cards 0 and 1
    CHECK(g.s[1].drew[0] == 2 && g.s[1].hole[2] == before[2] && g.s[1].hole[4] == before[4], "drew 2, kept the rest in place");
    CHECK(g.s[1].hole[0] != before[0] && g.s[1].hole[1] != before[1] && g.ndisc == 2, "the two discards are replaced");
    act(&g, A_CHECK, 0);                           // anything but a draw = stand pat
    CHECK(g.s[2].drew[0] == 0 && g.toAct == 0, "stood pat");
    gameAct(&g, (Action){ A_DRAW, 31 });
    CHECK(g.toAct == -1 && g.s[0].drew[0] == 5 && cardsOk(&g), "everyone drew; no card twice");
    gameAdvance(&g);
    CHECK(g.phase == PH_BET && g.street == 1 && g.toAct == 1 && g.betUnit == 10, "after the 1st draw: small bets, first left of the button");
    act(&g, A_CHECK, 0); act(&g, A_CHECK, 0); act(&g, A_CHECK, 0);
    gameAdvance(&g);
    while (g.toAct >= 0) gameAct(&g, (Action){ A_DRAW, 0 });
    gameAdvance(&g);
    CHECK(g.street == 2 && g.betUnit == 20, "after the 2nd draw: big bets (%d)", g.betUnit);

    // six players drawing five cards every time: the deck runs out and the discards are reshuffled,
    // never giving a player back what they just threw away
    GameConfig six = cfgN(6, V_TD27, BL_FIXED, 1000);
    int resh = 0, bad = 0, own = 0;
    for (int h = 0; h < 40; h++) {
        six.seed = 300 + h;
        gameSetup(&g, &six);
        gameNewHand(&g);
        int guard = 0;
        while (!g.handOver && ++guard < 400) {
            if (g.toAct < 0) { gameAdvance(&g); continue; }
            if (g.phase == PH_DRAW) {
                int p = g.toAct, thrown[5];
                memcpy(thrown, g.s[p].hole, sizeof thrown);
                int seq = g.evSeq;
                gameAct(&g, (Action){ A_DRAW, 31 });
                for (int i = seq; i < g.evSeq; i++) if (g.ev[i % MAXEV].type == EV_RESHUFFLE) resh++;
                for (int a = 0; a < 5; a++) for (int b2 = 0; b2 < 5; b2++) own += g.s[p].hole[a] == thrown[b2];
            } else act(&g, g.toAct % 2 ? A_CALL : A_CHECK, 0);
            if (!cardsOk(&g)) bad++;
        }
    }
    CHECK(resh > 0, "the deck ran out and was reshuffled (%d times)", resh);
    CHECK(bad == 0 && own == 0, "no duplicate cards (%d), nobody got their own discards back (%d)", bad, own);
}

// ---------------------------------------------------------------- hi-lo

static void playDown(Game *g) {
    while (!g->handOver) { if (g->toAct >= 0) act(g, A_CHECK, 0); else gameAdvance(g); }
}

static void testHiLo(void) {
    printf("hi-lo split pots: split, scoop, odd chip, quartering, side pots\n");
    Game g;
    // heads-up Omaha hi-lo: seat 0 has the low, seat 1 the high
    GameConfig cf = cfgN(2, V_OMAHA8, BL_FIXED, 1000);
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    const char *h1[2] = { "Ah 2h Kc Kd", "Qs Qd Js Td" };
    rig(&g, h1, "3s 5d 8c Qh 9h");
    act(&g, A_CALL, 0);
    playDown(&g);
    CHECK(g.pots[0].hiAmt == 2 && g.pots[0].loAmt == 2 && g.s[0].stack == 1000 && g.s[1].stack == 1000, "pot 4 split 2 / 2 (stacks %d %d)", g.s[0].stack, g.s[1].stack);
    CHECK(strstr(g.winText, "High: Carlos") && strstr(g.winText, "Low: Mia"), "headline names both halves: '%s'", g.winText);
    // no low possible: the high hand scoops
    gameSetup(&g, &cf);
    g.button = 0;
    gameNewHand(&g);
    const char *h2[2] = { "Ah 2h Kc Kd", "Qs Qd Js Td" };
    rig(&g, h2, "Ks Jd 9c Qh Th");
    act(&g, A_CALL, 0);
    playDown(&g);
    CHECK(g.pots[0].loAmt == 0 && g.s[0].won == 4, "no qualifying low: high takes it all (won %d)", g.s[0].won);
    CHECK(strstr(g.winText, "no low qualifies") != NULL, "headline: '%s'", g.winText);
    // odd chip goes to the high half: a dead small blind makes the pot 5
    GameConfig c3 = cfgN(3, V_OMAHA8, BL_FIXED, 1000);
    gameSetup(&g, &c3);
    g.button = 0;
    gameNewHand(&g);
    const char *h3[3] = { "Ah 2h Kc Kd", "7c 7d 6c 6d", "Qs Qd Js Td" };
    rig(&g, h3, "3s 5d 8c Qh 9h");
    act(&g, A_CALL, 0); act(&g, A_FOLD, 0); act(&g, A_CHECK, 0);
    playDown(&g);
    CHECK(g.s[2].won == 3 && g.s[0].won == 2, "pot 5: high 3, low 2 (got %d / %d)", g.s[2].won, g.s[0].won);
    // quartering: two players tie for low
    gameSetup(&g, &c3);
    g.button = 0;
    gameNewHand(&g);
    const char *h4[3] = { "Ah 2h Kc Kd", "As 2s 7c 7d", "Qs Qd Js Td" };
    rig(&g, h4, "3s 5d 8c Qh 9h");
    act(&g, A_CALL, 0); act(&g, A_CALL, 0); act(&g, A_CHECK, 0);
    playDown(&g);
    CHECK(g.s[2].won == 3 && g.s[0].won + g.s[1].won == 3 && g.s[0].won >= 1 && g.s[1].won >= 1, "pot 6: high 3, the tied lows share 3 (%d %d %d)", g.s[0].won, g.s[1].won, g.s[2].won);
    // side pot in hi-lo: each pot is split on its own
    gameSetup(&g, &c3);
    g.button = 0;
    gameNewHand(&g);
    g.s[0].stack = 10;
    const char *h5[3] = { "Ah 2h Kc Kd", "Qs Qd Js Td", "9s 9d Tc Th" };
    rig(&g, h5, "3s 5d 8c Qh 4h");
    int before = chips(&g);
    act(&g, A_RAISE, 0); act(&g, A_RAISE, 0); act(&g, A_RAISE, 0);   // button 4, SB 6, BB 8
    act(&g, A_CALL, 0); act(&g, A_CALL, 0);
    gameAdvance(&g);
    for (int k = 0; k < 6 && !g.handOver; k++) {
        if (g.toAct < 0) { gameAdvance(&g); continue; }
        act(&g, g.s[g.toAct].stack > 0 ? A_RAISE : A_CALL, 0);
    }
    playDown(&g);
    CHECK(chips(&g) == before && g.npots == 2, "side pots in hi-lo conserve chips (%d pots)", g.npots);
    CHECK(g.pots[0].loAmt > 0 && (g.pots[0].loMask & 1) && !(g.pots[1].eligible & 1), "the short stack wins low in the main pot only");
}

// ---------------------------------------------------------------- mixed games

static void testMixed(void) {
    printf("mixed games: HORSE and 8-Game rotations\n");
    Game g;
    GameConfig cf = cfgN(3, V_HOLDEM, BL_NOLIMIT, 1000);
    cf.mix = MIX_HORSE;
    gameSetup(&g, &cf);
    CHECK(g.variant == V_HOLDEM && g.limit == BL_FIXED, "HORSE starts with limit Hold'em");
    int order[16];
    for (int h = 0; h < 16; h++) {
        gameNewHand(&g);
        order[h] = g.variant;
        playDown(&g);
    }
    CHECK(order[2] == V_HOLDEM && order[3] == V_OMAHA8 && order[6] == V_RAZZ && order[9] == V_STUD && order[12] == V_STUD8 && order[15] == V_HOLDEM,
          "one orbit of each: H O R S E then back to H (%d %d %d %d %d %d)", order[2], order[3], order[6], order[9], order[12], order[15]);
    cf.mix = MIX_8GAME;
    gameSetup(&g, &cf);
    int lims[24], vars[24];
    for (int h = 0; h < 24; h++) { gameNewHand(&g); vars[h] = g.variant; lims[h] = g.limit; playDown(&g); }
    CHECK(vars[0] == V_TD27 && vars[18] == V_HOLDEM && lims[18] == BL_NOLIMIT && vars[21] == V_OMAHA && lims[21] == BL_POTLIMIT,
          "8-Game: starts with 2-7 triple draw, then NL Hold'em and PLO at the end");
}

// ---------------------------------------------------------------- every game and structure

static void testFuzzAll(int hands) {
    printf("fuzz every game x betting structure (%d hands each)\n", hands);
    Game *g = malloc(sizeof *g);
    Rng r;
    rngSeed(&r, 4242);
    for (int v = 0; v < V_COUNT; v++)
        for (int lim = 0; lim < BL_COUNT; lim++) {
            if (!(varInfo(v)->limits >> lim & 1)) continue;
            int badState = 0, badChips = 0, stuck = 0, badCards = 0, badEnd = 0, badCap = 0;
            for (int n = 2; n <= 6; n++) {
                GameConfig cf = cfgN(n, v, lim, 60 + 37 * n);
                cf.sb = 5; cf.bb = 10;
                cf.buyin = 300 + 70 * n;
                cf.seed = 77 + n + v * 13 + lim;
                gameSetup(g, &cf);
                for (int h = 0; h < hands / 5; h++) {
                    for (int i = 0; i < n; i++) if (rngInt(&r, 25) == 0) g->s[i].stack = 1 + rngInt(&r, 30);
                    gameNewHand(g);
                    int start = chips(g), guard = 0;
                    if (!cardsOk(g)) badCards++;
                    while (!g->handOver) {
                        if (++guard > 500) { stuck++; break; }
                        if (g->toAct < 0) { gameAdvance(g); if (!cardsOk(g)) badCards++; continue; }
                        const Seat *s = &g->s[g->toAct];
                        Legal l;
                        gameLegal(g, &l);
                        if (g->phase == PH_DRAW) {
                            if (!l.draw || !s->inHand) badState++;
                            gameAct(g, (Action){ A_DRAW, rngInt(&r, 1 << s->nhole) });
                            if (!cardsOk(g)) badCards++;
                            continue;
                        }
                        if (!s->inHand || s->allin) badState++;
                        if (l.toCall < 0 || l.minTo > l.maxTo || l.maxTo > s->stack + s->bet || (l.canRaise && l.minTo <= g->curBet)) badState++;
                        if (lim == BL_FIXED && l.canRaise && l.minTo != l.maxTo && !g->openPair && !l.bringIn) badState++;
                        if (lim == BL_FIXED && !g->capOff && g->nBets > 4) badCap++;
                        int t = rngInt(&r, 10);
                        Action a = { t < 2 ? A_FOLD : t < 6 ? (l.canCheck ? A_CHECK : A_CALL) : A_RAISE, 0 };
                        a.amount = rngInt(&r, 3) == 0 ? l.maxTo : l.minTo + rngInt(&r, l.maxTo - l.minTo + 1);
                        if (rngInt(&r, 20) == 0) a.amount = rngInt(&r, 5000) - 100;
                        gameAct(g, a);
                        for (int i = 0; i < n; i++) if (g->s[i].stack < 0 || g->s[i].bet < 0) badState++;
                        if (chips(g) != start) badChips++;
                    }
                    int after = 0;
                    for (int i = 0; i < n; i++) after += g->s[i].stack;
                    if (after != start) badChips++;
                    if (g->showdown) {
                        const VariantInfo *vi = varInfo(v);
                        for (int i = 0; i < n; i++)
                            if (g->s[i].inHand && g->s[i].nhole != vi->nhole) badEnd++;
                        if (vi->family == FAM_BOARD && g->nboard != 5) badEnd++;
                    }
                }
            }
            const char *nm = varInfo(v)->shortName, *ln = limitShort(lim);
            CHECK(badState == 0, "%s %s: invalid states %d", nm, ln, badState);
            CHECK(badChips == 0, "%s %s: chip conservation failures %d", nm, ln, badChips);
            CHECK(stuck == 0, "%s %s: hands that never finished %d", nm, ln, stuck);
            CHECK(badCards == 0, "%s %s: duplicate cards %d", nm, ln, badCards);
            CHECK(badEnd == 0, "%s %s: wrong card counts at showdown %d", nm, ln, badEnd);
            CHECK(badCap == 0, "%s %s: more than a bet and three raises %d", nm, ln, badCap);
        }
    free(g);
}

int main(void) {
    rangesInit();
    testEvaluator();
    testRanges();
    testRules();
    testShortAllin();
    testUncalled();
    testPotLimit();
    testSidePots();
    testShowdownBoardPlays();
    testFuzz(V_HOLDEM, BL_NOLIMIT, 20000);
    testFuzz(V_HOLDEM, BL_POTLIMIT, 10000);
    testFuzz(V_OMAHA, BL_POTLIMIT, 10000);
    testFuzz(V_OMAHA, BL_NOLIMIT, 5000);
    testEvalVariants();
    testStud();
    testDraw();
    testHiLo();
    testMixed();
    testFuzzAll(2000);
    testEquity();
    testCoachPreflop();
    testCoachPostflop();
    testCoachDetails();
    testSolver();
    testCoachShortCall();
    testCoachFuzz(V_HOLDEM, BL_NOLIMIT, 1500);
    testCoachFuzz(V_HOLDEM, BL_POTLIMIT, 1000);
    testCoachFuzz(V_OMAHA, BL_POTLIMIT, 500);
    testCoachFuzz(V_OMAHA, BL_NOLIMIT, 300);
    testBots();
    testArrange();
    testGlossary();
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
