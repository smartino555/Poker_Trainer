// rules.c - the rules coach: rule sheets for every game, plus explanations of what just happened,
// what you may do now and why each pot was won. Rules follow Wikipedia's articles on each game.
#include "rules.h"
#include "basic.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *SUITN[4] = { "clubs", "diamonds", "hearts", "spades" };
static const char *RNAME[13] = { "2", "3", "4", "5", "6", "7", "8", "9", "10", "Jack", "Queen", "King", "Ace" };
static const char LOWCH[14] = "A23456789TJQK";
#define LOWTOP (1 << 24)

void cardLong(int c, char *buf, int n) { snprintf(buf, n, "%s of %s", RNAME[cardRank(c)], SUITN[cardSuit(c)]); }

static const char *M(int v) {
    static char buf[8][24];
    static int k = 0;
    char *b = buf[k++ & 7];
    fmtChips(v, b, 24);
    return b;
}

static const VariantInfo *VI(const Game *g) { return varInfo(g->variant); }
static const char *NM(const Game *g, int p) { return g->s[p].name; }
static const char *V(const Game *g, int p, const char *you, const char *they) { return p == g->human ? you : they; }
static const char *POSS(const Game *g, int p) {
    static char buf[4][32];
    static int k = 0;
    char *b = buf[k++ & 3];
    if (p == g->human) snprintf(b, 32, "your");
    else snprintf(b, 32, "%s's", g->s[p].name);
    return b;
}

// true the first time a concept comes up in this game (then it gets the long explanation)
static bool first(RulesMemory *mem, const Game *g, int rc) {
    if (!mem) return true;
    bool f = !mem->seen[g->variant][rc];
    mem->seen[g->variant][rc] = true;
    return f;
}

static void put(char *out, int n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out, n, fmt, ap);
    va_end(ap);
}

static void add(char *out, int n, const char *fmt, ...) {
    size_t l = strlen(out);
    if ((int)l >= n - 1) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out + l, n - l, fmt, ap);
    va_end(ap);
}

static void upList(const Game *g, int p, char *buf, int n) {
    int up[MAXH], k = gameUpCards(g, p, up);
    buf[0] = 0;
    for (int i = 0; i < k; i++) add(buf, n, "%s%s", i ? " " : "", RNAME[cardRank(up[i])]);
}

static void showingName(const Game *g, int p, char *buf, int n) {
    int up[MAXH], k = gameUpCards(g, p, up);
    char cs[40];
    upList(g, p, cs, sizeof cs);
    if (g->variant == V_RAZZ) { snprintf(buf, n, "%s", cs); return; }
    char hn[64];
    handName(evalHand(up, k), hn, sizeof hn);
    if (handCat(evalHand(up, k)) == HC_HIGH) snprintf(buf, n, "%s (%s high)", cs, RNAME[handRank1(evalHand(up, k))]);
    else snprintf(buf, n, "%s (%s)", cs, hn);
}

// ---------------------------------------------------------------- comparing two hands

static const char *ORD[5] = { "highest", "second-highest", "third", "fourth", "fifth" };

void rulesCompare(int variant, int w, int l, char *buf, int n) {
    int rk = varInfo(variant)->rank;
    if (rk == RK_HIGH || rk == RK_HILO8) {
        int vw = w & 0xFFFFFF, vl = l & 0xFFFFFF, cw = handCat(vw), cl = handCat(vl);
        if (cw != cl) {
            if (variant == V_SHORTDECK && ((cw == HC_FLUSH && cl == HC_FULLHOUSE) || (cw == HC_FULLHOUSE && cl == HC_FLUSH)))
                snprintf(buf, n, "%s beats %s - in short deck a flush outranks a full house", catName(cw), catName(cl));
            else snprintf(buf, n, "%s beats %s", catName(cw), catName(cl));
            return;
        }
        int i = 0, a = 0, b = 0;
        for (; i < 5; i++) { a = vw >> (16 - 4 * i) & 15; b = vl >> (16 - 4 * i) & 15; if (a != b) break; }
        if (i == 5) { snprintf(buf, n, "the hands are identical - a tie (suits never break ties)"); return; }
        switch (cw) {
        case HC_PAIR:
            if (i == 0) snprintf(buf, n, "both have one pair; %s beat %s", rankPlural(a), rankPlural(b));
            else snprintf(buf, n, "same pair, so the kickers decide: %s beats %s", rankName(a), rankName(b));
            break;
        case HC_TWOPAIR:
            if (i == 0) snprintf(buf, n, "both have two pair; the top pair decides: %s beat %s", rankPlural(a), rankPlural(b));
            else if (i == 1) snprintf(buf, n, "same top pair, so the second pair decides: %s beat %s", rankPlural(a), rankPlural(b));
            else snprintf(buf, n, "same two pairs, so the kicker decides: %s beats %s", rankName(a), rankName(b));
            break;
        case HC_TRIPS:
            if (i == 0) snprintf(buf, n, "three %s beat three %s", rankPlural(a), rankPlural(b));
            else snprintf(buf, n, "same three of a kind, so the kickers decide: %s beats %s", rankName(a), rankName(b));
            break;
        case HC_STRAIGHT: case HC_STFLUSH:
            snprintf(buf, n, "the higher straight wins: %s-high beats %s-high", rankName(a), rankName(b));
            break;
        case HC_FULLHOUSE:
            if (i == 0) snprintf(buf, n, "both have a full house; the three of a kind decide: %s beat %s", rankPlural(a), rankPlural(b));
            else snprintf(buf, n, "same three of a kind, so the pair decides: %s beat %s", rankPlural(a), rankPlural(b));
            break;
        case HC_QUADS:
            if (i == 0) snprintf(buf, n, "four %s beat four %s", rankPlural(a), rankPlural(b));
            else snprintf(buf, n, "same four of a kind, so the kicker decides: %s beats %s", rankName(a), rankName(b));
            break;
        default:              // flush or high card: card by card from the top
            if (i == 0) snprintf(buf, n, "%s: the highest card decides, %s beats %s", cw == HC_FLUSH ? "both have a flush" : "no pair for either", rankName(a), rankName(b));
            else snprintf(buf, n, "%s: the top %d card%s match, so the %s card decides: %s beats %s", cw == HC_FLUSH ? "both have a flush" : "no pair for either",
                          i, i == 1 ? "" : "s", ORD[i], rankName(a), rankName(b));
            break;
        }
        return;
    }
    if (rk == RK_LOW_A5 || rk == RK_LOW_27) {
        bool a5 = rk == RK_LOW_A5;
        int kw = LOWTOP - w, kl = LOWTOP - l, cw = (kw >> 20) & 15, cl = (kl >> 20) & 15;
        static const char *LCAT[6] = { "no pair", "one pair", "two pair", "three of a kind", "a full house", "four of a kind" };
        if (cw != cl) {
            if (a5) snprintf(buf, n, "%s beats %s - in lowball, pairs count against you", LCAT[cw], LCAT[cl]);
            else snprintf(buf, n, "%s beats %s - in 2-7, pairs, straights and flushes all count against you", catName(cw), catName(cl));
            return;
        }
        int i = 0, a = 0, b = 0;
        for (; i < 5; i++) { a = kw >> (16 - 4 * i) & 15; b = kl >> (16 - 4 * i) & 15; if (a != b) break; }
        if (i == 5) { snprintf(buf, n, "the hands are the same - a tie"); return; }
        char ca = a5 ? LOWCH[a] : RANKCH[a], cb = a5 ? LOWCH[b] : RANKCH[b];
        if (i == 0) snprintf(buf, n, "lows compare from the top card down: %c beats %c (lower is better%s)", ca, cb, a5 ? "" : "; aces are high");
        else snprintf(buf, n, "the top %d card%s match, so the %s card decides: %c beats %c (lower is better)", i, i == 1 ? "" : "s", ORD[i], ca, cb);
        return;
    }
    // badugi
    int kw = (1 << 20) - w, kl = (1 << 20) - l, sw = 4 - (kw >> 16), sl = 4 - (kl >> 16);
    if (sw != sl) { snprintf(buf, n, "a %d-card hand beats any %d-card hand", sw, sl); return; }
    for (int i = 0; i < sw; i++) {
        int a = kw >> (12 - 4 * i) & 15, b = kl >> (12 - 4 * i) & 15;
        if (a == b) continue;
        if (i == 0) snprintf(buf, n, "both are %d-card hands; the highest card decides: %c beats %c (lower is better)", sw, LOWCH[a], LOWCH[b]);
        else snprintf(buf, n, "the top %d card%s match, so the next card decides: %c beats %c", i, i == 1 ? "" : "s", LOWCH[a], LOWCH[b]);
        return;
    }
    snprintf(buf, n, "the hands are the same - a tie");
}

// ---------------------------------------------------------------- rule sheets

static const char *RANK_HIGH =
    "HAND RANKINGS (best to worst)\n"
    "Straight flush (A-K-Q-J-10 of one suit is a royal flush) - Four of a kind - Full house - Flush - Straight - Three of a kind - Two pair - One pair - High card.\n"
    "Only five cards count. Hands of the same kind are compared by rank, and then by the leftover cards (kickers). Suits never break a tie: equal hands split the pot. "
    "An ace is high, or low in the straight 5-4-3-2-A; straights don't wrap around (Q-K-A-2-3 is not a straight).\n\n";

static const char *RANK_A5 =
    "LOW HANDS (ace-to-five)\n"
    "The lowest hand wins. Aces are low, and straights and flushes don't count against you, so the best hand is 5-4-3-2-A (the wheel). "
    "Hands are compared from the highest card down: 7-5-4-3-2 beats 7-6-3-2-A, and any 7-low beats any 8-low. Any hand without a pair beats any hand with one.\n\n";

static const char *RANK_27 =
    "LOW HANDS (deuce-to-seven)\n"
    "The lowest hand wins, but here aces are always HIGH, and straights and flushes count against you. The best hand is 7-5-4-3-2 of mixed suits. "
    "A-2-3-4-5 is not a straight here - it's just an ace-high hand, which is bad. 2-3-4-5-6 is a straight, also bad. "
    "Compare from the highest card down (8-5-4-3-2 beats 9-7-6-4-3). Any no-pair hand beats any pair.\n\n";

static const char *RANK_BADUGI =
    "HAND RANKINGS (badugi)\n"
    "Only cards of different suits AND different ranks count. Four of them make a badugi, the best kind of hand; if your cards repeat a suit or a rank, "
    "only the best of the repeated cards plays and you have a 3-card (or 2-card) hand. Any badugi beats any 3-card hand, which beats any 2-card hand. "
    "Aces are low. Between hands with the same number of cards, the lower highest card wins, then the next. The best hand is A-2-3-4 of four suits; the worst is four kings. "
    "Example (from Wikipedia): ace of spades, 5 of diamonds, 9 of diamonds, 9 of hearts is the 3-card hand 9-5-A.\n\n";

static const char *RANK_HILO =
    "SPLIT POT: HIGH AND LOW\n"
    "At the showdown the pot is split in half: one half to the best high hand (normal rankings), the other to the best low hand. "
    "To win low, a hand needs five cards of different ranks, 8 or lower ('eight or better'). Aces count as both high and low; straights and flushes don't spoil a low. "
    "If nobody has a qualifying low, the high hand wins the whole pot. One player can win both halves - a 'scoop'. "
    "Tied hands share their half, so you can win just a quarter of the pot ('getting quartered'). An odd chip goes to the high half. "
    "Your cards speak for themselves: you don't declare high or low.\n\n";

static void structureText(int limit, int variant, char *buf, int n) {
    const VariantInfo *vi = varInfo(variant);
    if (limit == BL_FIXED) {
        const char *late = vi->family == FAM_BOARD ? "the turn and river" : vi->family == FAM_STUD ? "5th, 6th and 7th street" : vi->draws == 1 ? "after the draw" : "after the second and third draws";
        const char *early = vi->family == FAM_BOARD ? "preflop and on the flop" : vi->family == FAM_STUD ? "3rd and 4th street" : vi->draws == 1 ? "before the draw" : "before the first draw and after the first";
        add(buf, n, "BETTING: FIXED LIMIT\nEvery bet and raise is a set size: the small bet %s, and the big bet (twice as much) on %s. "
                    "You choose only whether to bet or raise, not how much. A round allows one bet and three raises - then it's capped and players can only call or fold. "
                    "When just two players are left at the start of a round there is no cap.\n\n", early, late);
    } else if (limit == BL_POTLIMIT)
        add(buf, n, "BETTING: POT LIMIT\nThe biggest bet allowed is the size of the pot. To raise, first count your call into the pot: the raise can be as big as that total. "
                    "The smallest raise is the size of the previous bet or raise.\n\n");
    else
        add(buf, n, "BETTING: NO LIMIT\nYou may bet any amount from the minimum (one big blind) up to all your chips. A raise must be at least as big as the previous bet or raise. "
                    "An all-in for less than a full raise does not reopen the betting for players who have already acted.\n\n");
}

int rulesSheet(int variant, int limit, char *buf, int n) {
    const VariantInfo *vi = varInfo(variant);
    buf[0] = 0;
    switch (variant) {
    case V_HOLDEM: case V_SHORTDECK:
        add(buf, n, "GOAL\nWin the chips in the pot: make the best five-card hand at the showdown, or bet so that everyone else folds.\n\n");
        add(buf, n, "THE DEAL\nEach player gets 2 cards face down (hole cards). Five shared cards come face up in the middle: three at once (the flop), then one (the turn), then one more (the river). "
                    "Your hand is the best five of your 2 cards and the 5 shared cards - you may use both, one or none of your own cards. If the board itself is the best hand, you 'play the board' and can only split.\n\n");
        add(buf, n, "BLINDS AND ORDER\nThe dealer button moves one seat left each hand. The two players to its left post the small blind and big blind - forced bets so there's always something to win. "
                    "There are four betting rounds: preflop, flop, turn and river. Preflop, the player left of the big blind acts first; on later rounds the first player left of the button does, and the button acts last. "
                    "Heads-up, the button posts the small blind and acts first preflop, last after that.\n\n");
        if (variant == V_SHORTDECK)
            add(buf, n, "SHORT DECK (SIX PLUS)\nThe 2s, 3s, 4s and 5s are removed, leaving 36 cards. The ace still plays high, but also low in the smallest straight, A-6-7-8-9. "
                        "With fewer cards, flushes are harder to make than full houses, so a FLUSH BEATS A FULL HOUSE. Everything else ranks as usual, with a straight above three of a kind "
                        "(Wikipedia: 'how it's ranked in most games' - some rooms rank three of a kind above a straight instead). Many games use antes instead of blinds; this table uses blinds.\n\n");
        add(buf, n, "%s", RANK_HIGH);
        break;
    case V_OMAHA: case V_OMAHA5: case V_OMAHA8:
        add(buf, n, "GOAL\nWin the pot with the best hand at the showdown, or by making everyone fold.\n\n");
        add(buf, n, "THE DEAL\nLike Hold'em - blinds, a flop, turn and river, four betting rounds - but each player gets %d cards face down. "
                    "The key rule: your hand must use EXACTLY TWO of your cards and EXACTLY THREE of the shared cards. Four hearts on the board and one heart in your hand is NOT a flush; "
                    "three aces in your hand count as only a pair.\n\n", vi->nhole);
        if (variant == V_OMAHA8)
            add(buf, n, "OMAHA HI-LO\nThe pot is split between the best high hand and the best low hand. You build the two hands separately - each from exactly two of your cards and three from the board - "
                        "and they may use different cards from your hand. A low needs three different board cards of 8 or lower, so on some boards no low is possible.\n\n%s", RANK_HILO);
        if (variant == V_OMAHA5) add(buf, n, "FIVE-CARD OMAHA ('BIG O')\nWith five cards there are ten two-card combinations, so strong hands are common. It's usually played pot-limit and high only.\n\n");
        add(buf, n, "%s", RANK_HIGH);
        break;
    case V_STUD: case V_STUD8: case V_RAZZ: {
        bool razz = variant == V_RAZZ;
        add(buf, n, "GOAL\n%s\n\n", razz ? "Make the LOWEST five-card hand from your seven cards (or make everyone fold)." :
                                  variant == V_STUD8 ? "Win the high half, the low half, or both (a scoop), with the best five of your seven cards." : "Make the best five-card hand from your seven cards (or make everyone fold).");
        add(buf, n, "THE DEAL\nThere are no shared cards and no blinds. Everyone puts in an ante, then gets two cards face down and one face up (the door card). "
                    "Each later round brings one more card: 4th, 5th and 6th street face up, 7th street (the river) face down - two down, four up, one down. There are five betting rounds. "
                    "Everyone can see your up cards and you can see theirs: that's most of the skill in stud. Cards that folded players showed are 'dead' - they can't come to anyone.\n\n");
        add(buf, n, "THE BRING-IN\nOn 3rd street the player with the %s up card must start the betting with the bring-in, a forced bet smaller than a full bet. "
                    "%s A tie in rank is broken by suit: from lowest to highest, clubs, diamonds, hearts, spades. "
                    "The bring-in player may instead 'complete' to a full small bet. Players after a bring-in may fold, call it, or complete; completing counts as the bet, so the first raise goes to two small bets.\n\n",
            razz ? "HIGHEST" : "lowest", razz ? "In Razz aces are low, so a king is the worst card and the king of spades always brings in." : "Here aces count high, so a deuce is the lowest card.");
        add(buf, n, "WHO ACTS FIRST\nFrom 4th street on, the player whose up cards make the %s hand acts first - pairs and high cards only, since straights and flushes need five cards. "
                    "Equal up cards: the player nearest the dealer's left goes first.%s\n\n",
            razz ? "LOWEST" : "best", variant == V_STUD ? " In seven-card stud, an open pair (a pair among the up cards) on 4th street lets any player bet the big bet instead of the small bet." : "");
        add(buf, n, "RUNNING OUT OF CARDS\nWith eight players the deck can run short on 7th street; then one card is dealt face up in the middle for everyone to share. (This table has at most six players, so it never happens here.)\n\n");
        if (variant == V_STUD8) add(buf, n, "STUD HI-LO\nYou choose any five of your seven cards for high and any five for low - they may overlap. Example from Wikipedia: 7-7-6-4-4-3-2 is two pair for high and a 7-6-4-3-2 low.\n\n%s", RANK_HILO);
        add(buf, n, "%s", razz ? RANK_A5 : RANK_HIGH);
        break;
    }
    default: {                                           // draw games
        bool low = vi->rank != RK_HIGH;
        add(buf, n, "GOAL\n%s\n\n", variant == V_BADUGI ? "Make the best badugi - four low cards of four different suits - after up to three draws." :
                                  low ? "Make the LOWEST hand by deuce-to-seven rules." : "Make the best five-card poker hand after one chance to replace cards.");
        add(buf, n, "THE DEAL\nPlayers post blinds (casino style), then each gets %d cards face down. There are no shared cards and nobody sees anyone else's cards. "
                    "After the first betting round comes a draw: in turn, starting left of the button, each player throws away any cards they don't want and gets new ones from the deck. "
                    "Keeping all your cards is 'standing pat'. %s\n\n", vi->nhole,
            vi->draws == 3 ? "There are three draws and four betting rounds." : "There is one draw, then a final betting round and the showdown.");
        add(buf, n, "THE DRAW\nThis table follows the casino rule: you may throw away any number of cards. (Many home games allow at most three - or four if you keep an ace.) "
                    "Watch how many cards each player takes: standing pat usually means a finished hand, drawing one means close to one, drawing three or more means starting over. "
                    "If the deck runs out, the discards are shuffled into a new deck - but you never get back the cards you just threw away.\n\n");
        add(buf, n, "ORDER\nBefore the first draw, the player left of the big blind acts first. After a draw, the first player left of the button bets first, and the button acts last.\n\n");
        if (variant == V_DRAW5) add(buf, n, "OLD-SCHOOL VARIANTS\nIn 'jackpots' you need a pair of jacks or better to open the betting. Home games often use antes instead of blinds.\n\n");
        if (variant == V_SD27) add(buf, n, "KANSAS CITY LOWBALL\nSingle-draw deuce-to-seven is traditionally played no-limit: one draw, so bluffing by standing pat is a big part of the game.\n\n");
        add(buf, n, "%s", variant == V_BADUGI ? RANK_BADUGI : low ? RANK_27 : RANK_HIGH);
        break;
    }
    }
    structureText(limit, variant, buf, n);
    add(buf, n, "SHOWDOWN\nIf more than one player is left after the last betting round, the hands are shown and the best wins. If a bet isn't called, the bettor wins without showing. "
                "A player who is all-in can only win as much from each opponent as they put in themselves; bets beyond that go into a side pot for the others.");
    return (int)strlen(buf);
}

int rulesMixedSheet(int mix, char *buf, int n) {
    buf[0] = 0;
    add(buf, n, "MIXED GAMES\nA mixed game rotates through several poker games, playing each for a fixed number of hands - here one orbit, so each player deals once - before moving on. "
                "It tests all-round skill: the rules, the betting structure and the hand rankings change as the games change.\n\n");
    if (mix == MIX_HORSE)
        add(buf, n, "HORSE\nThe letters name the games: Hold'em, Omaha hi-lo (eight or better), Razz, Seven-card Stud and Stud hi-lo (Eight or better). All are played fixed limit here.\n\n");
    else
        add(buf, n, "8-GAME\nEight games: 2-7 Triple Draw, limit Hold'em, Omaha hi-lo, Razz, Seven-card Stud, Stud hi-lo, no-limit Hold'em and pot-limit Omaha.\n\n");
    add(buf, n, "Press H during a hand to read the full rules of the game being played right now.");
    return (int)strlen(buf);
}

// ---------------------------------------------------------------- events

static const char *drawWord(int k, int draws) {
    static const char *W[3] = { "first", "second", "third" };
    return draws == 1 ? "" : W[k < 0 ? 0 : k > 2 ? 2 : k];
}

bool rulesNarrate(const Game *g, const GameEvent *e, RulesMemory *mem, char *out, int n) {
    const VariantInfo *vi = VI(g);
    int p = e->seat;
    bool fixed = gameIsFixed(g);
    out[0] = 0;
    char cs[48], sh[80];
    switch (e->type) {
    case EV_GAME:
        put(out, n, "Next game: %s %s, for one orbit. %s Press H for its full rules.", limitName(e->b), varInfo(e->a)->name, varInfo(e->a)->blurb);
        return true;
    case EV_HAND:
        put(out, n, "Hand #%d of %s %s. %s %s the dealer button%s.", e->a, limitName(g->limit), vi->name, NM(g, g->button), V(g, g->button, "have", "has"),
            vi->family == FAM_STUD ? " (in stud it only breaks ties for who acts first)" : "");
        return true;
    case EV_BLINDS: {
        int sb = p, bb = e->a;
        if (!first(mem, g, RC_BLINDS)) { put(out, n, "Blinds: %s %s, %s %s.", NM(g, sb), M(g->sbAmt), NM(g, bb), M(g->bbAmt)); return true; }
        if (g->n == 2)
            put(out, n, "Heads-up blinds: the dealer (%s) posts the small blind, %s, and %s posts the big blind, %s. The dealer acts first %s and last after it.",
                NM(g, sb), M(g->sbAmt), NM(g, bb), M(g->bbAmt), vi->family == FAM_DRAW ? "before the draw" : "before the flop");
        else
            put(out, n, "Blinds are forced bets, so there's always something to win. The player left of the button (%s) posts the small blind, %s; the next player (%s) posts the big blind, %s. "
                        "Blinds are 'live': they count toward calling.%s", NM(g, sb), M(g->sbAmt), NM(g, bb), M(g->bbAmt),
                fixed ? " In fixed limit the big blind is the same as the small bet." : "");
        return true;
    }
    case EV_ANTES:
        if (!first(mem, g, RC_ANTES)) { put(out, n, "Everyone antes %s.", M(e->a)); return true; }
        put(out, n, "Stud has no blinds. Instead every player puts in an ante of %s before the deal, so the pot starts at %s. Antes are dead money: they don't count toward any bet.",
            M(e->a), M(gamePot(g)));
        return true;
    case EV_DEAL:
        if (!first(mem, g, RC_DEAL)) {
            if (vi->family == FAM_STUD) put(out, n, "Cards: two down, one up.");
            else put(out, n, "Cards: %d each, face down.", e->a);
            return true;
        }
        if (vi->family == FAM_STUD)
            put(out, n, "Each player gets two cards face down and one face up - the 'door card'. Everyone can see the up cards, so in stud you always see part of every hand. "
                        "Later streets bring one card each: 4th, 5th and 6th face up, 7th face down.");
        else if (vi->family == FAM_DRAW)
            put(out, n, "Each player gets %d cards face down. There are no shared cards: after this betting round you can throw cards away and draw new ones (%d draw%s in this game).",
                e->a, vi->draws, vi->draws == 1 ? "" : "s");
        else if (vi->omaha)
            put(out, n, "Each player gets %d cards face down. Remember the Omaha rule: your hand uses EXACTLY two of them with EXACTLY three of the five shared cards%s.",
                e->a, g->variant == V_OMAHA8 ? " - for the high hand and for the low hand separately" : "");
        else
            put(out, n, "Each player gets 2 cards face down%s. Five shared cards will follow (flop, turn, river); your hand is the best five of all seven.",
                g->variant == V_SHORTDECK ? " from a 36-card deck - the 2s to 5s are taken out" : "");
        return true;
    case EV_BRINGIN: {
        char cl[40];
        cardLong(e->a, cl, sizeof cl);
        bool razz = g->variant == V_RAZZ;
        if (!first(mem, g, RC_BRINGIN)) { put(out, n, "%s %s the bring-in (the %s).", NM(g, p), V(g, p, "have", "has"), cl); return true; }
        put(out, n, "%s %s the %s up card, the %s, so %s must bring in: a forced bet of %s, smaller than a full bet. Or %s may complete to a full bet of %s.",
            NM(g, p), V(g, p, "have", "has"), razz ? "highest" : "lowest", cl, p == g->human ? "you" : NM(g, p), M(g->bringIn),
            p == g->human ? "you" : NM(g, p), M(g->smallBet));
        if (razz) add(out, n, " In Razz aces are low, so kings are the worst cards.");
        else add(out, n, " (Aces count high here.)");
        if (e->b) add(out, n, " Another up card has the same rank, so the suit decides: %s.", razz ? "spades are the highest suit, then hearts, diamonds, clubs" : "clubs are the lowest suit, then diamonds, hearts, spades");
        return true;
    }
    case EV_BRINGIN_ACT:
        if (e->a) put(out, n, "%s %s to %s: a full bet instead of the bring-in. Raises are now %s each.", NM(g, p), V(g, p, "complete", "completes"), M(e->b), M(g->betUnit));
        else if (first(mem, g, RC_COMPLETE))
            put(out, n, "%s %s in for %s. The players after %s may fold, call %s, or 'complete' to %s - completing counts as the bet, so the first raise after it goes to %s.",
                NM(g, p), V(g, p, "bring", "brings"), M(e->b), p == g->human ? "you" : NM(g, p), M(e->b), M(g->smallBet), M(2 * g->smallBet));
        else put(out, n, "%s %s in for %s.", NM(g, p), V(g, p, "bring", "brings"), M(e->b));
        return true;
    case EV_STREET: {
        int r = e->a;
        const char *nm = vi->streets[r < vi->rounds ? r : 0];
        if (p < 0) {
            if (first(mem, g, RC_RUNOUT)) put(out, n, "%s: at most one player can still bet - everyone else is all-in. So there's no more betting: the rest of the cards are dealt and the hands are shown.", nm);
            else put(out, n, "%s: no more betting possible.", nm);
            return true;
        }
        bool big = fixed && r == vi->bigFrom;
        if (vi->family == FAM_STUD) {
            if (r == 0) return false;          // the bring-in explains 3rd street
            showingName(g, e->b, sh, sizeof sh);
            if (r == 4) put(out, n, "7th street (the river): the last card comes face DOWN. Everyone has seven cards - three down, four up - and plays the best five. %s %s first.", NM(g, p), V(g, p, "act", "acts"));
            else if (first(mem, g, RC_ORDER))
                put(out, n, "%s: one more card face up for everyone still in. From now on the %s hand showing acts first: %s, with %s. (Straights and flushes don't count until five cards are up.)",
                    nm, g->variant == V_RAZZ ? "lowest" : "best", NM(g, e->b), sh);
            else put(out, n, "%s: %s %s first (%s showing: %s).", nm, NM(g, p), V(g, p, "act", "acts"), g->variant == V_RAZZ ? "lowest" : "best", sh);
            if (p != e->b) add(out, n, " %s is all-in, so the next player starts.", NM(g, e->b));
        } else if (vi->family == FAM_DRAW) {
            if (r == 0) {
                if (!first(mem, g, RC_PREFLOP)) return false;
                put(out, n, "Betting before the draw: the player left of the big blind acts first (%s). Fold, call the big blind, or raise.", NM(g, p));
            } else put(out, n, "Betting after the %s%sdraw: the first player left of the button acts first (%s); the button acts last.", drawWord(r - 1, vi->draws), vi->draws == 1 ? "" : " ", NM(g, p));
        } else {
            if (r == 0) {
                if (!first(mem, g, RC_PREFLOP)) { put(out, n, "Preflop: %s %s first.", NM(g, p), V(g, p, "act", "acts")); return true; }
                put(out, n, "Preflop betting: the player left of the big blind acts first (%s). Everyone may fold, call the big blind, or raise. "
                            "If everyone just calls, the big blind may still raise - 'the option'.", NM(g, p));
            } else if (r == 1) {
                if (first(mem, g, RC_STREET))
                    put(out, n, "The flop: three shared cards. From now on the first player still in, left of the button, acts first (%s) and the button acts last. Nobody has bet yet, so players may check.", NM(g, p));
                else put(out, n, "Flop: %s %s first.", NM(g, p), V(g, p, "act", "acts"));
            } else if (r == 2) put(out, n, "The turn: a fourth shared card. %s %s first.", NM(g, p), V(g, p, "act", "acts"));
            else put(out, n, "The river: the fifth and last shared card, and the final betting round. %s %s first.", NM(g, p), V(g, p, "act", "acts"));
        }
        if (big) {
            if (first(mem, g, RC_BIGBET)) add(out, n, " Fixed limit: from now on every bet and raise is the big bet, %s (twice the small bet).", M(g->bigBet));
            else add(out, n, " Big bets now (%s).", M(g->bigBet));
        }
        return true;
    }
    case EV_OPENPAIR:
        upList(g, p, cs, sizeof cs);
        put(out, n, "%s %s an open pair (%s). In seven-card stud an open pair on 4th street lets any player bet the big bet (%s) instead of the small bet (%s) this round.",
            NM(g, p), V(g, p, "show", "shows"), cs, M(g->bigBet), M(g->smallBet));
        return true;
    case EV_CAP:
        if (first(mem, g, RC_CAP))
            put(out, n, "That's the cap: fixed limit allows one bet and three raises per round. Everyone else can now only call or fold. (With only two players in the hand there's no cap.)");
        else put(out, n, "The betting is capped.");
        return true;
    case EV_ALLIN:
        if (first(mem, g, RC_ALLIN))
            put(out, n, "%s %s all-in. An all-in player can't be pushed out: they stay in to the showdown, but can only win as much from each opponent as they put in. "
                        "Any further betting between the others goes into a side pot.", NM(g, p), V(g, p, "are", "is"));
        else put(out, n, "%s %s all-in.", NM(g, p), V(g, p, "are", "is"));
        return true;
    case EV_SHORTALLIN:
        if (!first(mem, g, RC_SHORTALLIN)) return false;
        put(out, n, "%s all-in was less than a full raise, so it does not reopen the betting: players who have already acted may only call or fold.", POSS(g, p));
        if (out[0] >= 'a' && out[0] <= 'z') out[0] -= 32;
        return true;
    case EV_DRAWS:
        if (first(mem, g, RC_DRAWS))
            put(out, n, "The %s%sdraw: starting left of the button, each player in turn throws away the cards they don't want and gets as many new ones. Keeping all your cards is 'standing pat'. "
                        "Watch how many cards each player takes - it says a lot about their hand.", drawWord(e->a, vi->draws), vi->draws == 1 ? "" : " ");
        else put(out, n, "The %s%sdraw.", drawWord(e->a, vi->draws), vi->draws == 1 ? "" : " ");
        return true;
    case EV_DRAW:
        if (p == g->human) {
            if (e->a == 0) put(out, n, "You stand pat.");
            else put(out, n, "You throw away %d and draw %d new card%s.", e->a, e->a, e->a == 1 ? "" : "s");
        } else if (e->a == 0) put(out, n, "%s stands pat%s", NM(g, p), first(mem, g, RC_PAT) ? " - keeping every card usually means a finished hand (or a bluff)." : ".");
        else if (e->a == 1) put(out, n, "%s draws 1 - probably one card away from a good hand.", NM(g, p));
        else put(out, n, "%s draws %d%s", NM(g, p), e->a, e->a >= 3 ? " - a weak hand, starting over." : ".");
        return true;
    case EV_RESHUFFLE:
        if (first(mem, g, RC_RESHUFFLE))
            put(out, n, "The deck ran out while %s %s drawing. As the rules say, the discards are shuffled into a new deck - except the cards %s just threw away.",
                NM(g, p), V(g, p, "were", "was"), p == g->human ? "you" : NM(g, p));
        else put(out, n, "Deck used up: the discards are shuffled to make a new one.");
        return true;
    case EV_UNCALLED:
        if (first(mem, g, RC_UNCALLED))
            put(out, n, "Nobody matched all of %s bet, so the extra %s goes back to %s: you can only win from each player as much as they can put in.", POSS(g, p), M(e->a), p == g->human ? "you" : NM(g, p));
        else put(out, n, "%s uncalled goes back to %s.", M(e->a), p == g->human ? "you" : NM(g, p));
        return true;
    case EV_WALK:
        if (first(mem, g, RC_WALK))
            put(out, n, "Everyone else folded, so %s %s the pot (%s) without a showdown. The last player left always wins, and doesn't have to show their cards.", NM(g, p), V(g, p, "win", "wins"), M(e->a));
        else put(out, n, "Everyone else folded: %s %s %s.", NM(g, p), V(g, p, "win", "wins"), M(e->a));
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------- your turn

int rulesYourTurn(const Game *g, int p, char out[][RULE_LEN], int max) {
    const VariantInfo *vi = VI(g);
    const Seat *s = &g->s[p];
    Legal l;
    gameLegal(g, &l);
    int k = 0;
    if (g->toAct != p || g->handOver || max <= 0) return 0;
    char hand[120];
    describeHandV(g, p, hand, sizeof hand);
    if (hand[0] && k < max) {
        put(out[k], RULE_LEN, "You hold: %s.", hand);
        if (vi->omaha && g->nboard >= 3) add(out[k], RULE_LEN, " (Exactly two of your cards + three from the board.)");
        if (vi->rank == RK_LOW_27) add(out[k], RULE_LEN, " (Lowest wins; aces are high; straights and flushes count against you.)");
        if (vi->rank == RK_LOW_A5) add(out[k], RULE_LEN, " (Lowest wins; aces are low.)");
        if (vi->rank == RK_BADUGI) add(out[k], RULE_LEN, " (Only cards of different suits and ranks count.)");
        k++;
    }
    if (l.draw) {
        int left = drawsLeft(g, p);
        if (k < max) put(out[k++], RULE_LEN, "Your draw: click the cards you want to throw away (they lift up), then press Draw. Press it with nothing selected to stand pat. "
                                             "You may change any number of cards.");
        if (k < max) put(out[k++], RULE_LEN, left > 1 ? "There are %d draws left including this one." : "This is the last draw: after it comes the final betting round.", left);
        return k;
    }
    if (l.bringIn) {
        if (k < max) put(out[k++], RULE_LEN, "You have the bring-in, so you must start the betting - folding isn't allowed. Bring in for %s (the minimum), or complete to %s (a full bet).",
                         M(l.toCall), M(l.minTo));
        return k;
    }
    char opt[RULE_LEN];
    if (l.canCheck) put(opt, sizeof opt, "Nobody has bet this round, so you may check (bet nothing)%s", l.canRaise ? "" : ".");
    else put(opt, sizeof opt, "You face a bet: fold (give up the hand), call %s%s", M(l.toCall), l.canRaise ? "" : ".");
    if (l.canRaise) {
        const char *verb = g->curBet == 0 ? "bet" : g->curBet < g->betUnit && gameIsFixed(g) ? "complete to" : "raise to";
        if (l.maxTo > l.minTo && gameIsFixed(g)) add(opt, sizeof opt, ", or bet %s or %s.", M(l.minTo), M(l.maxTo));
        else if (l.maxTo > l.minTo) add(opt, sizeof opt, ", or %s anything from %s to %s.", verb, M(l.minTo), M(l.maxTo));
        else add(opt, sizeof opt, ", or %s %s.", verb, M(l.minTo));
    }
    if (k < max) put(out[k++], RULE_LEN, "%s", opt);
    if (k < max) {
        if (gameIsFixed(g)) {
            int left = gameCapLeft(g);
            put(out[k], RULE_LEN, "Fixed limit: every bet and raise on this round is %s (the %s bet).", M(g->betUnit), g->betUnit == g->bigBet ? "big" : "small");
            if (g->openPair && g->curBet == 0) add(out[k], RULE_LEN, " An open pair is showing, so the big bet (%s) is allowed too.", M(g->bigBet));
            if (left < 0) add(out[k], RULE_LEN, " Only two players are in, so there is no cap on raises.");
            else if (left == 0) add(out[k], RULE_LEN, " The betting is capped - no more raises this round.");
            else add(out[k], RULE_LEN, " %d more raise%s allowed before the cap.", g->curBet == 0 ? left - 1 : left, (g->curBet == 0 ? left - 1 : left) == 1 ? " is" : "s are");
        } else if (g->limit == BL_POTLIMIT)
            put(out[k], RULE_LEN, "Pot limit: the most you can %s is the size of the pot after your call (%s here). The least is %s.", g->curBet ? "raise to" : "bet", M(l.maxTo), M(l.minTo));
        else
            put(out[k], RULE_LEN, "No limit: you can %s any amount from %s up to all your chips (%s). A raise must be at least as big as the last bet or raise.", g->curBet ? "raise to" : "bet", M(l.minTo), M(l.maxTo));
        k++;
    }
    if (!l.canRaise && k < max) {
        bool others = false;
        for (int q = 0; q < g->n; q++) if (q != p && g->s[q].inHand && !g->s[q].allin) others = true;
        if (!others) put(out[k++], RULE_LEN, "You can't raise: everyone else is all-in, so there's nobody left to bet against.");
        else if (gameIsFixed(g) && gameCapLeft(g) == 0) put(out[k++], RULE_LEN, "You can't raise: the betting is capped.");
        else if (s->acted) put(out[k++], RULE_LEN, "You can't re-raise: the last all-in was less than a full raise, and you've already acted this round.");
        else if (s->stack + s->bet <= g->curBet) put(out[k++], RULE_LEN, "You can't raise: calling already takes all your chips.");
    }
    if (!l.canCheck && l.toCall >= s->stack && k < max)
        put(out[k++], RULE_LEN, "Calling puts you all-in. You can't be forced out after that, but you can only win as much from each player as you put in.");
    if (k < max) {
        if (vi->family == FAM_STUD) put(out[k++], RULE_LEN, "Order in stud: the bring-in starts 3rd street; after that the best hand showing%s acts first each round.", g->variant == V_RAZZ ? " (lowest, in Razz)" : "");
        else if (vi->family == FAM_DRAW && drawsLeft(g, p) > 0) put(out[k++], RULE_LEN, "Draws still to come: %d.", drawsLeft(g, p));
        else if (vi->family == FAM_BOARD && g->street < ST_RIVER) put(out[k++], RULE_LEN, "Still to come: %s.", g->street == 0 ? "the flop, the turn and the river" : g->street == 1 ? "the turn and the river" : "the river");
    }
    return k;
}

// ---------------------------------------------------------------- showdown

static void names(const Game *g, int mask, char *buf, int n) {
    buf[0] = 0;
    int cnt = __builtin_popcount(mask), i = 0;
    for (int q = 0; q < g->n; q++) {
        if (!(mask >> q & 1)) continue;
        add(buf, n, "%s%s", i == 0 ? "" : i == cnt - 1 ? " and " : ", ", NM(g, q));
        i++;
    }
}

// the best value among the eligible players that is below 'top' (runner-up), or -1
static int runnerUp(const Game *g, int elig, int top, bool low, int *who) {
    int best = -1;
    *who = -1;
    for (int q = 0; q < g->n; q++) {
        if (!(elig >> q & 1)) continue;
        int v = low ? g->s[q].low : g->s[q].value;
        if (v < top && v > best) { best = v; *who = q; }
    }
    return best;
}

int rulesShowdown(const Game *g, char out[][RULE_LEN], int max) {
    const VariantInfo *vi = VI(g);
    int k = 0;
    if (!g->handOver || !g->showdown) return 0;
    bool split = vi->rank == RK_HILO8;
    for (int i = 0; i < g->npots && k < max; i++) {
        const PotResult *pr = &g->pots[i];
        char label[64], who[96], hn[64], cmp[200];
        if (g->npots == 1) snprintf(label, sizeof label, "The pot (%s)", M(pr->amount));
        else if (i == 0) snprintf(label, sizeof label, "Main pot (%s)", M(pr->amount));
        else snprintf(label, sizeof label, "Side pot %d (%s)", i, M(pr->amount));
        char *o = out[k++];
        o[0] = 0;
        if (g->npots > 1) { names(g, pr->eligible, who, sizeof who); add(o, RULE_LEN, "%s - only %s could win it. ", label, who); }
        else add(o, RULE_LEN, "%s: ", label);
        int hw = __builtin_ctz(pr->hiMask);
        handNameV(g->variant, g->s[hw].value, hn, sizeof hn);
        names(g, pr->hiMask, who, sizeof who);
        const char *half = split && pr->loAmt ? "high half" : "";
        if (__builtin_popcount(pr->hiMask) > 1)
            add(o, RULE_LEN, "%s%s%s split between %s, who all have %s.", half, half[0] ? " " : "", half[0] ? "is" : "It's", who, hn);
        else {
            int rw, rv = runnerUp(g, pr->eligible, g->s[hw].value, false, &rw);
            add(o, RULE_LEN, "%s%s%s %s with %s", half[0] ? "the " : "", half, half[0] ? " goes to" : "", NM(g, hw), hn);
            if (!half[0]) add(o, RULE_LEN, " - %s %s", NM(g, hw), V(g, hw, "win", "wins"));
            if (rw >= 0) {
                char rn[64];
                handNameV(g->variant, rv, rn, sizeof rn);
                rulesCompare(g->variant, g->s[hw].value, rv, cmp, sizeof cmp);
                add(o, RULE_LEN, ". Against %s %s: %s.", POSS(g, rw), rn, cmp);
            } else add(o, RULE_LEN, ".");
        }
        if (split) {
            if (!pr->loAmt) add(o, RULE_LEN, " Nobody had a low of five different cards 8 or lower, so the high hand takes the whole pot.");
            else if (k < max) {
                char *lo = out[k++];
                lo[0] = 0;
                int lw = __builtin_ctz(pr->loMask);
                char ln[40];
                lowName(g->s[lw].low, ln, sizeof ln);
                names(g, pr->loMask, who, sizeof who);
                if (__builtin_popcount(pr->loMask) > 1)
                    add(lo, RULE_LEN, "Low half (%s): %s tie with %s and share it - each gets a quarter of this pot or less ('quartered').", M(pr->loAmt), who, ln);
                else {
                    int rw, rv = runnerUp(g, pr->eligible, g->s[lw].low, true, &rw);
                    add(lo, RULE_LEN, "Low half (%s): %s with %s", M(pr->loAmt), NM(g, lw), ln);
                    if (rw >= 0 && rv > 0) {
                        char rn[40];
                        lowName(rv, rn, sizeof rn);
                        rulesCompare(V_RAZZ, g->s[lw].low, rv, cmp, sizeof cmp);
                        add(lo, RULE_LEN, ". Against %s %s: %s.", POSS(g, rw), rn, cmp);
                    } else add(lo, RULE_LEN, " - the only qualifying low.");
                }
                if (pr->hiMask == pr->loMask && __builtin_popcount(pr->hiMask) == 1) add(lo, RULE_LEN, " Best high AND best low: %s %s the whole pot.", NM(g, lw), V(g, lw, "scoop", "scoops"));
                if (pr->amount % 2) add(lo, RULE_LEN, " The pot was odd, so the extra chip went to the high half.");
            }
        }
    }
    if (k < max) {
        switch (vi->rank) {
        case RK_LOW_A5: put(out[k++], RULE_LEN, "Razz reminder: the lowest hand wins. Compare the highest card first, then the next; pairs are bad; straights and flushes don't count."); break;
        case RK_LOW_27: put(out[k++], RULE_LEN, "2-7 reminder: the lowest hand wins, aces are high, and pairs, straights and flushes all count against you."); break;
        case RK_BADUGI: put(out[k++], RULE_LEN, "Badugi reminder: more cards of different suits and ranks win; then the lower highest card."); break;
        default:
            if (vi->family == FAM_STUD) put(out[k++], RULE_LEN, "Each player used the best five of their seven cards.");
            else if (vi->omaha) put(out[k++], RULE_LEN, "Every hand used exactly two hole cards and three board cards.");
            break;
        }
    }
    return k;
}
