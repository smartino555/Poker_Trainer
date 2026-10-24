// sim.c - headless bot-vs-bot simulation: checks chip conservation and compares difficulty levels
// usage: sim [hands] [game] [lvl lvl ...]   (levels 1=easy 2=medium 3=hard, 2-6 seats)
//        game: 0 Hold'em, 1 Omaha, 2 Omaha hi-lo, 3 5-card Omaha, 4 short deck, 5 stud, 6 stud hi-lo, 7 razz,
//              8 5-card draw, 9 2-7 triple draw, 10 2-7 single draw, 11 badugi, h HORSE, e 8-Game
//        SIM_BB=15 sim ...   starts everyone with 15 big blinds instead of 100 (short-stack / push-fold play)
//        SIM_LIMIT=0|1|2     no-limit / pot-limit / fixed limit instead of the game's usual structure
#include "../src/ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv) {
    int hands = argc > 1 ? atoi(argv[1]) : 2000;
    int variant = V_HOLDEM, mix = MIX_NONE;
    if (argc > 2) {
        if (argv[2][0] == 'h') mix = MIX_HORSE;
        else if (argv[2][0] == 'e') mix = MIX_8GAME;
        else variant = atoi(argv[2]);
    }
    if (variant < 0 || variant >= V_COUNT) variant = V_HOLDEM;
    int limit = getenv("SIM_LIMIT") ? atoi(getenv("SIM_LIMIT")) : varInfo(variant)->defLimit;
    GameConfig cfg = { .variant = variant, .limit = limit, .mix = mix,
                       .n = 6, .sb = 1, .bb = 2, .buyin = 200, .human = -1, .seed = (uint64_t)time(NULL) };
    if (getenv("SIM_BB")) cfg.buyin = cfg.bb * atoi(getenv("SIM_BB"));
    int def[6] = { 1, 2, 3, 1, 2, 3 };
    if (argc > 3) {
        cfg.n = argc - 3 > MAXP ? MAXP : argc - 3;
        for (int i = 0; i < cfg.n; i++) cfg.levels[i] = atoi(argv[3 + i]);
    } else for (int i = 0; i < 6; i++) cfg.levels[i] = def[i];
    Game *g = malloc(sizeof *g);
    gameSetup(g, &cfg);
    Rng rng;
    rngSeed(&rng, cfg.seed * 31 + 7);
    long long net[MAXP] = { 0 };
    int showdowns = 0, acts = 0;
    clock_t t0 = clock();
    for (int h = 0; h < hands; h++) {
        int before[MAXP], chipsBefore = 0;
        gameNewHand(g);
        for (int i = 0; i < g->n; i++) { before[i] = g->s[i].stack + g->s[i].total; chipsBefore += before[i]; }
        int guard = 0;
        while (!g->handOver) {
            if (++guard > 500) { printf("stuck in hand %d\n", g->handNo); for (int i = 0; i < g->nlog; i++) printf("  %s\n", g->log[i]); return 1; }
            if (g->toAct >= 0) { gameAct(g, aiDecide(g, g->toAct, &rng)); acts++; }
            else gameAdvance(g);
        }
        int chipsAfter = 0;
        for (int i = 0; i < g->n; i++) {
            if (g->s[i].stack < 0) { printf("negative stack seat %d hand %d\n", i, g->handNo); return 1; }
            chipsAfter += g->s[i].stack;
            net[i] += g->s[i].stack - before[i];
        }
        if (chipsAfter != chipsBefore) {
            printf("chip leak in hand %d: %d -> %d\n", g->handNo, chipsBefore, chipsAfter);
            for (int i = 0; i < g->nlog; i++) printf("  %s\n", g->log[i]);
            return 1;
        }
        showdowns += g->showdown;
    }
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("%s %s, %d hands, %d seats, %.1fs (%.1f ms/hand), %d%% showdowns, %.1f actions/hand\n",
           mix ? "" : limitName(limit), mix ? mixName(mix) : varInfo(variant)->name, hands, g->n, secs, 1000 * secs / hands, 100 * showdowns / hands, (double)acts / hands);
    double byLv[4] = { 0 };
    int cnt[4] = { 0 };
    for (int i = 0; i < g->n; i++) {
        double bb100 = net[i] / (double)g->bbAmt / hands * 100;
        Stats *s = &g->s[i].st;
        printf("  seat %d %-7s %-9s %+8.1f bb/100   VPIP %2.0f%% PFR %2.0f%% (last stack)\n", i, levelName(g->s[i].level), g->s[i].name, bb100,
               s->hands ? 100.0 * s->vpip / s->hands : 0, s->hands ? 100.0 * s->pfr / s->hands : 0);
        byLv[g->s[i].level] += bb100;
        cnt[g->s[i].level]++;
    }
    for (int l = 1; l <= 3; l++) if (cnt[l]) printf("  %-7s average %+8.1f bb/100\n", levelName(l), byLv[l] / cnt[l]);
    free(g);
    return 0;
}
