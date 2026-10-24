// main.c - raylib front end for the Hold'em / Omaha poker trainer
#include "raylib.h"
#include "ai.h"
#include "range.h"
#include "arrange.h"
#include "glossary.h"
#include "sound.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#define VW 1600          // virtual canvas
#define VH 900
#define PX 1180          // side panel
#define PW 405
#define TCX 585          // table centre
#define TCY 395

enum { MODE_PLAY, MODE_TRAIN };

// ---------------------------------------------------------------- globals

static Font fBold, fReg;
static bool hasUI = false;
static Camera2D cam;
static Vector2 mouse;
static int gLayer = 0, gActive = 0;

static Game game;
static bool gameStarted = false;
static Rng uiRng;
static int gameMode = MODE_PLAY;          // mode of the running game

// settings (menu)
static int setVariant = V_HOLDEM, setMode = MODE_TRAIN, setLimit = BL_NOLIMIT, setOpp = 5;
static int setLv[MAXP] = { LV_HUMAN, LV_MEDIUM, LV_MEDIUM, LV_MEDIUM, LV_MEDIUM, LV_MEDIUM };
static const int BLINDS[][2] = { { 1, 2 }, { 2, 5 }, { 5, 10 }, { 10, 20 }, { 25, 50 }, { 50, 100 }, { 100, 200 }, { 250, 500 } };
#define NBLINDS 8
static const int BUYINS[] = { 20, 30, 40, 50, 75, 100, 150, 200, 250, 300 };
#define NBUYINS 10
static int blindIdx = 0, buyIdx = 5, speedIdx = 1;
static bool fourColor = false, autoDeal = true, hintsBefore = true;
static const float SPEEDS[3] = { 1.2f, 0.7f, 0.3f };

static bool showMenu = true, showHelp = false, showLessons = false, showChart = false, showGloss = false;
static int lessonPage = 0, chartTab = 0, panelTab = 0;
static float stepTimer = 0, animT = 0, handOverT = 0;
static float whyScroll = 0, logScroll = 0, helpScroll = 0, glossScroll = 0;
static int glossCat = -1, glossFocus = -1;      // category filter (-1 all), entry to jump to and highlight
static bool glossJump = false;
static char glossQ[32];
static char think[NTHINK][220];                 // quiz-mode checklist for the current decision
static int nthink = 0;

// betting controls
static int raiseAmt = 0, raiseKey = -1;
static bool dragSlider = false;

// coach
typedef struct { int street; char did[64]; char best[96]; int q; } Review;
static Advice adv, lastAdv;
static int advKey = -1;
static bool lastValid = false;
static int lastQ = -1;
static char lastDid[96], lastNote[200];
static Review review[24];
static int nreview = 0;
static int decisions = 0, qCount[5];
enum { LK_LOOSE, LK_TIGHT, LK_PASSIVE_PRE, LK_BADCALL, LK_BADFOLD, LK_MISSVALUE, LK_OVERBLUFF, LK_SLOWRAISE, LK_SIZING, LK_N };
static int leaks[LK_N];
static const char *LEAK_NAME[LK_N] = {
    "Playing too many hands preflop", "Folding too much preflop", "Limping / flat-calling instead of raising",
    "Calling without the odds", "Folding hands with enough equity", "Missing value bets",
    "Betting / bluffing too much", "Just calling with strong hands", "Bet sizing",
};
static int handsPlayed = 0, biggestPot = 0;
static bool handCounted = false;          // this hand's result is in the session stats

// hero card arrangement (display order only - the engine never depends on it)
static Arrange arr;
static int orderHand = -1;
static int sortPref = SORT_NONE;          // remembered and applied to every new hand
static Vector2 cardGrab;                  // where on the card the pointer grabbed it

#ifdef AUTOTEST
static bool fakeMouse = false, fakeDown = false, fakeDownPrev = false;
static Vector2 fakePos;
static bool MDown(void) { return fakeMouse ? fakeDown : IsMouseButtonDown(MOUSE_BUTTON_LEFT); }
static bool MPressed(void) { return fakeMouse ? fakeDown && !fakeDownPrev : IsMouseButtonPressed(MOUSE_BUTTON_LEFT); }
#else
static bool MDown(void) { return IsMouseButtonDown(MOUSE_BUTTON_LEFT); }
static bool MPressed(void) { return IsMouseButtonPressed(MOUSE_BUTTON_LEFT); }
#endif

static char flashMsg[160];
static float flashT = 0;

// palette
static const Color C_BG      = { 14, 22, 30, 255 };
static const Color C_FELT    = { 26, 102, 72, 255 };
static const Color C_FELT2   = { 20, 84, 60, 255 };
static const Color C_RAIL    = { 58, 38, 28, 255 };
static const Color C_PAPER   = { 248, 245, 236, 255 };
static const Color C_INK     = { 34, 36, 44, 255 };
static const Color C_MUTED   = { 118, 116, 108, 255 };
static const Color C_GOLD    = { 240, 196, 70, 255 };
static const Color C_RED     = { 200, 36, 44, 255 };
static const Color C_BLUE    = { 36, 92, 196, 255 };
static const Color C_GREEN   = { 22, 128, 70, 255 };
static const Color C_PANEL   = { 24, 32, 42, 255 };
static const Color C_PANEL2  = { 34, 44, 58, 255 };
static const Color C_TEXT    = { 226, 230, 236, 255 };
static const Color C_DIM     = { 150, 160, 172, 255 };
static const Color Q_COL[5]  = { { 60, 190, 110, 255 }, { 120, 200, 90, 255 }, { 230, 180, 60, 255 }, { 235, 120, 50, 255 }, { 220, 60, 60, 255 } };
static const Color LV_COL[4] = { { 90, 90, 90, 255 }, { 50, 150, 90, 255 }, { 46, 110, 190, 255 }, { 190, 60, 60, 255 } };

// ---------------------------------------------------------------- text & widgets

static void Txt(Font f, const char *s, float x, float y, float sz, Color c) {
    DrawTextEx(f, s, (Vector2){ x, y }, sz, hasUI ? 0.0f : sz / 10.0f, c);
}
static float TxtW(Font f, const char *s, float sz) { return MeasureTextEx(f, s, sz, hasUI ? 0.0f : sz / 10.0f).x; }
static void TxtC(Font f, const char *s, float cx, float y, float sz, Color c) { Txt(f, s, cx - TxtW(f, s, sz) / 2, y, sz, c); }
static void TxtR(Font f, const char *s, float rx, float y, float sz, Color c) { Txt(f, s, rx - TxtW(f, s, sz), y, sz, c); }
static void Flash(const char *msg) { snprintf(flashMsg, sizeof flashMsg, "%s", msg); flashT = 3.0f; }

// word-wrapped text; returns the height used
static float Wrap(Font f, const char *s, float x, float y, float w, float sz, float lh, Color c, bool draw) {
    char line[600] = "", test[700];
    float yy = y;
    const char *p = s;
    while (*p) {
        if (*p == '\n') { if (draw && line[0]) Txt(f, line, x, yy, sz, c); yy += lh; line[0] = 0; p++; continue; }
        const char *q = p;
        while (*q && *q != ' ' && *q != '\n') q++;
        int wl = (int)(q - p);
        snprintf(test, sizeof test, "%s%s%.*s", line, line[0] ? " " : "", wl, p);
        if (line[0] && TxtW(f, test, sz) > w) {
            if (draw) Txt(f, line, x, yy, sz, c);
            yy += lh;
            snprintf(line, sizeof line, "%.*s", wl, p);
        } else snprintf(line, sizeof line, "%s", test);
        p = q;
        while (*p == ' ') p++;
    }
    if (line[0]) { if (draw) Txt(f, line, x, yy, sz, c); yy += lh; }
    return yy - y;
}

static bool Hover(Rectangle r) { return gLayer == gActive && CheckCollisionPointRec(mouse, r); }
static bool KeyHit(int k) { return gLayer == gActive && IsKeyPressed(k); }

static bool Button(Rectangle r, const char *label, bool enabled, Color base) {
    bool hov = enabled && Hover(r);
    Color c = enabled ? base : (Color){ 95, 100, 108, 255 };
    if (hov) c = ColorBrightness(c, 0.15f);
    DrawRectangleRounded((Rectangle){ r.x, r.y + 3, r.width, r.height }, 0.35f, 8, Fade(BLACK, 0.3f));
    DrawRectangleRounded(r, 0.35f, 8, c);
    DrawRectangleRoundedLinesEx(r, 0.35f, 8, 1.5f, Fade(WHITE, enabled ? 0.35f : 0.12f));
    float sz = r.height * 0.45f;
    if (sz > 21) sz = 21;
    while (sz > 11 && TxtW(fBold, label, sz) > r.width - 10) sz -= 1;
    TxtC(fBold, label, r.x + r.width / 2, r.y + (r.height - sz) / 2, sz, enabled ? WHITE : (Color){ 180, 180, 185, 255 });
    return hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void ClipBegin(Rectangle r) {
    Vector2 a = GetWorldToScreen2D((Vector2){ r.x, r.y }, cam), b = GetWorldToScreen2D((Vector2){ r.x + r.width, r.y + r.height }, cam);
    BeginScissorMode((int)a.x, (int)a.y, (int)(b.x - a.x), (int)(b.y - a.y));
}

// ---------------------------------------------------------------- glossary tooltips

static int tipEntry = -1;           // glossary entry under the pointer this frame

static void TipIdx(Rectangle r, int i) { if (i >= 0 && Hover(r)) tipEntry = i; }
static void Tip(Rectangle r, const char *term) { if (Hover(r)) TipIdx(r, glossLookup(term)); }

static void DrawTip(void) {
    if (tipEntry < 0) return;
    const GlossEntry *e = glossGet(tipEntry);
    tipEntry = -1;
    float w = 400, x = mouse.x + 18, y = mouse.y + 20;
    float h = Wrap(fReg, e->def, 0, 0, w - 28, 15, 21, C_TEXT, false) + 48;
    if (x + w > VW - 8) x = mouse.x - w - 12;
    if (y + h > VH - 8) y = mouse.y - h - 12;
    if (x < 8) x = 8;
    if (y < 8) y = 8;
    DrawRectangleRounded((Rectangle){ x + 3, y + 5, w, h }, 0.08f, 8, Fade(BLACK, 0.45f));
    DrawRectangleRounded((Rectangle){ x, y, w, h }, 0.08f, 8, (Color){ 20, 26, 36, 250 });
    DrawRectangleRoundedLinesEx((Rectangle){ x, y, w, h }, 0.08f, 8, 1.5f, C_GOLD);
    Txt(fBold, e->term, x + 14, y + 10, 17, C_GOLD);
    Wrap(fReg, e->def, x + 14, y + 36, w - 28, 15, 21, C_TEXT, true);
}

static void OpenGlossary(int focus) {
    showGloss = true;
    glossFocus = focus;
    glossJump = focus >= 0;
    glossQ[0] = 0;
    glossCat = -1;
    glossScroll = 0;
    while (GetCharPressed()) {}          // the key that opened it isn't a search letter
}

static const char *Money(int v) {
    static char buf[8][32];
    static int k = 0;
    char *b = buf[k++ & 7];
    fmtChips(v, b, 32);
    return b;
}

// ---------------------------------------------------------------- cards

static void Tri(Vector2 a, Vector2 b, Vector2 c, Color col) { DrawTriangle(a, b, c, col); DrawTriangle(a, c, b, col); }

static Color SuitCol(int s) {
    if (fourColor) { static const Color F[4] = { { 30, 130, 60, 255 }, { 30, 90, 200, 255 }, { 200, 30, 40, 255 }, { 25, 25, 30, 255 } }; return F[s]; }
    return s == 1 || s == 2 ? C_RED : (Color){ 25, 25, 30, 255 };
}

static void DrawSuit(int s, float cx, float cy, float r, Color c) {
    switch (s) {
    case 2:   // heart
        DrawCircleV((Vector2){ cx - r * 0.48f, cy - r * 0.25f }, r * 0.52f, c);
        DrawCircleV((Vector2){ cx + r * 0.48f, cy - r * 0.25f }, r * 0.52f, c);
        Tri((Vector2){ cx - r * 0.98f, cy - r * 0.1f }, (Vector2){ cx + r * 0.98f, cy - r * 0.1f }, (Vector2){ cx, cy + r * 0.95f }, c);
        break;
    case 1:   // diamond
        Tri((Vector2){ cx, cy - r }, (Vector2){ cx - r * 0.72f, cy }, (Vector2){ cx + r * 0.72f, cy }, c);
        Tri((Vector2){ cx, cy + r }, (Vector2){ cx - r * 0.72f, cy }, (Vector2){ cx + r * 0.72f, cy }, c);
        break;
    case 3:   // spade
        DrawCircleV((Vector2){ cx - r * 0.47f, cy + r * 0.2f }, r * 0.5f, c);
        DrawCircleV((Vector2){ cx + r * 0.47f, cy + r * 0.2f }, r * 0.5f, c);
        Tri((Vector2){ cx - r * 0.95f, cy + r * 0.08f }, (Vector2){ cx + r * 0.95f, cy + r * 0.08f }, (Vector2){ cx, cy - r }, c);
        Tri((Vector2){ cx, cy + r * 0.2f }, (Vector2){ cx - r * 0.38f, cy + r }, (Vector2){ cx + r * 0.38f, cy + r }, c);
        break;
    default:  // club
        DrawCircleV((Vector2){ cx, cy - r * 0.45f }, r * 0.45f, c);
        DrawCircleV((Vector2){ cx - r * 0.5f, cy + r * 0.15f }, r * 0.45f, c);
        DrawCircleV((Vector2){ cx + r * 0.5f, cy + r * 0.15f }, r * 0.45f, c);
        Tri((Vector2){ cx, cy - r * 0.1f }, (Vector2){ cx - r * 0.38f, cy + r }, (Vector2){ cx + r * 0.38f, cy + r }, c);
        break;
    }
}

static void RankText(int rank, char *buf) {
    if (rank == 8) snprintf(buf, 4, "10");
    else { buf[0] = RANKCH[rank]; buf[1] = 0; }
}

static void DrawCardBack(Rectangle r) {
    DrawRectangleRounded((Rectangle){ r.x + 2, r.y + 3, r.width, r.height }, 0.14f, 6, Fade(BLACK, 0.35f));
    DrawRectangleRounded(r, 0.14f, 6, (Color){ 245, 245, 245, 255 });
    Rectangle in = { r.x + r.width * 0.08f, r.y + r.width * 0.08f, r.width * 0.84f, r.height - r.width * 0.16f };
    DrawRectangleRounded(in, 0.12f, 6, (Color){ 150, 32, 44, 255 });
    float step = r.width * 0.18f;
    BeginBlendMode(BLEND_ALPHA);
    for (float d = -in.height; d < in.width; d += step) {
        float x0 = in.x + d, y0 = in.y, x1 = in.x + d + in.height, y1 = in.y + in.height;
        if (x0 < in.x) { y0 += in.x - x0; x0 = in.x; }
        if (x1 > in.x + in.width) { y1 -= x1 - (in.x + in.width); x1 = in.x + in.width; }
        if (y1 > y0) DrawLineEx((Vector2){ x0, y0 }, (Vector2){ x1, y1 }, 1.5f, Fade(WHITE, 0.18f));
    }
    EndBlendMode();
    DrawRectangleRoundedLinesEx(in, 0.12f, 6, 1.5f, Fade(WHITE, 0.6f));
}

static void DrawCard(int c, Rectangle r, bool dim, bool hilite) {
    DrawRectangleRounded((Rectangle){ r.x + 2, r.y + 3, r.width, r.height }, 0.14f, 6, Fade(BLACK, 0.35f));
    DrawRectangleRounded(r, 0.14f, 6, (Color){ 253, 252, 248, 255 });
    DrawRectangleRoundedLinesEx(r, 0.14f, 6, 1.0f, (Color){ 190, 190, 196, 255 });
    int rank = cardRank(c), s = cardSuit(c);
    Color col = SuitCol(s);
    char rt[4];
    RankText(rank, rt);
    if (r.height >= 80) {
        float sz = r.height * 0.27f;
        Txt(fBold, rt, r.x + r.width * 0.09f, r.y + r.height * 0.04f, sz, col);
        DrawSuit(s, r.x + r.width * 0.09f + TxtW(fBold, rt, sz) / 2, r.y + r.height * 0.4f, r.width * 0.09f, col);
        DrawSuit(s, r.x + r.width * 0.6f, r.y + r.height * 0.66f, r.width * 0.27f, col);
    } else {
        float sz = r.height * 0.42f;
        TxtC(fBold, rt, r.x + r.width / 2, r.y + r.height * 0.07f, sz, col);
        DrawSuit(s, r.x + r.width / 2, r.y + r.height * 0.7f, r.width * 0.22f, col);
    }
    if (dim) DrawRectangleRounded(r, 0.14f, 6, Fade(BLACK, 0.45f));
    if (hilite) DrawRectangleRoundedLinesEx((Rectangle){ r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, 0.14f, 6, 3, C_GOLD);
}

static void DrawChips(float cx, float cy, int amount) {
    static const Color CC[4] = { { 220, 220, 225, 255 }, { 200, 40, 50, 255 }, { 40, 110, 200, 255 }, { 30, 30, 34, 255 } };
    int n = 1 + (amount / (game.bbAmt * 2 > 0 ? game.bbAmt * 2 : 1));
    if (n > 5) n = 5;
    for (int i = 0; i < n; i++) {
        Color c = CC[(amount / game.bbAmt + i) % 4];
        float y = cy - i * 4;
        DrawEllipse((int)cx, (int)y + 2, 13, 6, Fade(BLACK, 0.35f));
        DrawEllipse((int)cx, (int)y, 13, 6, c);
        DrawEllipseLines((int)cx, (int)y, 13, 6, Fade(WHITE, 0.6f));
    }
    const char *t = Money(amount);
    float w = TxtW(fBold, t, 16);
    DrawRectangleRounded((Rectangle){ cx + 17, cy - 11, w + 12, 22 }, 0.5f, 6, Fade(BLACK, 0.55f));
    Txt(fBold, t, cx + 23, cy - 8, 16, C_GOLD);
}

// ---------------------------------------------------------------- game flow

static float BotDelay(void) { return SPEEDS[speedIdx]; }

static int Invested(void) { return game.s[game.human].buyins * game.buyin; }

static int HeroChips(void) {
    const Seat *h = &game.s[game.human];
    return h->stack + (game.handOver ? 0 : h->total);
}

static void ResetCoach(void) {
    advKey = -1;
    lastValid = false;
    lastQ = -1;
    nreview = 0;
}

// Seed material. GetTime() only counts from startup (and browsers coarsen it), so on its
// own two players who click Start equally fast get the same cards; mix in the wall clock
// and, in the browser, Math.random().
static uint64_t Entropy(void) {
    uint64_t e = (uint64_t)time(NULL) * 0x9E3779B97F4A7C15ull ^ (uint64_t)(GetTime() * 1e6);
#ifdef __EMSCRIPTEN__
    e ^= (uint64_t)(emscripten_random() * 4294967296.0) << 24;
#endif
    return e;
}

static void DealHoleCards(void) {
    gameNewHand(&game);
    int dealt = 0;
    for (int i = 0; i < game.n; i++) if (game.s[i].inHand) dealt++;
    sndDeal(dealt * holeCount(&game));
}

static void StartNewGame(void) {
    GameConfig cfg = { 0 };
    cfg.variant = setVariant;
    cfg.limit = setLimit;
    cfg.n = setOpp + 1;
    for (int i = 0; i < MAXP; i++) cfg.levels[i] = i == 0 ? LV_HUMAN : setLv[i];
    cfg.sb = BLINDS[blindIdx][0];
    cfg.bb = BLINDS[blindIdx][1];
    cfg.buyin = BUYINS[buyIdx] * cfg.bb;
    cfg.human = 0;
    cfg.seed = Entropy() ^ ((uint64_t)GetRandomValue(0, 1 << 30) << 20);
    gameSetup(&game, &cfg);
    rngSeed(&uiRng, cfg.seed ^ 0xC0FFEEull);
    gameMode = setMode;
    gameStarted = true;
    showMenu = false;
    decisions = handsPlayed = biggestPot = 0;
    memset(qCount, 0, sizeof qCount);
    memset(leaks, 0, sizeof leaks);
    ResetCoach();
    panelTab = gameMode == MODE_TRAIN ? 0 : 1;
    DealHoleCards();
    stepTimer = BotDelay();
    handOverT = 0;
    handCounted = false;
}

static void NextHand(void) {
    if (game.s[game.human].stack <= 0) return;
    ResetCoach();
    DealHoleCards();
    stepTimer = BotDelay();
    handOverT = 0;
    handCounted = false;
    whyScroll = 0;
}

static void Rebuy(void) {
    Seat *h = &game.s[game.human];
    h->stack += game.buyin;
    h->buyins++;
    gameLog(&game, "You rebuy for %s.", Money(game.buyin));
    NextHand();
}

static int DecisionKey(void) { return game.handNo * 1000 + game.nActs; }

static void EnsureAdvice(void) {
    if (gameMode != MODE_TRAIN || game.toAct != game.human || game.handOver) return;
    int key = DecisionKey();
    if (advKey == key) return;
    advise(&game, game.human, &adv, 2500, &uiRng);
    nthink = thinkPrompts(&game, game.human, think);
    advKey = key;
    whyScroll = 0;
}

static void ActionText(const Game *g, Action a, char *buf, int n) {
    Legal l;
    gameLegal(g, &l);
    switch (a.type) {
    case A_FOLD: snprintf(buf, n, "Fold"); break;
    case A_CHECK: snprintf(buf, n, "Check"); break;
    case A_CALL: snprintf(buf, n, "Call %s", Money(l.toCall)); break;
    default: snprintf(buf, n, "%s %s", g->curBet == 0 ? "Bet" : "Raise to", Money(a.amount)); break;
    }
}

static void GradeHuman(Action a) {
    if (gameMode != MODE_TRAIN || advKey != DecisionKey()) return;
    Legal l;
    gameLegal(&game, &l);
    if (a.type == A_FOLD && l.canCheck) a.type = A_CHECK;
    int q = adv.q[a.type];
    if (q < 0) q = Q_MISTAKE;
    lastNote[0] = 0;
    if (a.type == A_RAISE && q <= Q_GOOD && adv.lo > 0 && (a.amount < adv.lo || a.amount > adv.hi)) {
        bool allin = a.amount >= game.s[game.human].stack + game.s[game.human].bet;
        bool recAllin = adv.aggAmount >= game.s[game.human].stack + game.s[game.human].bet;
        if (!(allin && recAllin)) {
            q = Q_INACC;
            snprintf(lastNote, sizeof lastNote, "Right idea, but size it around %s (%s - %s works).", Money(adv.aggAmount), Money(adv.lo), Money(adv.hi));
            leaks[LK_SIZING]++;
        }
    }
    bool pre = game.street == ST_PREFLOP;
    int rec = adv.act;
    if (q >= Q_MISTAKE) {
        if (pre) {
            if ((a.type == A_CALL || a.type == A_RAISE) && (rec == A_FOLD || rec == A_CHECK)) leaks[LK_LOOSE]++;
            else if (a.type == A_FOLD) leaks[LK_TIGHT]++;
            else if (a.type == A_CALL && rec == A_RAISE) leaks[LK_PASSIVE_PRE]++;
        } else {
            if (a.type == A_CALL && rec == A_FOLD) leaks[LK_BADCALL]++;
            else if (a.type == A_FOLD) leaks[LK_BADFOLD]++;
            else if (a.type == A_CHECK && rec == A_RAISE) leaks[LK_MISSVALUE]++;
            else if (a.type == A_RAISE && (rec == A_CHECK || rec == A_FOLD || rec == A_CALL)) leaks[LK_OVERBLUFF]++;
            else if (a.type == A_CALL && rec == A_RAISE) leaks[LK_SLOWRAISE]++;
        }
    } else if (q == Q_INACC && !pre && a.type == A_CHECK && rec == A_RAISE && adv.equity > 0.6f) leaks[LK_MISSVALUE]++;
    decisions++;
    qCount[q]++;
    int st = game.street > ST_RIVER ? ST_RIVER : game.street;
    lastAdv = adv;
    lastValid = true;
    lastQ = q;
    ActionText(&game, a, lastDid, sizeof lastDid);
    if (nreview < 24) {
        Review *r = &review[nreview++];
        r->street = st;
        snprintf(r->did, sizeof r->did, "%s", lastDid);
        snprintf(r->best, sizeof r->best, "%s", adv.head);
        r->q = q;
    }
    if (!hintsBefore || q >= Q_INACC) {
        char m[160];
        snprintf(m, sizeof m, "%s: %s  -  coach: %s", qualityName(q), lastDid, adv.head);
        Flash(m);
    }
}

static void HumanAct(Action a) {
    EnsureAdvice();
    GradeHuman(a);
    gameAct(&game, a);
    stepTimer = BotDelay();
}

static void UpdateGame(float dt) {
    if (game.handOver) {
        if (!handCounted) {
            handCounted = true;
            handsPlayed++;
            int pot = 0;
            for (int i = 0; i < game.npots; i++) pot += game.potAmt[i];
            if (game.s[game.human].won > 0 && pot > biggestPot) biggestPot = pot;
            const Seat *h = &game.s[game.human];
            if (h->won > h->total) sndWin();
            else if (h->won < h->total && h->inHand) sndLose();   // folding is your own choice: no jeer
        }
        handOverT += dt;
        float wait = gameMode == MODE_TRAIN ? 9999 : game.showdown ? 4.0f : 2.2f;
        if (autoDeal && handOverT > wait && game.s[game.human].stack > 0) NextHand();
        return;
    }
    if (game.toAct == game.human) { EnsureAdvice(); return; }
    stepTimer -= dt;
    if (stepTimer > 0) return;
    if (game.toAct < 0) {
        int before = game.nboard;
        gameAdvance(&game);
        sndDeal(game.nboard - before);
        stepTimer = BotDelay() * (game.toAct < 0 && !game.handOver ? 1.3f : 0.8f);
    } else {
        Action a = aiDecide(&game, game.toAct, &uiRng);
        gameAct(&game, a);
        stepTimer = BotDelay();
    }
    if (game.handOver) handOverT = 0;
    EnsureAdvice();
}

// ---------------------------------------------------------------- table

static const Vector2 SPOT[6] = { { TCX, 722 }, { 150, 540 }, { 190, 190 }, { TCX, 92 }, { 980, 190 }, { 1020, 540 } };
static int SpotFor(int seat) {
    static const int MAP[7][6] = { { 0 }, { 0 }, { 0, 3 }, { 0, 2, 4 }, { 0, 1, 3, 5 }, { 0, 1, 2, 4, 5 }, { 0, 1, 2, 3, 4, 5 } };
    return MAP[game.n][seat];
}

static void ActLabel(const Seat *s, char *buf, int n) {
    buf[0] = 0;
    switch (s->lastAct) {
    case A_FOLD: snprintf(buf, n, "Fold"); break;
    case A_CHECK: snprintf(buf, n, "Check"); break;
    case A_CALL: snprintf(buf, n, s->allin ? "Call - All-in" : "Call"); break;
    case A_RAISE: snprintf(buf, n, s->allin ? "All-in" : s->lastBet ? "Bet" : "Raise"); break;
    }
}

static Rectangle HeroSlot(Vector2 sp, int k) {
    int hc = holeCount(&game);
    float cw = hc == 2 ? 86 : 74, ch = hc == 2 ? 120 : 104, gap = hc == 2 ? 94 : 66;
    float x0 = sp.x - (gap * (hc - 1) + cw) / 2;
    return (Rectangle){ x0 + k * gap, sp.y - 160 + (hc == 4 ? 14 : 0), cw, ch };
}

// draws your cards and lets you rearrange them: drag a card, or click two cards to swap them
static void HeroCards(const Seat *s, Vector2 sp) {
    int hc = holeCount(&game);
    if (orderHand != game.handNo) {
        orderHand = game.handNo;
        arrReset(&arr, hc);
        arrSort(&arr, s->hole, sortPref);
    }
    bool active = gLayer == gActive;
    int under = -1;
    for (int k = hc - 1; k >= 0 && active; k--)          // later cards overlap earlier ones
        if (CheckCollisionPointRec(mouse, HeroSlot(sp, k))) { under = k; break; }
    bool pressed = active && MPressed();
    if (pressed && under >= 0) {
        Rectangle r = HeroSlot(sp, under);
        cardGrab = (Vector2){ mouse.x - r.x, mouse.y - r.y };
    }
    int target = -1;                                      // slot the dragged card would land in
    if (arr.dragging) {
        float cx = mouse.x - cardGrab.x + HeroSlot(sp, 0).width / 2, best = 1e9f;
        for (int k = 0; k < hc; k++) {
            Rectangle r = HeroSlot(sp, k);
            float d = fabsf(r.x + r.width / 2 - cx);
            if (d < best) { best = d; target = k; }
        }
    }
    int dragged = arr.press;
    arrInput(&arr, under, pressed, MDown(), mouse.x, mouse.y, target);
    if (arr.dragging && target < 0) target = dragged;    // the drag only just started
    bool dim = !s->inHand && !game.handOver;
    if (arr.dragging) {
        int rest[4], n = 0;                               // order with the dragged card moved to the target
        for (int k = 0; k < hc; k++) if (k != dragged) rest[n++] = arr.order[k];
        for (int k = 0, j = 0; k < hc; k++) {
            if (k == target) continue;
            DrawCard(s->hole[rest[j++]], HeroSlot(sp, k), dim, false);
        }
        if (target >= 0) DrawRectangleRoundedLinesEx(HeroSlot(sp, target), 0.14f, 6, 2, Fade(C_GOLD, 0.6f));
        Rectangle r = HeroSlot(sp, dragged);
        r.x = mouse.x - cardGrab.x;
        r.y = mouse.y - cardGrab.y - 10;
        DrawCard(s->hole[arr.order[dragged]], r, dim, true);
    } else {
        for (int k = 0; k < hc; k++) {
            Rectangle r = HeroSlot(sp, k);
            if (k == arr.sel) r.y -= 16;
            else if (k == under) r.y -= 5;
            DrawCard(s->hole[arr.order[k]], r, dim, k == arr.sel);
        }
    }
    // sort button and hint, left of the cards
    Rectangle first = HeroSlot(sp, 0);
    static const char *SORT_LBL[3] = { "Sort", "By suit", "By rank" };
    Rectangle sb = { first.x - 86, first.y + first.height / 2 - 16, 74, 32 };
    if (Button(sb, SORT_LBL[sortPref + 1], true, (Color){ 60, 66, 80, 255 })) {
        sortPref = sortPref == SORT_RANK ? SORT_SUIT : SORT_RANK;
        arrSort(&arr, s->hole, sortPref);
    }
    if (under >= 0 && !arr.dragging)
        TxtR(fReg, arr.sel >= 0 ? "click another card to swap" : "drag to rearrange", first.x - 12, sb.y + 40, 13, C_DIM);
}

static void DrawSeat(int i) {
    const Seat *s = &game.s[i];
    Vector2 sp = SPOT[SpotFor(i)];
    bool hero = i == game.human;
    bool top = sp.y < 300;
    int hc = holeCount(&game);
    bool showAll = game.handOver && gameMode == MODE_TRAIN;
    // hole cards
    if (hero || s->total > 0 || s->inHand || game.handOver) {
        if (hero) HeroCards(s, sp);
        else if (s->inHand || (showAll && s->stack + s->total > 0)) {
            float cw = 44, ch = 62, gap = hc == 2 ? 48 : 30;
            float x0 = sp.x - (gap * (hc - 1) + cw) / 2, y = top ? sp.y + 36 : sp.y - 36 - ch;
            bool face = s->shown || showAll;
            for (int k = 0; k < hc; k++) {
                Rectangle r = { x0 + k * gap, y, cw, ch };
                if (face) DrawCard(s->hole[k], r, !s->inHand, false);
                else DrawCardBack(r);
            }
        }
    }
    // seat box
    Rectangle box = { sp.x - 92, sp.y - 30, 184, 60 };
    bool toAct = game.toAct == i && !game.handOver;
    if (toAct) {
        float a = 0.55f + 0.45f * sinf(animT * 5);
        DrawRectangleRounded((Rectangle){ box.x - 5, box.y - 5, box.width + 10, box.height + 10 }, 0.4f, 8, Fade(C_GOLD, 0.35f * a));
    }
    bool winner = game.handOver && s->won > 0;
    DrawRectangleRounded((Rectangle){ box.x, box.y + 3, box.width, box.height }, 0.35f, 8, Fade(BLACK, 0.4f));
    DrawRectangleRounded(box, 0.35f, 8, s->inHand || game.handOver ? (Color){ 30, 36, 46, 240 } : (Color){ 30, 36, 46, 150 });
    DrawRectangleRoundedLinesEx(box, 0.35f, 8, 2, winner ? C_GOLD : toAct ? C_GOLD : Fade(WHITE, 0.25f));
    Color nameCol = s->inHand || game.handOver ? C_TEXT : C_DIM;
    Txt(fBold, s->name, box.x + 14, box.y + 8, 18, nameCol);
    if (!hero) {
        const char *lv = levelName(s->level);
        float w = TxtW(fBold, lv, 12) + 12;
        DrawRectangleRounded((Rectangle){ box.x + box.width - w - 10, box.y + 9, w, 18 }, 0.5f, 6, LV_COL[s->level]);
        Tip((Rectangle){ box.x + box.width - w - 10, box.y + 9, w, 18 }, "Bot levels");
        TxtC(fBold, lv, box.x + box.width - w / 2 - 10, box.y + 12, 12, WHITE);
    }
    Txt(fBold, Money(s->stack), box.x + 14, box.y + 32, 19, s->stack > 0 ? C_GOLD : C_DIM);
    const char *pn = posName(&game, i);
    if (strcmp(pn, "BTN") != 0 || game.n == 2) {
        const char *lbl = game.n == 2 ? (i == game.button ? "BTN/SB" : "BB") : pn;
        TxtR(fReg, lbl, box.x + box.width - 12, box.y + 35, 15, C_DIM);
        static const char *PT[6] = { "Under the Gun (UTG)", "Hijack (HJ)", "Cutoff (CO)", "Button (BTN)", "Small blind (SB)", "Big blind (BB)" };
        float lw = TxtW(fReg, lbl, 15);
        Tip((Rectangle){ box.x + box.width - 12 - lw, box.y + 33, lw, 20 }, game.n == 2 ? "Heads-up" : PT[posIndex(&game, i)]);
    }
    // dealer button
    if (i == game.button) {
        Vector2 d = { sp.x + (TCX - sp.x) * 0.16f + (sp.x < TCX ? 30 : sp.x > TCX ? -30 : 110), sp.y + (TCY - sp.y) * 0.22f };
        if (hero) d = (Vector2){ sp.x + 130, sp.y - 30 };
        DrawCircleV((Vector2){ d.x + 1, d.y + 2 }, 13, Fade(BLACK, 0.4f));
        DrawCircleV(d, 13, (Color){ 250, 250, 245, 255 });
        Tip((Rectangle){ d.x - 13, d.y - 13, 26, 26 }, "Button (BTN)");
        DrawCircleLinesV(d, 13, (Color){ 120, 120, 120, 255 });
        TxtC(fBold, "D", d.x, d.y - 8, 16, C_INK);
    }
    // last action tag
    char al[32];
    ActLabel(s, al, sizeof al);
    if (al[0] && !game.handOver) {
        float w = TxtW(fBold, al, 14) + 16;
        float ty = hero ? box.y + box.height + 4 : top ? box.y + box.height + 72 : box.y - 108;
        Color tc = s->lastAct == A_FOLD ? (Color){ 110, 110, 116, 255 } : s->lastAct == A_RAISE ? (Color){ 200, 90, 40, 255 } : (Color){ 60, 120, 180, 255 };
        DrawRectangleRounded((Rectangle){ sp.x - w / 2, ty, w, 22 }, 0.5f, 6, tc);
        TxtC(fBold, al, sp.x, ty + 3, 14, WHITE);
    }
    if (game.handOver && s->shown && s->inHand && s->value) {
        char hn[64];
        handName(s->value, hn, sizeof hn);
        float ty = hero ? box.y + box.height + 4 : top ? box.y + box.height + 72 : box.y - 108;
        float w = TxtW(fBold, hn, 14) + 16;
        DrawRectangleRounded((Rectangle){ sp.x - w / 2, ty, w, 22 }, 0.5f, 6, winner ? (Color){ 190, 140, 30, 255 } : (Color){ 50, 56, 66, 255 });
        TxtC(fBold, hn, sp.x, ty + 3, 14, WHITE);
    }
    // HUD stats in training mode
    if (gameMode == MODE_TRAIN && !hero && s->st.hands >= 5) {
        char hud[64];
        snprintf(hud, sizeof hud, "VPIP %d  PFR %d  (%dh)", 100 * s->st.vpip / s->st.hands, 100 * s->st.pfr / s->st.hands, s->st.hands);
        float hy = top ? box.y - 20 : box.y + box.height + 4;
        TxtC(fReg, hud, sp.x, hy, 13, C_DIM);
        float hw = TxtW(fReg, hud, 13);
        Tip((Rectangle){ sp.x - hw / 2, hy - 2, hw, 18 }, "HUD");
    }
    // chips in front
    if (s->bet > 0) {
        Vector2 b = hero ? (Vector2){ TCX - 20, 520 } : (Vector2){ sp.x + (TCX - sp.x) * 0.42f - 20, sp.y + (TCY - sp.y) * (top ? 0.5f : 0.4f) };
        DrawChips(b.x, b.y, s->bet);
    }
}

static void DrawTable(void) {
    Rectangle t = { TCX - 500, TCY - 280, 1000, 560 };
    DrawRectangleRounded((Rectangle){ t.x, t.y + 8, t.width, t.height }, 1.0f, 64, Fade(BLACK, 0.45f));
    DrawRectangleRounded(t, 1.0f, 64, C_RAIL);
    DrawRectangleRounded((Rectangle){ t.x + 18, t.y + 18, t.width - 36, t.height - 36 }, 1.0f, 64, (Color){ 84, 56, 40, 255 });
    DrawRectangleRounded((Rectangle){ t.x + 30, t.y + 30, t.width - 60, t.height - 60 }, 1.0f, 64, C_FELT2);
    DrawRectangleRounded((Rectangle){ t.x + 44, t.y + 44, t.width - 88, t.height - 88 }, 1.0f, 64, C_FELT);
    DrawRectangleRoundedLinesEx((Rectangle){ TCX - 340, TCY - 165, 680, 330 }, 1.0f, 64, 2, Fade(WHITE, 0.07f));
    const char *title = game.variant == V_OMAHA ? (game.limit == BL_POTLIMIT ? "POT-LIMIT OMAHA" : "NO-LIMIT OMAHA")
                                                : (game.limit == BL_POTLIMIT ? "POT-LIMIT HOLD'EM" : "NO-LIMIT HOLD'EM");
    TxtC(fBold, title, TCX, TCY + 108, 20, Fade(WHITE, 0.13f));

    // board
    float cw = 74, ch = 104, gap = 82, x0 = TCX - (gap * 4 + cw) / 2, y = 318;
    for (int i = 0; i < 5; i++) {
        Rectangle r = { x0 + i * gap, y, cw, ch };
        if (i < game.nboard) DrawCard(game.board[i], r, false, false);
        else DrawRectangleRoundedLinesEx(r, 0.14f, 6, 1.5f, Fade(WHITE, 0.12f));
    }
    // pot
    int pot = gamePot(&game), bets = 0;
    for (int i = 0; i < game.n; i++) bets += game.s[i].bet;
    char pt[80];
    if (game.handOver) {
        int tot = 0;
        for (int i = 0; i < game.npots; i++) tot += game.potAmt[i];
        if (game.npots > 1) snprintf(pt, sizeof pt, "Pot %s  (main + %d side)", Money(tot), game.npots - 1);
        else snprintf(pt, sizeof pt, "Pot %s", Money(tot));
    } else if (bets > 0 && pot - bets > 0) snprintf(pt, sizeof pt, "Pot %s   (total %s)", Money(pot - bets), Money(pot));
    else snprintf(pt, sizeof pt, "Pot %s", Money(pot));
    float w = TxtW(fBold, pt, 20) + 28;
    DrawRectangleRounded((Rectangle){ TCX - w / 2, y - 42, w, 32 }, 0.5f, 8, Fade(BLACK, 0.35f));
    TxtC(fBold, pt, TCX, y - 36, 20, C_GOLD);

    for (int i = 0; i < game.n; i++) DrawSeat(i);

    if (game.handOver && game.winText[0]) {
        float ww = TxtW(fBold, game.winText, 21) + 40;
        Rectangle b = { TCX - ww / 2, 438, ww, 40 };
        DrawRectangleRounded((Rectangle){ b.x, b.y + 3, b.width, b.height }, 0.5f, 8, Fade(BLACK, 0.4f));
        DrawRectangleRounded(b, 0.5f, 8, (Color){ 20, 26, 34, 235 });
        DrawRectangleRoundedLinesEx(b, 0.5f, 8, 2, C_GOLD);
        TxtC(fBold, game.winText, TCX, b.y + 10, 21, WHITE);
    }
}

// ---------------------------------------------------------------- betting controls

static int Unit(void) { return game.bbAmt >= 2 ? game.bbAmt / 2 : 1; }

static int Snap(int v, const Legal *l) {
    int u = Unit();
    if (v >= l->maxTo) return l->maxTo;
    v = (v + u / 2) / u * u;
    if (v < l->minTo) v = l->minTo;
    if (v > l->maxTo) v = l->maxTo;
    return v;
}

static void DrawControls(void) {
    Rectangle bar = { 20, 784, 1140, 104 };
    DrawRectangleRounded(bar, 0.2f, 8, Fade(C_PANEL, 0.92f));
    DrawRectangleRoundedLinesEx(bar, 0.2f, 8, 1.5f, Fade(WHITE, 0.12f));
    if (!gameStarted) return;
    const Seat *h = &game.s[game.human];
    if (game.handOver) {
        bool busted = h->stack <= 0;
        if (busted) {
            Txt(fBold, "You're out of chips.", bar.x + 30, bar.y + 22, 24, C_TEXT);
            Txt(fReg, "Rebuy to keep playing, or open the menu to change the stakes.", bar.x + 30, bar.y + 58, 16, C_DIM);
            if (Button((Rectangle){ bar.x + bar.width - 400, bar.y + 26, 180, 52 }, "Rebuy", true, C_GREEN) || KeyHit(KEY_ENTER)) Rebuy();
            if (Button((Rectangle){ bar.x + bar.width - 205, bar.y + 26, 180, 52 }, "Menu", true, C_BLUE)) showMenu = true;
            return;
        }
        int net = h->won - h->total;
        char m[96];
        if (h->total == 0 && h->won == 0) snprintf(m, sizeof m, "You folded before the flop.");
        else if (net > 0) snprintf(m, sizeof m, "You won %s this hand.", Money(net));
        else if (net < 0) snprintf(m, sizeof m, "You lost %s this hand.", Money(-net));
        else snprintf(m, sizeof m, "You broke even this hand.");
        Txt(fBold, m, bar.x + 30, bar.y + 24, 22, net > 0 ? C_GOLD : C_TEXT);
        const char *sub = gameMode == MODE_TRAIN ? "Review your decisions in the coach panel, then deal the next hand."
                        : autoDeal ? "Next hand starts automatically..." : "";
        Txt(fReg, sub, bar.x + 30, bar.y + 60, 16, C_DIM);
        if (Button((Rectangle){ bar.x + bar.width - 230, bar.y + 26, 205, 52 }, "Next Hand", true, C_GREEN) || KeyHit(KEY_SPACE) || KeyHit(KEY_ENTER)) NextHand();
        return;
    }
    if (game.toAct != game.human) {
        char m[96];
        if (game.toAct >= 0) snprintf(m, sizeof m, "%s is thinking...", game.s[game.toAct].name);
        else snprintf(m, sizeof m, "Dealing...");
        Txt(fBold, m, bar.x + 30, bar.y + 24, 22, C_TEXT);
        const char *sub = !h->inHand ? "You folded - watch how the hand plays out." : h->allin ? "You're all-in."
                        : game.toAct < 0 ? "No more betting this round." : "Waiting for your turn.";
        Txt(fReg, sub, bar.x + 30, bar.y + 60, 16, C_DIM);
        return;
    }
    Legal l;
    gameLegal(&game, &l);
    int key = DecisionKey();
    bool coachOn = gameMode == MODE_TRAIN && hintsBefore && advKey == key;
    if (raiseKey != key) {
        raiseKey = key;
        raiseAmt = coachOn && adv.aggAmount > 0 ? Snap(adv.aggAmount, &l) : l.minTo;
        if (gameMode != MODE_TRAIN || !hintsBefore) {
            int pot = gamePot(&game);
            raiseAmt = Snap(game.curBet == 0 ? pot * 2 / 3 : game.street == ST_PREFLOP && game.nRaises == 0 ? (int)(2.5f * game.bbAmt) : 3 * game.curBet, &l);
        }
    }
    if (raiseAmt < l.minTo) raiseAmt = l.minTo;
    if (raiseAmt > l.maxTo) raiseAmt = l.maxTo;
    int rec = coachOn ? adv.act : -1;
    float bx = bar.x + 20, by = bar.y + 16, bh = 72;
    Rectangle rf = { bx, by, 150, bh }, rc = { bx + 162, by, 200, bh }, rr = { bx + 374, by, 230, bh };
    char lbl[64];
    // highlight the coach's pick
    Rectangle hl = rec == A_FOLD ? rf : (rec == A_CHECK || rec == A_CALL) ? rc : rr;
    if (rec >= 0) {
        float a = 0.5f + 0.5f * sinf(animT * 4);
        DrawRectangleRoundedLinesEx((Rectangle){ hl.x - 5, hl.y - 5, hl.width + 10, hl.height + 10 }, 0.35f, 8, 3, Fade((Color){ 90, 230, 140, 255 }, 0.5f + 0.5f * a));
    }
    if (!l.canCheck) {
        if (Button(rf, "Fold", true, (Color){ 150, 50, 50, 255 }) || KeyHit(KEY_F)) { HumanAct((Action){ A_FOLD, 0 }); return; }
    } else Button(rf, "Fold", false, (Color){ 150, 50, 50, 255 });
    if (l.canCheck) snprintf(lbl, sizeof lbl, "Check");
    else snprintf(lbl, sizeof lbl, "Call %s%s", Money(l.toCall), l.toCall >= h->stack ? " (all-in)" : "");
    if (Button(rc, lbl, true, (Color){ 46, 110, 190, 255 }) || KeyHit(KEY_C) || KeyHit(KEY_K)) {
        HumanAct((Action){ l.canCheck ? A_CHECK : A_CALL, 0 });
        return;
    }
    bool allin = raiseAmt >= h->stack + h->bet;
    if (allin) snprintf(lbl, sizeof lbl, "All-in %s", Money(raiseAmt));
    else snprintf(lbl, sizeof lbl, "%s %s", game.curBet == 0 ? "Bet" : "Raise to", Money(raiseAmt));
    if (Button(rr, lbl, l.canRaise, (Color){ 190, 100, 40, 255 }) || (l.canRaise && (KeyHit(KEY_R) || KeyHit(KEY_ENTER)))) {
        HumanAct((Action){ A_RAISE, raiseAmt });
        return;
    }
    if (!l.canRaise) return;
    // presets
    int pot = gamePot(&game);
    float px = bx + 622, pw = 88, pg = 6;
    struct { const char *n; int to; } pre[5];
    if (game.street == ST_PREFLOP && game.nRaises == 0) {
        int limp = 0;
        for (int q = 0; q < game.n; q++) limp += game.s[q].inHand && game.s[q].pf == PF_LIMP;
        pre[0].n = "2.5 BB"; pre[0].to = (int)(2.5f * game.bbAmt) + limp * game.bbAmt;
        pre[1].n = "3 BB"; pre[1].to = 3 * game.bbAmt + limp * game.bbAmt;
        pre[2].n = "4 BB"; pre[2].to = 4 * game.bbAmt + limp * game.bbAmt;
    } else if (game.curBet == 0) {
        pre[0].n = "1/3 pot"; pre[0].to = pot / 3;
        pre[1].n = "1/2 pot"; pre[1].to = pot / 2;
        pre[2].n = "2/3 pot"; pre[2].to = pot * 2 / 3;
    } else {
        pre[0].n = "2.5x"; pre[0].to = (int)(2.5f * game.curBet);
        pre[1].n = "3x"; pre[1].to = 3 * game.curBet;
        pre[2].n = "4x"; pre[2].to = 4 * game.curBet;
    }
    pre[3].n = "Pot"; pre[3].to = game.curBet + pot + (game.curBet - h->bet);
    pre[4].n = l.maxTo < h->stack + h->bet ? "Max" : "All-in"; pre[4].to = l.maxTo;
    for (int i = 0; i < 5; i++)
        if (Button((Rectangle){ px + i * (pw + pg), by, pw, 32 }, pre[i].n, true, (Color){ 70, 78, 92, 255 })) raiseAmt = Snap(pre[i].to, &l);
    // slider
    Rectangle sl = { px + 6, by + 52, 5 * (pw + pg) - pg - 12, 8 };
    DrawRectangleRounded(sl, 1, 6, (Color){ 60, 66, 78, 255 });
    float f = l.maxTo > l.minTo ? (float)(raiseAmt - l.minTo) / (l.maxTo - l.minTo) : 1;
    DrawRectangleRounded((Rectangle){ sl.x, sl.y, sl.width * f, sl.height }, 1, 6, (Color){ 190, 100, 40, 255 });
    if (coachOn && adv.aggAmount > 0 && l.maxTo > l.minTo) {
        float rfx = (float)(Snap(adv.aggAmount, &l) - l.minTo) / (l.maxTo - l.minTo);
        DrawRectangle((int)(sl.x + sl.width * rfx) - 1, (int)sl.y - 7, 3, 22, (Color){ 90, 230, 140, 255 });
    }
    Vector2 knob = { sl.x + sl.width * f, sl.y + sl.height / 2 };
    Rectangle hit = { sl.x - 10, sl.y - 14, sl.width + 20, sl.height + 28 };
    if (Hover(hit) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) dragSlider = true;
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) dragSlider = false;
    if (dragSlider) {
        float t = (mouse.x - sl.x) / sl.width;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        raiseAmt = Snap(l.minTo + (int)(t * (l.maxTo - l.minTo)), &l);
    }
    float wheel = GetMouseWheelMove();
    if (gLayer == gActive && wheel != 0 && mouse.y > 780 && mouse.x < PX) raiseAmt = Snap(raiseAmt + (wheel > 0 ? 1 : -1) * game.bbAmt, &l);
    if (KeyHit(KEY_UP) || KeyHit(KEY_RIGHT)) raiseAmt = Snap(raiseAmt + game.bbAmt, &l);
    if (KeyHit(KEY_DOWN) || KeyHit(KEY_LEFT)) raiseAmt = Snap(raiseAmt - game.bbAmt, &l);
    DrawCircleV(knob, 12, (Color){ 250, 240, 225, 255 });
    DrawCircleLinesV(knob, 12, (Color){ 190, 100, 40, 255 });
    char mm[64];
    snprintf(mm, sizeof mm, "min %s", Money(l.minTo));
    Txt(fReg, mm, sl.x, sl.y + 16, 13, C_DIM);
    snprintf(mm, sizeof mm, "%s %s", l.maxTo < h->stack + h->bet ? "pot" : "max", Money(l.maxTo));
    TxtR(fReg, mm, sl.x + sl.width, sl.y + 16, 13, C_DIM);
}

// ---------------------------------------------------------------- side panel

static void Section(const char *t, float x, float y) {
    Txt(fBold, t, x, y, 14, (Color){ 140, 170, 210, 255 });
}

static void Stat(const char *label, const char *val, float x, float y, float w, Color vc, const char *term) {
    DrawRectangleRounded((Rectangle){ x, y, w, 50 }, 0.25f, 6, C_PANEL2);
    Tip((Rectangle){ x, y, w, 50 }, term);
    TxtC(fReg, label, x + w / 2, y + 6, 13, C_DIM);
    TxtC(fBold, val, x + w / 2, y + 24, 19, vc);
}

static float DrawGrades(const Advice *a, float x, float y, float maxW) {
    static const char *AN[4] = { "Fold", "Check", "Call", "Raise" };
    float cx = x, y0 = y;
    for (int k = 0; k < 4; k++) {
        if (a->q[k] < 0) continue;
        const char *nm = k == A_RAISE && a->openBet ? "Bet" : AN[k];
        char t[48];
        snprintf(t, sizeof t, "%s: %s", nm, qualityName(a->q[k]));
        float w = TxtW(fBold, t, 13) + 16;
        if (cx > x && cx + w > x + maxW) { cx = x; y += 30; }
        Rectangle r = { cx, y, w, 24 };
        Tip(r, "Grades");
        DrawRectangleRounded(r, 0.5f, 6, Fade(Q_COL[a->q[k]], 0.85f));
        TxtC(fBold, t, r.x + w / 2, r.y + 5, 13, (Color){ 20, 20, 24, 255 });
        cx += w + 6;
    }
    return y - y0 + 32;
}

// bulleted, scrollable list of lines (the coach's reasons, or the quiz checklist)
static float DrawLines(const char *const *lines, int n, float x, float y, float w, float h, bool numbered) {
    Rectangle clip = { x - 4, y, w + 8, h };
    float total = 0;
    for (int i = 0; i < n; i++) total += Wrap(fReg, lines[i], x + 12, 0, w - 14, 15, 20, C_TEXT, false) + 8;
    float maxS = total > h ? total - h : 0;
    if (gLayer == gActive && CheckCollisionPointRec(mouse, clip)) whyScroll -= GetMouseWheelMove() * 40;
    if (whyScroll < 0) whyScroll = 0;
    if (whyScroll > maxS) whyScroll = maxS;
    ClipBegin(clip);
    float yy = y - whyScroll;
    for (int i = 0; i < n; i++) {
        if (numbered) TxtC(fBold, TextFormat("%d", i + 1), x + 3, yy + 1, 14, C_GOLD);
        else DrawCircleV((Vector2){ x + 4, yy + 9 }, 2.5f, (Color){ 140, 170, 210, 255 });
        yy += Wrap(fReg, lines[i], x + 12, yy, w - 14, 15, 20, C_TEXT, true) + 8;
    }
    EndScissorMode();
    if (maxS > 0) TxtR(fReg, whyScroll < maxS ? "scroll for more" : "", x + w, y + h + 2, 12, C_DIM);
    return total;
}

// glossary terms the advice mentions, as chips: hover for the definition, click to open the glossary
static float DrawTermChips(const Advice *a, float x, float y, float w) {
    static int terms[24], nterms = 0;
    static unsigned sum = 0;
    unsigned h = 0;
    for (const char *p = a->head; *p; p++) h = h * 31 + (unsigned char)*p;
    for (int i = 0; i < a->nwhy; i++) for (const char *p = a->why[i]; *p; p++) h = h * 31 + (unsigned char)*p;
    if (h != sum) {
        static char all[sizeof a->why + 600];
        int len = snprintf(all, sizeof all, "%s. %s. %s. %s.", a->spot, a->hand, a->draws, a->head);
        for (int i = 0; i < a->nwhy && len < (int)sizeof all - 2; i++) len += snprintf(all + len, sizeof all - len, " %s", a->why[i]);
        nterms = glossFind(all, terms, 24);
        sum = h;
    }
    if (!nterms) return 0;
    Txt(fBold, "TERMS", x, y + 5, 12, (Color){ 140, 170, 210, 255 });
    float cx = x + 50, cy = y;
    for (int i = 0; i < nterms; i++) {
        const char *t = glossGet(terms[i])->term, *par = strstr(t, " (");
        char lbl[48];
        int pl = par ? (int)(strlen(par) - 3) : 0;
        if (par && pl <= 5) snprintf(lbl, sizeof lbl, "%.*s", pl, par + 2);        // "Stack-to-pot ratio (SPR)" -> "SPR"
        else snprintf(lbl, sizeof lbl, "%.*s", par ? (int)(par - t) : (int)strlen(t), t);
        float bw = TxtW(fReg, lbl, 13) + 16;
        if (cx + bw > x + w) { cx = x + 50; cy += 26; if (cy > y + 26) break; }
        Rectangle r = { cx, cy, bw, 22 };
        bool hov = Hover(r);
        DrawRectangleRounded(r, 0.5f, 6, hov ? (Color){ 70, 90, 120, 255 } : C_PANEL2);
        DrawRectangleRoundedLinesEx(r, 0.5f, 6, 1, Fade((Color){ 140, 170, 210, 255 }, 0.5f));
        TxtC(fReg, lbl, r.x + bw / 2, r.y + 4, 13, C_TEXT);
        TipIdx(r, terms[i]);
        if (hov && MPressed()) OpenGlossary(terms[i]);
        cx += bw + 5;
    }
    return cy - y + 26;
}

static void DrawCoachTab(float x, float y, float w) {
    bool myTurn = game.toAct == game.human && !game.handOver && advKey == DecisionKey();
    const Advice *a = myTurn ? &adv : lastValid ? &lastAdv : NULL;
    float y0 = y;
    if (!a) {
        Section("COACH", x, y);
        Wrap(fReg, game.handOver ? "No decisions for you this hand." :
             "The coach analyses every decision you make: position, hand strength, pot odds, your equity against "
             "the opponents' likely ranges, and what a solid player does here. Wait for your turn.", x, y + 26, w, 16, 22, C_TEXT, true);
    } else {
        Section(myTurn ? "YOUR DECISION" : "LAST DECISION", x, y);
        y += 22;
        y += Wrap(fBold, a->spot, x, y, w, 17, 22, WHITE, true) + 4;
        // numbers
        char b1[32], b2[32], b3[32];
        float sw = (w - 12) / 3;
        if (a->hasEquity) snprintf(b1, sizeof b1, "%.0f%%", a->equity * 100); else snprintf(b1, sizeof b1, "-");
        if (a->need > 0) snprintf(b2, sizeof b2, "%.0f%%", a->need * 100); else snprintf(b2, sizeof b2, "free");
        if (a->preflop) snprintf(b3, sizeof b3, "top %.0f%%", a->top * 100 < 1 ? 1 : a->top * 100);
        else snprintf(b3, sizeof b3, "%.1f", a->spr);
        Color ec = a->hasEquity && a->need > 0 ? (a->equity >= a->need ? Q_COL[0] : Q_COL[4]) : WHITE;
        Stat("Your equity", b1, x, y, sw, ec, "Equity");
        Stat("Equity needed", b2, x + sw + 6, y, sw, WHITE, "Pot odds");
        Stat(a->preflop ? "Hand strength" : "Stack / pot", b3, x + 2 * (sw + 6), y, sw, WHITE,
             a->preflop ? "Starting-hand strength (top X%)" : "Stack-to-pot ratio (SPR)");
        y += 60;
        bool reveal = !myTurn || hintsBefore;
        Rectangle rb = { x, y, w, 64 };
        DrawRectangleRounded(rb, 0.2f, 8, reveal ? (Color){ 30, 70, 52, 255 } : C_PANEL2);
        if (reveal) {
            Tip(rb, "Coach recommends");
            Txt(fReg, "Coach recommends", x + 12, y + 8, 13, (Color){ 160, 220, 180, 255 });
            Txt(fBold, a->head, x + 12, y + 27, 22, WHITE);
            if (a->alt >= 0 && a->altFreq > 0) {
                char mx[48];
                snprintf(mx, sizeof mx, "mix ~%.0f%%", a->altFreq * 100);
                TxtR(fReg, mx, x + w - 12, y + 10, 13, (Color){ 160, 220, 180, 255 });
                float mw = TxtW(fReg, mx, 13);
                Tip((Rectangle){ x + w - 12 - mw, y + 6, mw, 20 }, "Mixed strategy");
            }
        } else {
            Tip(rb, "Hints / Quiz mode");
            Txt(fBold, "Quiz mode - make your play", x + 12, y + 12, 18, WHITE);
            Txt(fReg, "Work through the questions below, then act. You're graded right after.", x + 12, y + 38, 14, C_DIM);
        }
        y += 72;
        if (!myTurn && lastQ >= 0) {
            char t[140];
            snprintf(t, sizeof t, "You: %s  -  %s", lastDid, qualityName(lastQ));
            DrawRectangleRounded((Rectangle){ x, y, w, 30 }, 0.4f, 6, Fade(Q_COL[lastQ], 0.25f));
            DrawRectangleRoundedLinesEx((Rectangle){ x, y, w, 30 }, 0.4f, 6, 1.5f, Q_COL[lastQ]);
            Txt(fBold, t, x + 10, y + 7, 15, WHITE);
            y += 36;
            if (lastNote[0]) y += Wrap(fReg, lastNote, x, y, w, 14, 18, Q_COL[Q_INACC], true) + 4;
        }
        if (reveal) y += DrawGrades(a, x, y, w);
        float whyH = y0 + 600 - (game.handOver && nreview ? 150 : 0) - y;
        if (reveal) {
            whyH -= 58;                      // room for the term chips
            if (whyH < 60) whyH = 60;
            const char *lines[NWHY];
            for (int i = 0; i < a->nwhy; i++) lines[i] = a->why[i];
            DrawLines(lines, a->nwhy, x, y, w, whyH, false);
            DrawTermChips(a, x, y + whyH + 18, w);
            whyH += 58;
        } else {
            if (whyH < 60) whyH = 60;
            Section("THINK IT THROUGH", x, y);
            const char *lines[NTHINK];
            for (int i = 0; i < nthink; i++) lines[i] = think[i];
            DrawLines(lines, nthink, x, y + 22, w, whyH - 22, true);
        }
        y += whyH + 14;
    }
    // hand review + session score at the bottom
    float by = y0 + 620;
    if (game.handOver && nreview > 0) {
        float ry = by - 150;
        Section("THIS HAND", x, ry);
        ry += 20;
        static const char *SN[4] = { "Pre", "Flop", "Turn", "River" };
        for (int i = nreview > 6 ? nreview - 6 : 0; i < nreview; i++) {    // the last six decisions
            const Review *r = &review[i];
            DrawCircleV((Vector2){ x + 6, ry + 9 }, 5, Q_COL[r->q]);
            char t[200];
            snprintf(t, sizeof t, "%s: %s", SN[r->street], r->did);
            Txt(fBold, t, x + 18, ry, 14, WHITE);
            if (r->q >= Q_INACC) TxtR(fReg, TextFormat("best: %s", r->best), x + w, ry, 13, C_DIM);
            else TxtR(fReg, qualityName(r->q), x + w, ry, 13, Q_COL[r->q]);
            ry += 21;
        }
    }
    DrawLine((int)x, (int)by, (int)(x + w), (int)by, Fade(WHITE, 0.12f));
    Section("SESSION", x, by + 10);
    char t[160];
    int good = qCount[0] + qCount[1];
    snprintf(t, sizeof t, "%d decisions  -  %d%% good  -  %d mistakes, %d blunders", decisions, decisions ? 100 * good / decisions : 0, qCount[Q_MISTAKE], qCount[Q_BLUNDER]);
    Txt(fReg, t, x, by + 30, 14, C_TEXT);
    int best = -1, second = -1;
    for (int i = 0; i < LK_N; i++) {
        if (!leaks[i]) continue;
        if (best < 0 || leaks[i] > leaks[best]) { second = best; best = i; }
        else if (second < 0 || leaks[i] > leaks[second]) second = i;
    }
    if (best >= 0) {
        snprintf(t, sizeof t, "Biggest leak: %s (%d)", LEAK_NAME[best], leaks[best]);
        Tip((Rectangle){ x, by + 48, TxtW(fReg, t, 14), 18 }, "Leak");
        Txt(fReg, t, x, by + 50, 14, Q_COL[Q_MISTAKE]);
        if (second >= 0) {
            snprintf(t, sizeof t, "Next: %s (%d)", LEAK_NAME[second], leaks[second]);
            Txt(fReg, t, x, by + 68, 14, Q_COL[Q_INACC]);
        }
    } else Txt(fReg, "No leaks found yet.", x, by + 50, 14, C_DIM);
}

static void DrawLogTab(float x, float y, float w, float h) {
    Rectangle clip = { x - 4, y, w + 8, h };
    float total = 0;
    for (int i = 0; i < game.nlog; i++) total += Wrap(fReg, game.log[i], x, 0, w, 14, 18, C_TEXT, false) + 2;
    float maxS = total > h ? total - h : 0;
    if (gLayer == gActive && CheckCollisionPointRec(mouse, clip)) logScroll += GetMouseWheelMove() * 40;
    if (logScroll < 0) logScroll = 0;
    if (logScroll > maxS) logScroll = maxS;
    ClipBegin(clip);
    float yy = y + h - total + logScroll;
    if (total < h) yy = y;
    for (int i = 0; i < game.nlog; i++) {
        const char *s = game.log[i];
        bool head = strncmp(s, "---", 3) == 0;
        bool street = strncmp(s, "Flop", 4) == 0 || strncmp(s, "Turn", 4) == 0 || strncmp(s, "River", 5) == 0;
        Color c = head ? C_GOLD : street ? (Color){ 140, 190, 240, 255 } : strncmp(s, "You", 3) == 0 ? (Color){ 150, 230, 170, 255 } : C_TEXT;
        yy += Wrap(head || street ? fBold : fReg, s, x, yy, w, 14, 18, c, true) + 2;
    }
    EndScissorMode();
}

static void DrawPanel(void) {
    Rectangle r = { PX, 12, PW, VH - 24 };
    DrawRectangleRounded(r, 0.03f, 8, C_PANEL);
    DrawRectangleRoundedLinesEx(r, 0.03f, 8, 1.5f, Fade(WHITE, 0.1f));
    float x = PX + 16, w = PW - 32;
    if (!gameStarted) return;
    bool train = gameMode == MODE_TRAIN;
    // header: stack & session result
    const Seat *h = &game.s[game.human];
    int net = HeroChips() - Invested();
    char t[96];
    Txt(fBold, Money(HeroChips()), x, r.y + 14, 24, C_GOLD);
    snprintf(t, sizeof t, "%s%s  (%+.1f BB)", net >= 0 ? "+" : "", Money(net), (float)net / game.bbAmt);
    TxtR(fBold, t, x + w, r.y + 18, 18, net > 0 ? Q_COL[0] : net < 0 ? Q_COL[4] : C_TEXT);
    snprintf(t, sizeof t, "Hand #%d  -  %d played  -  %d buy-in%s", game.handNo, handsPlayed, h->buyins, h->buyins == 1 ? "" : "s");
    Txt(fReg, t, x, r.y + 46, 13, C_DIM);
    float ty = r.y + 70;
    if (train) {
        if (Button((Rectangle){ x, ty, w / 2 - 4, 32 }, "Coach", true, panelTab == 0 ? C_BLUE : (Color){ 60, 66, 80, 255 })) panelTab = 0;
        if (Button((Rectangle){ x + w / 2 + 4, ty, w / 2 - 4, 32 }, "Hand log", true, panelTab == 1 ? C_BLUE : (Color){ 60, 66, 80, 255 })) panelTab = 1;
        ty += 44;
    } else {
        Section("HAND LOG", x, ty + 4);
        ty += 28;
        panelTab = 1;
    }
    if (train && panelTab == 0) DrawCoachTab(x, ty, w);
    else DrawLogTab(x, ty, w, r.y + r.height - ty - (train ? 70 : 80));
    // footer buttons
    float fy = r.y + r.height - 50;
    if (train) {
        float bw = (w - 12) / 3;
        if (Button((Rectangle){ x, fy, bw, 36 }, "Charts", game.variant == V_HOLDEM, (Color){ 80, 70, 140, 255 }) || (game.variant == V_HOLDEM && KeyHit(KEY_G))) {
            showChart = true;
            int pos = posIndex(&game, game.human);
            chartTab = game.n == 2 ? 5 : pos <= POS_SB ? pos : 10;
        }
        if (Button((Rectangle){ x + bw + 6, fy, bw, 36 }, "Lessons", true, (Color){ 80, 70, 140, 255 }) || KeyHit(KEY_L)) { showLessons = true; helpScroll = 0; }
        if (Button((Rectangle){ x + 2 * (bw + 6), fy, bw, 36 }, hintsBefore ? "Hints: on" : "Quiz mode", true, hintsBefore ? (Color){ 40, 120, 80, 255 } : (Color){ 150, 100, 40, 255 }))
            hintsBefore = !hintsBefore;
    } else {
        snprintf(t, sizeof t, "Biggest pot won: %s", Money(biggestPot));
        Txt(fReg, t, x, fy - 6, 14, C_DIM);
        Txt(fReg, "Want feedback on your play? Start a Training game from the menu.", x, fy + 14, 13, C_DIM);
    }
}

// ---------------------------------------------------------------- overlays

static void Panel(Rectangle r) {
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.55f));
    DrawRectangleRounded((Rectangle){ r.x + 4, r.y + 6, r.width, r.height }, 0.04f, 8, Fade(BLACK, 0.4f));
    DrawRectangleRounded(r, 0.04f, 8, C_PAPER);
    DrawRectangleRoundedLinesEx(r, 0.04f, 8, 2, (Color){ 200, 180, 140, 255 });
}

static bool Choice(Rectangle r, const char *label, bool on) {
    return Button(r, label, true, on ? (Color){ 46, 110, 190, 255 } : (Color){ 150, 150, 160, 255 });
}

static void DrawMenu(void) {
    Rectangle r = { VW / 2.0f - 400, 30, 800, 840 };
    Panel(r);
    float cx = r.x + r.width / 2, lx = r.x + 50, cx0 = r.x + 230;
    TxtC(fBold, "Poker Trainer", cx, r.y + 26, 44, C_INK);
    TxtC(fReg, "Texas Hold'em & Omaha  -  play the bots or train with a coach", cx, r.y + 80, 17, C_MUTED);
    int deco[5] = { CARD(12, 3), CARD(11, 2), CARD(10, 1), CARD(9, 0), CARD(8, 3) };
    for (int i = 0; i < 5; i++) DrawCard(deco[i], (Rectangle){ cx - 2.5f * 66 + i * 66 + 4, r.y + 112, 58, 82 }, false, false);
    float y = r.y + 210, rowH = 47;

    Txt(fBold, "Game", lx, y + 9, 19, C_INK);
    if (Choice((Rectangle){ cx0, y, 200, 40 }, "Texas Hold'em", setVariant == V_HOLDEM)) { setVariant = V_HOLDEM; setLimit = BL_NOLIMIT; }
    if (Choice((Rectangle){ cx0 + 210, y, 200, 40 }, "Omaha", setVariant == V_OMAHA)) { setVariant = V_OMAHA; setLimit = BL_POTLIMIT; }
    y += rowH;
    Txt(fBold, "Betting", lx, y + 9, 19, C_INK);
    if (Choice((Rectangle){ cx0, y, 200, 40 }, "No-Limit", setLimit == BL_NOLIMIT)) setLimit = BL_NOLIMIT;
    if (Choice((Rectangle){ cx0 + 210, y, 200, 40 }, "Pot-Limit", setLimit == BL_POTLIMIT)) setLimit = BL_POTLIMIT;
    y += rowH;
    Txt(fBold, "Mode", lx, y + 9, 19, C_INK);
    if (Choice((Rectangle){ cx0, y, 200, 40 }, "Play", setMode == MODE_PLAY)) setMode = MODE_PLAY;
    if (Choice((Rectangle){ cx0 + 210, y, 200, 40 }, "Training (coach)", setMode == MODE_TRAIN)) setMode = MODE_TRAIN;
    y += rowH;
    Txt(fBold, "Opponents", lx, y + 9, 19, C_INK);
    for (int k = 1; k <= 5; k++)
        if (Choice((Rectangle){ cx0 + (k - 1) * 82, y, 74, 40 }, TextFormat("%d", k), setOpp == k)) setOpp = k;
    TxtR(fReg, setOpp == 1 ? "heads-up" : setOpp == 5 ? "6-max" : "", r.x + r.width - 40, y + 12, 14, C_MUTED);
    y += rowH;
    Txt(fBold, "Bot skill", lx, y + 9, 19, C_INK);
    for (int k = 1; k <= setOpp; k++) {
        Rectangle b = { cx0 + (k - 1) * 106, y, 98, 40 };
        if (Button(b, levelName(setLv[k]), true, LV_COL[setLv[k]])) setLv[k] = setLv[k] % 3 + 1;
    }
    y += 44;
    Txt(fReg, "click a bot to cycle - set all:", cx0, y + 6, 14, C_MUTED);
    for (int l = 1; l <= 3; l++)
        if (Button((Rectangle){ cx0 + 200 + (l - 1) * 92, y, 84, 28 }, levelName(l), true, LV_COL[l]))
            for (int k = 1; k < MAXP; k++) setLv[k] = l;
    y += 40;
    Txt(fBold, "Blinds", lx, y + 9, 19, C_INK);
    if (Button((Rectangle){ cx0, y, 44, 40 }, "<", blindIdx > 0, (Color){ 100, 110, 130, 255 })) blindIdx--;
    char t[96];
    snprintf(t, sizeof t, "%s / %s", Money(BLINDS[blindIdx][0]), Money(BLINDS[blindIdx][1]));
    TxtC(fBold, t, cx0 + 140, y + 9, 21, C_INK);
    if (Button((Rectangle){ cx0 + 236, y, 44, 40 }, ">", blindIdx < NBLINDS - 1, (Color){ 100, 110, 130, 255 })) blindIdx++;
    y += rowH;
    Txt(fBold, "Buy-in", lx, y + 9, 19, C_INK);
    if (Button((Rectangle){ cx0, y, 44, 40 }, "<", buyIdx > 0, (Color){ 100, 110, 130, 255 })) buyIdx--;
    snprintf(t, sizeof t, "%s  (%d BB)", Money(BUYINS[buyIdx] * BLINDS[blindIdx][1]), BUYINS[buyIdx]);
    TxtC(fBold, t, cx0 + 140, y + 9, 21, C_INK);
    if (Button((Rectangle){ cx0 + 236, y, 44, 40 }, ">", buyIdx < NBUYINS - 1, (Color){ 100, 110, 130, 255 })) buyIdx++;
    TxtR(fReg, BUYINS[buyIdx] < 40 ? "short stack: push/fold poker" : BUYINS[buyIdx] >= 100 ? "deep: the charts assume ~100 BB" : "medium stack", r.x + r.width - 40, y + 12, 14, C_MUTED);
    y += rowH;
    Txt(fBold, "Speed", lx, y + 9, 19, C_INK);
    static const char *sp[3] = { "Slow", "Normal", "Fast" };
    for (int s = 0; s < 3; s++)
        if (Choice((Rectangle){ cx0 + s * 136, y, 128, 40 }, sp[s], speedIdx == s)) speedIdx = s;
    y += rowH;
    Txt(fBold, "Options", lx, y + 9, 19, C_INK);
    if (Choice((Rectangle){ cx0, y, 200, 40 }, fourColor ? "Four-color deck: on" : "Four-color deck: off", fourColor)) fourColor = !fourColor;
    if (Choice((Rectangle){ cx0 + 210, y, 200, 40 }, autoDeal ? "Auto-deal: on" : "Auto-deal: off", autoDeal)) autoDeal = !autoDeal;
    y += 54;
    TxtC(fReg, "Easy: loose and passive, calls far too much.   Medium: tight, but ignores position and ranges.", cx, y, 14, C_MUTED);
    TxtC(fReg, "Hard: plays the coach's strategy, mixes in bluffs and adapts to how you play.", cx, y + 19, 14, C_MUTED);
    y += 48;
    if (Button((Rectangle){ cx - 360, y, 230, 52 }, gameStarted ? "New Game" : "Start Game", true, C_GREEN) || (KeyHit(KEY_ENTER) && !gameStarted)) StartNewGame();
    if (gameStarted) {
        if (Button((Rectangle){ cx - 115, y, 230, 52 }, "Resume", true, C_BLUE)) showMenu = false;
        TxtC(fReg, "Settings apply when you start a new game.", cx, y + 62, 14, C_MUTED);
    } else if (Button((Rectangle){ cx - 115, y, 230, 52 }, "How to play", true, C_BLUE)) { showHelp = true; helpScroll = 0; }
    if (Button((Rectangle){ cx + 130, y, 230, 52 }, "Glossary", true, (Color){ 80, 70, 140, 255 })) OpenGlossary(-1);
}

static float DrawPages(Rectangle r, const char *title, const char *body, float *scroll) {
    TxtC(fBold, title, r.x + r.width / 2, r.y + 24, 32, C_INK);
    Rectangle clip = { r.x + 40, r.y + 80, r.width - 80, r.height - 190 };
    float total = Wrap(fReg, body, clip.x, 0, clip.width, 17, 25, C_INK, false);
    float maxS = total > clip.height ? total - clip.height : 0;
    if (gLayer == gActive && CheckCollisionPointRec(mouse, clip)) *scroll -= GetMouseWheelMove() * 50;
    if (*scroll < 0) *scroll = 0;
    if (*scroll > maxS) *scroll = maxS;
    ClipBegin(clip);
    Wrap(fReg, body, clip.x, clip.y - *scroll, clip.width, 17, 25, C_INK, true);
    EndScissorMode();
    if (maxS > 0) TxtR(fReg, *scroll < maxS ? "scroll for more" : "", clip.x + clip.width, clip.y + clip.height + 4, 13, C_MUTED);
    return maxS;
}

static const char *HELP_TEXT =
    "TEXAS HOLD'EM\n"
    "Everyone gets 2 private cards. Five shared cards are dealt face up in the middle: 3 on the flop, 1 on the turn, 1 on the river. "
    "There is a betting round before the flop and after each new card. Your hand is the best 5 cards out of your 2 and the 5 on the board (you may use 0, 1 or 2 of your own).\n\n"
    "OMAHA\n"
    "Same structure, but you get 4 private cards and must use EXACTLY 2 of them with EXACTLY 3 from the board. "
    "A board with four hearts does not give you a flush unless you hold two hearts yourself. Omaha is normally played Pot-Limit: the biggest raise allowed is the size of the pot.\n\n"
    "BLINDS AND BETTING\n"
    "The two players left of the dealer button post the small and big blind. Preflop, the player left of the big blind acts first; after the flop the first player left of the button does. "
    "On your turn you can fold, check (if nobody has bet), call, or bet / raise. A raise must be at least as big as the previous bet or raise. "
    "If you can't cover a bet you can still call all-in; side pots are made automatically.\n\n"
    "HAND RANKINGS (best to worst)\n"
    "Straight flush - Four of a kind - Full house - Flush - Straight - Three of a kind - Two pair - One pair - High card. "
    "An ace can be high (T-J-Q-K-A) or low (A-2-3-4-5) in a straight. Equal hands split the pot.\n\n"
    "CONTROLS\n"
    "F = fold,  C or K = check / call,  R or Enter = bet / raise.  Up / Down arrows or the mouse wheel change the bet size, or drag the slider and use the size presets. "
    "Space or Enter deals the next hand.  Esc opens the menu,  H this help,  T the glossary,  L the lessons,  M mutes the sound.  In training mode G opens the preflop charts.\n"
    "Rearrange your own cards any time: drag a card to a new spot, or click one card and then another to swap them. "
    "The Sort button orders them by rank, then by suit (handy in Omaha), and your choice is kept for the next hands.\n\n"
    "TRAINING MODE\n"
    "A coach panel analyses each of your decisions: your position, hand strength, pot odds, your equity against what the opponents probably hold, and what a solid player does. "
    "With hints on, it shows the recommendation before you act and highlights the button. In quiz mode it stays hidden and grades your play afterwards (Best / Good / Inaccuracy / Mistake / Blunder). "
    "Your session score and biggest leaks are tracked at the bottom. After each hand all the bots' cards are shown so you can see what they had.\n\n"
    "LEARNING THE LANGUAGE\n"
    "Press T (or the Glossary button) for a searchable glossary of every poker term the game uses. During play, hover the mouse over a stat box, a seat's position, "
    "the VPIP / PFR numbers or a grade to see what it means. Under the coach's reasons, the TERMS row lists the jargon it used: hover a term for its definition, click it to open the glossary.\n\n"
    "LEARNING TO THINK LIKE A PLAYER\n"
    "In quiz mode the coach panel shows a 'think it through' checklist for every decision: position, the action so far, the board, what each opponent's betting says about their hand, "
    "the price, and your plan. It never gives the answer - work through it, act, then compare with the coach. Lessons 7-11 (L) cover thinking through a hand, ranges, "
    "reading real players, bankroll and tilt, and how to keep improving, including live-casino tips.";

typedef struct { const char *title, *body; } Lesson;
static const Lesson LESSONS[] = {
    { "1. Position",
      "Position is where you sit relative to the dealer button, and it decides who acts last after the flop. Acting last is a huge advantage: you see what everyone did before you decide, "
      "you can take free cards, control the pot size and bluff more effectively.\n\n"
      "That's why good players play TIGHT from early position and LOOSE from late position. At a 6-handed table a solid player opens about:\n"
      "UTG (first to act) ~18%  -  Hijack ~22%  -  Cutoff ~29%  -  Button ~45%  -  Small blind ~38%.\n\n"
      "The blinds are the worst seats: you're out of position for the whole hand. You defend the big blind wide because you already have money in and get a discount, "
      "but from the small blind it's usually best to 3-bet or fold rather than call.\n\n"
      "Open the Charts button to see the exact ranges the coach uses. Your current hand is outlined in gold." },
    { "2. Raise or fold, don't limp",
      "When nobody has entered the pot, come in with a RAISE (about 2.5 big blinds, 3 from the small blind), not a call. Raising wins the blinds immediately some of the time, "
      "builds a pot when you have the stronger range, and gives you the initiative: you can win the pot with a continuation bet on most flops.\n\n"
      "Facing a raise, you have three choices. 3-bet (re-raise to ~3x in position, ~4x out of position) with your strongest hands (QQ+, AK) plus a few bluffs that block "
      "big hands, like A5s. Call with hands that play well after the flop: pairs, suited broadways, suited connectors - mostly when you'll have position. Fold the rest.\n\n"
      "Against a 3-bet, continue with roughly the top 10% of your opening range. Against a 4-bet, only premium hands continue.\n\n"
      "Short stacked (about 12 big blinds or less) the game becomes push-or-fold: a raise commits you anyway, so move all-in with a range that widens the later your position. "
      "The coach's push ranges are SOLVED - computed as a Nash equilibrium, so nobody can exploit them. For example, at 10 BB: UTG shoves ~14% of hands, the button ~32%, the small blind ~59%.\n\n"
      "With up to ~20 BB, facing an open, calling is rarely right (you're too short to play after the flop): RE-STEAL all-in or fold. The coach works out whether the shove profits from how often they fold and your equity when they call." },
    { "3. Pot odds and equity",
      "EQUITY is your share of the pot if all the cards were dealt out: how often you win (ties split). POT ODDS tell you how much equity you need to call.\n\n"
      "Needed equity = amount to call / (pot after you call).\n"
      "Example: the pot is $100 and your opponent bets $50. You call $50 to win $200 total, so you need 50 / 200 = 25%.\n\n"
      "If your equity against their likely hands is higher than that, calling makes money in the long run - even though you'll often lose this particular pot.\n\n"
      "Counting outs: an OUT is a card that makes your hand. A flush draw has 9 outs, an open-ended straight draw 8, a gutshot 4. "
      "Rule of 4 and 2: on the flop, outs x 4 is roughly your chance of hitting by the river; on the turn, outs x 2 is your chance on the river.\n\n"
      "IMPLIED ODDS: when you hit a disguised draw you often win more money later. With deep stacks you can call a little lighter with draws - but not on the river, where there's nothing left to draw to.\n\n"
      "The coach estimates opponents' RANGES (all the hands they could have) from their position, their preflop action, how they've bet since, and their stats (VPIP / PFR, shown under each bot)." },
    { "4. Betting for value",
      "Bet when worse hands will call you (VALUE) or when better hands will fold (BLUFF). Hands in the middle - a weak pair, ace high - usually want to check: "
      "betting them gets called by better and folds out worse.\n\n"
      "Size your bets for the board. On DRY boards (like K-7-2 rainbow) a small bet of about 1/3 pot does the job. On WET boards (two of a suit, connected cards) "
      "bet bigger - 2/3 to 3/4 pot - so draws pay the wrong price to chase.\n\n"
      "Continuation bet: if you raised preflop and get checked to on the flop heads-up, bet small often. Your range has more strong hands than the caller's, especially on high-card boards.\n\n"
      "With a strong hand facing a bet, RAISE for value instead of just calling - especially on wet boards, where waiting gives free cards to draws.\n\n"
      "Multiway pots need stronger hands. Against two or three opponents someone usually connects, so bluff less and value-bet only solid hands.\n\n"
      "Stack-to-pot ratio (SPR) = effective stack / pot. With a low SPR (under ~2) top pair is happy to get all-in; with a high SPR you want a stronger hand to stack off." },
    { "5. Bluffing and balance",
      "If you only bet with strong hands, observant opponents just fold. Good players BLUFF with the right hands at the right frequency.\n\n"
      "Best bluffs: SEMI-BLUFFS with draws. You win when they fold now, and when called you can still make the best hand.\n\n"
      "On the river there's nothing to draw to. A balanced player bluffs about bet / (pot + 2 x bet) of their betting range: around 25% for a half-pot bet, 33% for a pot-sized bet. "
      "Bluff with hands that can't win at showdown and that tell a believable story - not with hands that might win by checking.\n\n"
      "MINIMUM DEFENSE FREQUENCY (MDF) is the other side: facing a bet B into pot P you must continue with about P / (P + B) of your range, or the bettor profits by bluffing any two cards. "
      "That doesn't mean calling with everything - fold your weakest hands and keep the rest.\n\n"
      "Don't bluff calling stations. The Easy bots call far too much: just value-bet them relentlessly. The Hard bot adjusts this way too." },
    { "6. Omaha differences",
      "In Omaha everyone has 4 cards and 6 two-card combinations, so hands run much closer and strong hands are made far more often. Nut hands matter: a non-nut flush or the low end of a straight loses big pots.\n\n"
      "Good starting hands work together: aces (especially suited), double-suited hands, and RUNDOWNS like J-T-9-8 that make many straights. "
      "A DANGLER - a card that doesn't connect with the others - weakens the hand.\n\n"
      "Remember: exactly 2 of your cards + exactly 3 from the board. Four spades on the board with one spade in your hand is not a flush.\n\n"
      "Pre-flop equities are close (even AA vs a good rundown is around 60/40), so position matters even more than in Hold'em. "
      "Raise the pot when you open, and play far fewer hands from the blinds and early position.\n\n"
      "After the flop, draws are often favorites over made hands: a wrap straight draw can have 13-20 outs. Count your outs - and which of them make the NUTS." },
    { "7. Thinking through a hand",
      "Strong players don't decide by feel. They run through the same short list of questions at every decision. Quiz mode shows this list for each spot - use it until it's automatic.\n\n"
      "BEFORE YOU LOOK AT YOUR CARDS\n"
      "Who is at the table and how do they play (loose or tight, passive or aggressive)? How deep are the stacks? Who is to your left - they act after you all night.\n\n"
      "PREFLOP\n"
      "1. Position: how many players act after you, and will you be in position after the flop?\n"
      "2. Action so far: did anyone limp or raise? From which seat? An early-position raise is a strong range; a button raise is wide.\n"
      "3. Stack depth: short stacks (under ~20 BB) play push/fold; deep stacks favour suited and connected hands that make nut hands.\n"
      "4. Your hand in context: is it in the range for this exact spot? Raise, call or fold - and if you raise, to how much?\n\n"
      "ON EACH STREET AFTER THE FLOP\n"
      "1. The board: dry or wet? Which draws came in? Does it favour the preflop raiser's range (high cards) or the caller's (low, connected cards)?\n"
      "2. Their range: start from their preflop range and narrow it with every action. A check-call on a wet board is often a draw or a medium pair; a raise is usually two pair or better, or a big draw.\n"
      "3. Your hand against that range - not against one guessed hand. Are you ahead of most of it, behind most of it, or drawing?\n"
      "4. Price: pot odds when facing a bet. Do you have the equity? Are there implied odds (deep stacks, a hidden draw)?\n"
      "5. Purpose: if you bet, is it for VALUE (worse hands call) or as a BLUFF (better hands fold)? If neither, check. A bet with no purpose just loses money to better hands.\n"
      "6. Size: pick the size that does the job - small to keep worse hands in, big to charge draws or to push folds.\n"
      "7. Plan: what will you do on the next card if you're raised, or if the flush comes? Decide now, not when you're under pressure.\n\n"
      "AFTER THE HAND\n"
      "Was the decision good given what you knew? The result of one hand is mostly luck; the quality of the decision is what you control." },
    { "8. Ranges, not hands",
      "Beginners try to put an opponent on one exact hand ('he has AK'). Good players think in RANGES: all the hands that player would play this way, and how often.\n\n"
      "BUILDING A RANGE, STEP BY STEP (example)\n"
      "A tight player opens from UTG: about the top 15% - pairs, strong aces, suited broadways. You call on the button. Flop K-8-3 with two diamonds.\n"
      "They bet 1/3 pot: that's their whole range c-betting (strong hands and air alike), so it narrows little.\n"
      "Turn 2 of clubs, they bet 3/4 pot: now it's mostly Kx (top pair), overpairs, sets and diamond draws. Weak aces have mostly given up.\n"
      "River 7 of spades, they bet big again: value hands (KQ, AK, sets) plus missed diamond draws. That's the range you're playing against.\n\n"
      "COUNTING COMBOS\n"
      "Every pair can be dealt 6 ways, every suited hand 4 ways, every offsuit hand 12 ways. If you hold a king on that K-8-3 board, KK has only 1 combo left and AK only 12 instead of 16. "
      "This is the BLOCKER effect: your cards change what they can have.\n\n"
      "WHO HAS THE ADVANTAGE?\n"
      "On high-card boards the preflop raiser has more strong hands (AK, KK, AA) - their range is ahead, so they can bet small and often. "
      "On low, connected boards (7-6-5) the caller has more sets and straights - the raiser should check more.\n\n"
      "CAPPED RANGES\n"
      "When someone just calls preflop, they rarely have AA or KK (they'd have re-raised). Their range is CAPPED, which makes big bets and bluffs against them work better." },
    { "9. Reading real players",
      "Most real-life games are full of predictable players. The money comes from spotting their leaks and adjusting - not from fancy plays.\n\n"
      "THE MAIN TYPES (VPIP / PFR)\n"
      "Nit (12 / 9): very tight. Steal their blinds, fold to their big bets and raises.\n"
      "Tight-aggressive regular (22 / 18): solid. Avoid tough spots, play standard.\n"
      "Calling station (45 / 5): calls too much, rarely raises. Never bluff them; bet your good hands big on every street. When they finally raise, believe them.\n"
      "Maniac (60 / 40): bets and raises constantly. Tighten up, call down lighter with good hands, let them bluff into you.\n"
      "Weak-tight 'scared money': folds too much to pressure. Bluff and c-bet them more.\n\n"
      "WHAT TO WATCH\n"
      "How often they play hands and whether they raise or limp. What they showed down and how they played it - this is the most reliable information you'll get. "
      "Bet sizing: many recreational players bet big with strong hands and small with draws or weak hands. Patterns across many hands beat any single read.\n\n"
      "LIVE TELLS - USE WITH CARE\n"
      "Physical tells are far less reliable than betting patterns. Common ones: players acting weak (sighing, shrugging) often have a strong hand; acting strong (staring you down, splashing chips) is often weak. "
      "Quick calls usually mean a medium hand or a draw, not a monster. Use a tell only as a tie-breaker in a close decision.\n\n"
      "TAKE NOTES\n"
      "After a session, write one line per regular: 'calls 3 streets with any pair', 'only raises the river with the nuts'. Next time you sit down, you start with an edge." },
    { "10. Bankroll, variance and tilt",
      "You can play perfectly and still lose for weeks. Managing money and emotions is as important as strategy.\n\n"
      "BANKROLL\n"
      "Keep poker money separate. For cash games, have at least 20-40 buy-ins for your stake (for $1/$2 with $200 buy-ins: $4,000-$8,000). Tournaments need 100+ buy-ins. "
      "Move down when you drop below your limit - it's not failure, it's how pros survive downswings.\n\n"
      "VARIANCE\n"
      "Even an 80% favourite loses one time in five. A solid winner can be down 30+ buy-ins over a few thousand hands. Judge yourself on decisions, not results. "
      "Review the hands you lost AND the ones you won - a winning hand can be badly played.\n\n"
      "TILT\n"
      "Tilt is playing worse because of emotion: after a bad beat, a long losing run, or an annoying opponent. Signs: playing more hands, calling to 'see if they're bluffing', chasing losses, moving up in stakes.\n"
      "Set rules before you sit down: a stop-loss (for example 3 buy-ins), a time limit, and 'two bad beats in a row = a 10-minute walk'. Don't play tired, hungry, drunk or upset.\n\n"
      "GAME SELECTION\n"
      "Your win rate depends mostly on who you play against. A good player at a tough table can lose; an average player at a soft table wins. Look for tables with lots of limping, big multiway pots and players chatting and having fun." },
    { "11. How to keep improving",
      "STUDY ROUTINE\n"
      "Spend about as much time studying as playing. Work on one topic at a time (for example: defending the big blind, or turn barrels) for a week.\n"
      "Use quiz mode here: answer the 'think it through' questions out loud before every action, then compare with the coach. Watch the leaks list - fix the top one first.\n"
      "Mark hands that felt difficult and review them afterwards: write down the ranges, the pot odds and what each option wins. Hands you're unsure about teach you the most.\n\n"
      "THE FUNDAMENTALS THAT WIN THE MOST\n"
      "Play tight from early position and wider from the button. Raise or fold preflop - don't limp. Value-bet your strong hands, especially against players who call too much. "
      "Fold more to big river raises from passive players. Bluff less against calling stations and in multiway pots. Mind the pot odds when calling with draws.\n\n"
      "TAKING IT TO A LIVE CASINO\n"
      "Live games are usually looser and more passive than online: more limping, more calling. Bet bigger for value (they'll call), bluff less, and isolate limpers with bigger raises (4-6 BB plus 1 per limper).\n"
      "Etiquette and rules: act only in turn; say your action out loud ('raise to 30') - verbal declarations are binding; put your chips out in one motion (no 'string bets'); "
      "protect your cards with a chip; show both cards at showdown to win the pot; don't discuss a hand while it's being played; tip the dealer when you win a pot (common in US casinos).\n"
      "The rake takes a slice of every pot, so marginal calls and loose play cost even more in real games.\n\n"
      "FURTHER STEPS\n"
      "When the basics are solid, study with real solver outputs and range-training tools, track your results over thousands of hands, and discuss tough hands with stronger players." },
};
#define NLESSONS (int)(sizeof LESSONS / sizeof LESSONS[0])

static void DrawHelp(void) {
    Rectangle r = { VW / 2.0f - 470, 50, 940, 800 };
    Panel(r);
    DrawPages(r, "How to Play", HELP_TEXT, &helpScroll);
    if (Button((Rectangle){ r.x + r.width / 2 - 100, r.y + r.height - 66, 200, 46 }, "Close", true, C_BLUE)) showHelp = false;
}

static void DrawLessons(void) {
    Rectangle r = { VW / 2.0f - 470, 50, 940, 800 };
    Panel(r);
    // jump straight to a lesson: fundamentals (1-6) and real-table skills (7+)
    float bw = 52, bx = r.x + r.width / 2 - (NLESSONS * (bw + 6) - 6) / 2;
    for (int i = 0; i < NLESSONS; i++)
        if (Choice((Rectangle){ bx + i * (bw + 6), r.y + 16, bw, 30 }, TextFormat("%d", i + 1), lessonPage == i)) { lessonPage = i; helpScroll = 0; }
    TxtR(fReg, "Basics", bx - 10, r.y + 23, 13, C_MUTED);
    Txt(fReg, "At a real table", bx + NLESSONS * (bw + 6) + 4, r.y + 23, 13, C_MUTED);
    DrawPages((Rectangle){ r.x, r.y + 40, r.width, r.height - 40 }, LESSONS[lessonPage].title, LESSONS[lessonPage].body, &helpScroll);
    float by = r.y + r.height - 66;
    if (Button((Rectangle){ r.x + 40, by, 160, 46 }, "< Previous", lessonPage > 0, C_BLUE) || (lessonPage > 0 && KeyHit(KEY_LEFT))) { lessonPage--; helpScroll = 0; }
    if (Button((Rectangle){ r.x + r.width - 200, by, 160, 46 }, "Next >", lessonPage < NLESSONS - 1, C_BLUE) || (lessonPage < NLESSONS - 1 && KeyHit(KEY_RIGHT))) { lessonPage++; helpScroll = 0; }
    if (Button((Rectangle){ r.x + r.width / 2 - 90, by, 180, 46 }, "Close", true, (Color){ 100, 110, 130, 255 })) showLessons = false;
    TxtC(fReg, TextFormat("Lesson %d of %d", lessonPage + 1, NLESSONS), r.x + r.width / 2, by - 26, 14, C_MUTED);
}

static void DrawChart(void) {
    Rectangle r = { VW / 2.0f - 560, 30, 1120, 840 };
    Panel(r);
    TxtC(fBold, "Preflop Range Charts", r.x + r.width / 2, r.y + 20, 30, C_INK);
    int nc = chartCount();
    float tx = r.x + 30, ty = r.y + 66, tw = (r.width - 60 - 6 * 6) / 7.0f;
    for (int i = 0; i < nc; i++) {
        Rectangle b = { tx + (i % 7) * (tw + 6), ty + (i / 7) * 40, tw, 34 };
        if (Choice(b, chartGet(i)->name, chartTab == i)) chartTab = i;
    }
    const ChartDef *cd = chartGet(chartTab);
    Range rr, rc;
    rangeParse(&rr, cd->raise);
    bool hasCall = cd->call && cd->call[0];
    if (hasCall) rangeParse(&rc, cd->call); else memset(&rc, 0, sizeof rc);
    float cell = 44, gx = r.x + 40, gy = r.y + 152;
    int heroCls = -1;
    if (gameStarted && game.variant == V_HOLDEM) heroCls = hcClass(game.s[game.human].hole[0], game.s[game.human].hole[1]);
    for (int i = 0; i < NCLS; i++) {
        int row = i / 13, col = i % 13;
        Rectangle c = { gx + col * cell, gy + row * cell, cell - 2, cell - 2 };
        Color fill = (Color){ 222, 220, 212, 255 };
        bool two = hasCall || chartTab >= 6;
        if (rr.in[i]) fill = two ? (Color){ 210, 70, 60, 255 } : (Color){ 60, 160, 90, 255 };
        else if (rc.in[i]) fill = (Color){ 70, 130, 210, 255 };
        DrawRectangleRec(c, fill);
        char nm[4];
        hcClassName(i, nm);
        bool on = rr.in[i] || rc.in[i];
        TxtC(fBold, nm, c.x + c.width / 2, c.y + 13, 15, on ? WHITE : (Color){ 120, 118, 110, 255 });
        if (i == heroCls) DrawRectangleLinesEx((Rectangle){ c.x - 2, c.y - 2, c.width + 4, c.height + 4 }, 3, C_GOLD);
    }
    float ix = gx + 13 * cell + 30, iw = r.x + r.width - 40 - ix, iy = gy;
    iy += Wrap(fBold, cd->name, ix, iy, iw, 24, 30, C_INK, true) + 6;
    iy += Wrap(fReg, cd->desc, ix, iy, iw, 16, 23, C_INK, true) + 16;
    float fr = rangeFraction(&rr), fc = hasCall ? rangeFraction(&rc) : 0;
    bool two = hasCall || chartTab >= 6;
    DrawRectangle((int)ix, (int)iy + 2, 18, 18, two ? (Color){ 210, 70, 60, 255 } : (Color){ 60, 160, 90, 255 });
    Txt(fReg, TextFormat("%s: %.1f%% of hands", two ? (chartTab >= 11 ? "4-bet" : "3-bet") : "Raise", fr * 100), ix + 28, iy, 17, C_INK);
    iy += 28;
    if (two) {
        DrawRectangle((int)ix, (int)iy + 2, 18, 18, (Color){ 70, 130, 210, 255 });
        Txt(fReg, TextFormat("Call: %.1f%% of hands", fc * 100), ix + 28, iy, 17, C_INK);
        iy += 28;
    }
    DrawRectangle((int)ix, (int)iy + 2, 18, 18, (Color){ 222, 220, 212, 255 });
    Txt(fReg, "Fold", ix + 28, iy, 17, C_INK);
    iy += 44;
    iy += Wrap(fReg, "How to read it: pairs run down the diagonal. Suited hands are above it (AKs), offsuit hands below (AKo). "
                     "A suited combo comes in 4 ways, offsuit in 12, a pair in 6 - so offsuit hands make up most of a range.", ix, iy, iw, 15, 21, C_MUTED, true) + 10;
    if (heroCls >= 0) {
        char nm[4];
        hcClassName(heroCls, nm);
        Wrap(fReg, TextFormat("Your current hand (%s) is outlined in gold - top %.1f%% of starting hands.", nm, hcTop[heroCls] * 100), ix, iy, iw, 15, 21, C_INK, true);
    }
    TxtC(fReg, "Charts are for ~100 BB stacks at a 6-handed table. Short stacks play tighter and more all-in; the coach adjusts automatically.", r.x + r.width / 2, r.y + r.height - 100, 14, C_MUTED);
    if (Button((Rectangle){ r.x + r.width / 2 - 100, r.y + r.height - 70, 200, 46 }, "Close", true, C_BLUE)) showChart = false;
}

static void DrawGlossary(void) {
    Rectangle r = { VW / 2.0f - 580, 30, 1160, 840 };
    Panel(r);
    TxtC(fBold, "Poker Glossary", r.x + r.width / 2, r.y + 20, 32, C_INK);
    TxtC(fReg, "Every term the coach uses. Tip: during play, hover any stat, position or highlighted term to see its definition.", r.x + r.width / 2, r.y + 60, 15, C_MUTED);
    // search box: typing goes straight into it
    if (gLayer == gActive) {
        int ch, len = (int)strlen(glossQ);
        while ((ch = GetCharPressed()) > 0)
            if (ch >= 32 && ch < 127 && len < (int)sizeof glossQ - 1) { glossQ[len++] = (char)ch; glossQ[len] = 0; glossScroll = 0; glossFocus = -1; }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && len > 0) { glossQ[len - 1] = 0; glossScroll = 0; }
    }
    float lx = r.x + 30, lw = 240;
    Rectangle sb = { lx, r.y + 96, lw, 40 };
    DrawRectangleRounded(sb, 0.3f, 6, WHITE);
    DrawRectangleRoundedLinesEx(sb, 0.3f, 6, 1.5f, (Color){ 160, 150, 130, 255 });
    if (glossQ[0]) {
        Txt(fReg, glossQ, sb.x + 12, sb.y + 10, 18, C_INK);
        if (fmodf(animT, 1.0f) < 0.6f) DrawRectangle((int)(sb.x + 14 + TxtW(fReg, glossQ, 18)), (int)sb.y + 9, 2, 22, C_INK);
        if (Button((Rectangle){ sb.x + sb.width - 34, sb.y + 7, 26, 26 }, "x", true, (Color){ 150, 150, 160, 255 })) { glossQ[0] = 0; glossScroll = 0; }
    } else Txt(fReg, "Type to search...", sb.x + 12, sb.y + 11, 17, C_MUTED);
    float cy = sb.y + 54;
    for (int c = -1; c < GC_N; c++) {
        if (Choice((Rectangle){ lx, cy, lw, 38 }, c < 0 ? "All terms" : glossCatName(c), glossCat == c)) { glossCat = c; glossScroll = 0; glossFocus = -1; }
        cy += 44;
    }
    // the list
    Rectangle clip = { r.x + 300, r.y + 96, r.width - 340, r.height - 190 };
    float tw = clip.width - 24, total = 0, focusY = -1;
    int shown = 0;
    for (int i = 0; i < glossCount(); i++) {
        const GlossEntry *e = glossGet(i);
        if ((glossCat >= 0 && e->cat != glossCat) || !glossMatches(i, glossQ)) continue;
        if (i == glossFocus) focusY = total;
        total += 30 + Wrap(fReg, e->def, 0, 0, tw, 16, 23, C_INK, false) + 16;
        shown++;
    }
    float maxS = total > clip.height ? total - clip.height : 0;
    if (glossJump && focusY >= 0) { glossScroll = focusY; glossJump = false; }
    if (Hover(clip)) glossScroll -= GetMouseWheelMove() * 60;
    if (glossScroll < 0) glossScroll = 0;
    if (glossScroll > maxS) glossScroll = maxS;
    ClipBegin(clip);
    float yy = clip.y - glossScroll;
    for (int i = 0; i < glossCount(); i++) {
        const GlossEntry *e = glossGet(i);
        if ((glossCat >= 0 && e->cat != glossCat) || !glossMatches(i, glossQ)) continue;
        float h = 30 + Wrap(fReg, e->def, 0, 0, tw, 16, 23, C_INK, false) + 16;
        if (yy + h > clip.y && yy < clip.y + clip.height) {
            if (i == glossFocus) DrawRectangleRounded((Rectangle){ clip.x, yy - 4, clip.width - 8, h - 6 }, 0.1f, 6, (Color){ 250, 232, 170, 255 });
            Txt(fBold, e->term, clip.x + 12, yy + 2, 20, C_INK);
            TxtR(fReg, glossCatName(e->cat), clip.x + clip.width - 20, yy + 6, 13, C_MUTED);
            Wrap(fReg, e->def, clip.x + 12, yy + 30, tw, 16, 23, (Color){ 60, 62, 70, 255 }, true);
        }
        yy += h;
    }
    EndScissorMode();
    if (!shown) TxtC(fReg, TextFormat("No terms match \"%s\".", glossQ), clip.x + clip.width / 2, clip.y + 40, 18, C_MUTED);
    if (maxS > 0) TxtR(fReg, glossScroll < maxS ? "scroll for more" : "", clip.x + clip.width, clip.y + clip.height + 4, 13, C_MUTED);
    Txt(fReg, TextFormat("%d of %d terms", shown, glossCount()), lx, r.y + r.height - 58, 15, C_MUTED);
    if (Button((Rectangle){ r.x + r.width / 2 - 100, r.y + r.height - 70, 200, 46 }, "Close", true, C_BLUE)) showGloss = false;
}

static void DrawTopBar(void) {
    if (Button((Rectangle){ 20, 14, 96, 34 }, "Menu", true, (Color){ 60, 66, 80, 255 })) showMenu = true;
    if (Button((Rectangle){ 124, 14, 96, 34 }, "Help", true, (Color){ 60, 66, 80, 255 })) { showHelp = true; helpScroll = 0; }
    if (Button((Rectangle){ 228, 14, 120, 34 }, "Glossary", true, (Color){ 60, 66, 80, 255 })) OpenGlossary(-1);
    if (Button((Rectangle){ 356, 14, 110, 34 }, "Lessons", true, (Color){ 60, 66, 80, 255 })) { showLessons = true; helpScroll = 0; }
    if (!gameStarted) return;
    char t[96];
    snprintf(t, sizeof t, "%s / %s blinds  -  %s", Money(game.sbAmt), Money(game.bbAmt), gameMode == MODE_TRAIN ? "Training" : "Play");
    TxtR(fReg, t, 1160, 22, 16, C_DIM);
}

// ---------------------------------------------------------------- autotest (screenshots)

#ifdef AUTOTEST
static int atFrame = 0, atStage = 0, atShots = 0;
#ifndef SHOTDIR
#define SHOTDIR "."
#endif
static void Shot(const char *name) { TakeScreenshot(TextFormat("shot_%02d_%s.png", atShots++, name)); }
static void AutoTest(void) {
    atFrame++;
    speedIdx = 2;
    switch (atStage) {
    case 0: if (atFrame == 20) { Shot("menu"); setMode = MODE_TRAIN; setOpp = 5; setLv[1] = LV_EASY; setLv[3] = LV_HARD; autoDeal = true; StartNewGame(); atStage = 1; atFrame = 0; } break;
    case 1:
        if (game.toAct == game.human && !game.handOver && atFrame > 30) {
            static int n = 0;
            n++;
            if (n == 1) Shot("train_pre");
            if (n == 3 || n == 6 || n == 9) Shot("train_decision");
            if (n == 5) Shot("quiz_checklist");
            hintsBefore = n < 4 || n >= 5;
            Action a = { adv.act, adv.amount };
            if (n == 2) a = (Action){ A_CALL, 0 };
            HumanAct(a);
            atFrame = 0;
            if (n >= 12) { atStage = 2; atFrame = 0; }
        }
        if (game.handOver && atFrame > 40) { static int ho = 0; if (ho++ % 3 == 0) Shot("handover"); NextHand(); atFrame = 0; }
        if (atFrame > 4000) { atStage = 2; atFrame = 0; }
        break;
    case 2:                          // new learning screens, then the older overlays (stage 21)
        if (atFrame == 1) { fakeMouse = true; fakePos = (Vector2){ 1250, 205 }; }
        if (atFrame == 3) Shot("tooltip_stat");
        if (atFrame == 4) { fakePos = (Vector2){ 1260, 590 }; }
        if (atFrame == 6) { Shot("tooltip_panel"); fakeMouse = false; }
        if (atFrame == 8) OpenGlossary(glossLookup("Minimum defense frequency (MDF)"));
        if (atFrame == 12) { Shot("glossary_focus"); snprintf(glossQ, sizeof glossQ, "pot"); }
        if (atFrame == 16) { Shot("glossary_search"); showGloss = false; showLessons = true; lessonPage = 6; helpScroll = 0; }
        if (atFrame == 20) { Shot("lesson7"); showLessons = false; atStage = 21; atFrame = 0; }
        break;
    case 21: if (atFrame == 5) { showChart = true; chartTab = 3; } if (atFrame == 15) { Shot("chart"); showChart = false; showLessons = true; lessonPage = 2; } if (atFrame == 25) { Shot("lesson"); showLessons = false; showHelp = true; } if (atFrame == 35) { Shot("help"); showHelp = false; panelTab = 1; } if (atFrame == 45) { Shot("log"); panelTab = 0; atStage = 3; atFrame = 0; } break;
    case 3:
        if (atFrame == 5) { setVariant = V_OMAHA; setLimit = BL_POTLIMIT; setMode = MODE_TRAIN; setOpp = 3; StartNewGame(); atStage = 30; atFrame = 0; }
        break;
    case 30: {                       // rearrange the hero's cards with a simulated mouse
        static int f0 = -1;
        if (f0 < 0 && game.toAct == game.human && atFrame > 30) f0 = atFrame;
        if (f0 < 0) break;
        int t = atFrame - f0;
        Vector2 sp = SPOT[SpotFor(game.human)];
        Rectangle a = HeroSlot(sp, 0), b = HeroSlot(sp, 3), c1 = HeroSlot(sp, 1);
        Vector2 ca = { a.x + 20, a.y + 50 }, cb = { b.x + 40, b.y + 50 }, cc = { c1.x + 30, c1.y + 50 };
        const Seat *h = &game.s[game.human];
        char o[64];
        #define ORDER() do { o[0] = 0; for (int k = 0; k < 4; k++) { char cs[3]; cardStr(h->hole[arr.order[k]], cs); strcat(o, cs); strcat(o, " "); } } while (0)
        fakeMouse = true;
        if (t == 0) { ORDER(); printf("AT dealt order: %s\n", o); fakePos = ca; fakeDown = false; }
        if (t == 1) fakeDown = true;
        if (t >= 2 && t <= 14) { float u = (t - 2) / 12.0f; fakePos = (Vector2){ ca.x + (cb.x - ca.x) * u, ca.y - 30 * sinf(u * PI) }; }
        if (t == 9) Shot("drag_mid");
        if (t == 16) fakeDown = false;
        if (t == 18) { ORDER(); printf("AT after drag 1->4: %s\n", o); Shot("drag_done"); }
        if (t == 20) { fakePos = (Vector2){ HeroSlot(sp, 0).x + 20, HeroSlot(sp, 0).y + 50 }; fakeDown = true; }
        if (t == 21) fakeDown = false;
        if (t == 23) Shot("click_selected");
        if (t == 24) { fakePos = cc; fakeDown = true; }
        if (t == 25) fakeDown = false;
        if (t == 27) { ORDER(); printf("AT after click-swap 1<->2: %s\n", o); }
        if (t == 29) { sortPref = SORT_SUIT; arrSort(&arr, h->hole, SORT_SUIT); ORDER(); printf("AT sorted by suit: %s\n", o); }
        if (t == 31) { Shot("sorted_suit"); sortPref = SORT_RANK; arrSort(&arr, h->hole, SORT_RANK); ORDER(); printf("AT sorted by rank: %s\n", o); fakeMouse = false; fflush(stdout); atStage = 31; atFrame = 0; }
        break;
    }
    case 31:
        if (game.toAct == game.human && !game.handOver && atFrame > 30) {
            static int m = 0;
            if (m++ == 2) { Shot("omaha"); }
            HumanAct((Action){ adv.act, adv.amount });
            atFrame = 6;
            if (m > 6) { atStage = 4; atFrame = 0; }
        }
        if (game.handOver && atFrame > 40) { NextHand(); atFrame = 6; }
        if (atFrame > 4000) { atStage = 4; atFrame = 0; }
        break;
    case 4:
        if (atFrame == 5) { setVariant = V_HOLDEM; setLimit = BL_NOLIMIT; setMode = MODE_PLAY; setOpp = 2; StartNewGame(); }
        if (game.toAct == game.human && !game.handOver && atFrame > 30) { static int k = 0; if (k++ == 1) Shot("play"); HumanAct((Action){ A_CALL, 0 }); atFrame = 6; if (k > 4) { atStage = 5; atFrame = 0; } }
        if (game.handOver && atFrame > 40) { static int z = 0; if (z++ == 0) Shot("play_over"); NextHand(); atFrame = 6; }
        if (atFrame > 4000) atStage = 5;
        break;
    default: CloseWindow(); exit(0);
    }
}
#endif

// ---------------------------------------------------------------- main

static Font TryLoadFont(const char *name, int size, int *cps, int ncp, bool *ok) {
    char path[700];
    snprintf(path, sizeof path, "%sassets/%s", GetApplicationDirectory(), name);
    if (!FileExists(path)) snprintf(path, sizeof path, "assets/%s", name);
    if (FileExists(path)) {
        Font f = LoadFontEx(path, size, cps, ncp);
        if (f.texture.id != 0 && f.glyphCount > 0) {
            GenTextureMipmaps(&f.texture);
            SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
            *ok = true;
            return f;
        }
    }
    *ok = false;
    return GetFontDefault();
}

#ifdef __EMSCRIPTEN__
// The page stretches the canvas over its container with CSS; give it one
// pixel per device pixel so text stays sharp on HiDPI screens. (Mouse
// coordinates arrive already scaled to canvas pixels.)
static void FitCanvas(void) {
    double cw, ch;
    if (emscripten_get_element_css_size("#canvas", &cw, &ch) != EMSCRIPTEN_RESULT_SUCCESS) return;
    double dpr = emscripten_get_device_pixel_ratio();
    int w = (int)(cw * dpr + 0.5), h = (int)(ch * dpr + 0.5);
    if (w > 0 && h > 0 && (w != GetScreenWidth() || h != GetScreenHeight())) SetWindowSize(w, h);
}
#endif

// One frame: input, game update and drawing
static void Frame(void) {
#ifdef __EMSCRIPTEN__
    FitCanvas();
#endif
    float dt = GetFrameTime();
    animT += dt;
    if (flashT > 0) flashT -= dt;
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float zoom = fminf(sw / VW, sh / VH);
    cam.zoom = zoom;
    cam.offset = (Vector2){ (sw - VW * zoom) / 2, (sh - VH * zoom) / 2 };
    cam.target = (Vector2){ 0, 0 };
    cam.rotation = 0;
    mouse = GetScreenToWorld2D(GetMousePosition(), cam);

    bool overlay = showMenu || showHelp || showLessons || showChart || showGloss;
    gActive = overlay ? 2 : 0;
#ifdef AUTOTEST
    if (fakeMouse) mouse = fakePos;
#endif
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (showGloss) showGloss = false;
        else if (showHelp) showHelp = false;
        else if (showLessons) showLessons = false;
        else if (showChart) showChart = false;
        else if (showMenu) { if (gameStarted) showMenu = false; }
        else showMenu = true;
    } else if (IsKeyPressed(KEY_H) && !showMenu && !showGloss) { showHelp = !showHelp; helpScroll = 0; }
    else if (IsKeyPressed(KEY_L) && !overlay && gameMode != MODE_TRAIN) { showLessons = true; helpScroll = 0; }
    else if (IsKeyPressed(KEY_T) && !overlay) OpenGlossary(-1);
    else if (IsKeyPressed(KEY_M) && !showGloss) { sndSetMuted(!sndMuted()); Flash(sndMuted() ? "Sound off" : "Sound on"); }
    overlay = showMenu || showHelp || showLessons || showChart || showGloss;
    gActive = overlay ? 2 : 0;

    if (gameStarted && !overlay) UpdateGame(dt);
    sndUpdate(dt);
#ifdef AUTOTEST
    AutoTest();
#endif

    BeginDrawing();
    ClearBackground(C_BG);
    BeginMode2D(cam);
    DrawRectangle(0, 0, VW, VH, C_BG);
    gLayer = 0;
    DrawTable();
    DrawControls();
    DrawPanel();
    DrawTopBar();
    if (flashT > 0 && flashMsg[0]) {
        float w = TxtW(fBold, flashMsg, 18) + 36;
        float a = flashT > 0.5f ? 1 : flashT * 2;
        Rectangle b = { TCX - w / 2, game.handOver ? 484 : 438, w, 34 };
        if (b.x < 10) b.x = 10;
        DrawRectangleRounded(b, 0.5f, 8, Fade((Color){ 20, 24, 30, 255 }, 0.92f * a));
        DrawRectangleRoundedLinesEx(b, 0.5f, 8, 1.5f, Fade(C_GOLD, a));
        TxtC(fBold, flashMsg, b.x + b.width / 2, b.y + 8, 18, Fade(WHITE, a));
    }
    DrawTip();
    gLayer = 2;
    if (showGloss) DrawGlossary();     // help and the glossary can be opened from the menu, so they draw on top
    else if (showHelp) DrawHelp();
    else if (showMenu) DrawMenu();
    else if (showLessons) DrawLessons();
    else if (showChart) DrawChart();
    EndMode2D();
    EndDrawing();
#ifdef AUTOTEST
    fakeDownPrev = fakeDown;
#endif
}

int main(void) {
#ifdef AUTOTEST
    ChangeDirectory(SHOTDIR);
    freopen(SHOTDIR "/at.log", "w", stdout);
#endif
#ifdef __EMSCRIPTEN__
    SetConfigFlags(FLAG_MSAA_4X_HINT);  // FitCanvas() handles the size
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
#endif
    InitWindow(1440, 810, "Poker Trainer");
    SetWindowMinSize(800, 450);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    uint64_t e = Entropy();
    SetRandomSeed((unsigned)(e ^ (e >> 32)) ^ 0x5A5Au);

    bool ok1, ok2;
    int ui[110], nui = 0;
    for (int c = 32; c < 127; c++) ui[nui++] = c;
    fBold = TryLoadFont("ui_bold.otf", 64, ui, nui, &ok1);
    fReg = TryLoadFont("ui_regular.otf", 48, ui, nui, &ok2);
    hasUI = ok1 && ok2;
    if (!ok2) fReg = fBold;
    rangesInit();
    sndInit();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(Frame, 0, 1);  // the browser drives the frames; never returns
#else
    while (!WindowShouldClose()) Frame();
#endif
    sndClose();
    CloseWindow();
    return 0;
}
