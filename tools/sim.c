// sim.c - headless bot-vs-bot simulation: checks chip conservation and compares difficulty levels
// usage: sim [hands] [game 0=holdem 1=omaha] [lvl lvl ...]   (levels 1=easy 2=medium 3=hard, 2-6 seats)
//        SIM_BB=15 sim ...   starts everyone with 15 big blinds instead of 100 (short-stack / push-fold play)
#include "../src/ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv) {
    int hands = argc > 1 ? atoi(argv[1]) : 2000;
    int variant = argc > 2 ? atoi(argv[2]) : V_HOLDEM;
    GameConfig cfg = { .variant = variant, .limit = variant == V_OMAHA ? BL_POTLIMIT : BL_NOLIMIT,
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
            if (++guard > 500) { printf("stuck in hand %d\n", g->handNo); return 1; }
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
    printf("%s, %d hands, %d seats, %.1fs (%.1f ms/hand), %d%% showdowns, %.1f actions/hand\n",
           variant == V_OMAHA ? "Omaha" : "Hold'em", hands, g->n, secs, 1000 * secs / hands, 100 * showdowns / hands, (double)acts / hands);
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
