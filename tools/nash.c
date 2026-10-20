// nash.c - solves short-stack push/fold poker (no antes, chip EV) and writes src/pushfold.c
//
// Spot: everyone folds to a player with S big blinds (effective). They move all-in or fold; each player
// behind calls or folds. Over-calls (a second caller) are rare and ignored. Hand-vs-hand equities come
// from Monte Carlo over the 169 starting-hand classes, with exact card-removal weights.
// The equilibrium is found by fictitious play: each player repeatedly best-responds to the others'
// average strategy.
//
// usage: nash [boards per matchup] > src/pushfold.c       (make nash)
#include "../src/range.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NPOS 5                // pusher: UTG, HJ, CO, BTN, SB (BB never pushes first)
#define NDEPTH 48             // S = 1.5, 2.0 .. 25.0 BB
#define ITERS 400

static float E[NCLS][NCLS];   // equity of class i against class j
static float W[NCLS][NCLS];   // average number of j combos compatible with one combo of i

static int combos[1326][2], ncombo;

static void buildEquity(int boards, Rng *rng) {
    int list[NCLS][12], cnt[NCLS] = { 0 };
    for (int a = 0; a < 52; a++)
        for (int b = a + 1; b < 52; b++) {
            int k = hcClass(a, b);
            list[k][cnt[k]++] = ncombo;
            combos[ncombo][0] = a; combos[ncombo][1] = b; ncombo++;
        }
    for (int i = 0; i < NCLS; i++)
        for (int j = 0; j < NCLS; j++) {
            int ok = 0;
            for (int x = 0; x < cnt[i]; x++)
                for (int y = 0; y < cnt[j]; y++) {
                    int *p = combos[list[i][x]], *q = combos[list[j][y]];
                    ok += p[0] != q[0] && p[0] != q[1] && p[1] != q[0] && p[1] != q[1];
                }
            W[i][j] = (float)ok / cnt[i];
        }
    for (int i = 0; i < NCLS; i++)
        for (int j = i; j < NCLS; j++) {
            if (W[i][j] == 0) { E[i][j] = E[j][i] = 0.5f; continue; }
            double win = 0;
            for (int t = 0; t < boards; t++) {
                int *p, *q;
                do {
                    p = combos[list[i][rngInt(rng, cnt[i])]];
                    q = combos[list[j][rngInt(rng, cnt[j])]];
                } while (p[0] == q[0] || p[0] == q[1] || p[1] == q[0] || p[1] == q[1]);
                bool used[52] = { 0 };
                used[p[0]] = used[p[1]] = used[q[0]] = used[q[1]] = true;
                int h1[7] = { p[0], p[1] }, h2[7] = { q[0], q[1] };
                for (int k = 0; k < 5; k++) {
                    int c;
                    do c = rngInt(rng, 52); while (used[c]);
                    used[c] = true;
                    h1[2 + k] = h2[2 + k] = c;
                }
                int v1 = evalHand(h1, 7), v2 = evalHand(h2, 7);
                win += v1 > v2 ? 1 : v1 == v2 ? 0.5 : 0;
            }
            E[i][j] = (float)(win / boards);
            E[j][i] = 1 - E[i][j];
        }
}

// blinds posted by each position (UTG..BB)
static const float BLIND[6] = { 0, 0, 0, 0, 0.5f, 1.0f };

// solves one pusher position at one depth; push[] receives the pusher's strategy (fraction per class)
static void solve(int pos, float S, float *push) {
    float call[6][NCLS];
    for (int h = 0; h < NCLS; h++) push[h] = 0.5f;
    for (int j = 0; j < 6; j++) for (int h = 0; h < NCLS; h++) call[j][h] = 0.3f;
    for (int it = 0; it < ITERS; it++) {
        float step = 1.0f / (it + 2);
        float brPush[NCLS], brCall[6][NCLS];
        // callers: equity of each hand against the push range, versus the price
        for (int j = pos + 1; j < 6; j++) {
            float pot = 2 * S + 1.5f - BLIND[pos] - BLIND[j];
            for (int c = 0; c < NCLS; c++) {
                float wsum = 0, esum = 0;
                for (int h = 0; h < NCLS; h++) { float w = push[h] * W[c][h]; wsum += w; esum += w * E[c][h]; }
                float eq = wsum > 0 ? esum / wsum : 0;
                brCall[j][c] = eq * pot - S > -BLIND[j] ? 1.0f : 0.0f;
            }
        }
        // pusher: each player behind gets a chance to call, in order
        for (int h = 0; h < NCLS; h++) {
            float reach = 1, ev = 0;
            for (int j = pos + 1; j < 6; j++) {
                float wsum = 0, csum = 0, esum = 0;
                for (int c = 0; c < NCLS; c++) {
                    float w = W[h][c];
                    wsum += w;
                    csum += w * call[j][c];
                    esum += w * call[j][c] * E[h][c];
                }
                float pc = wsum > 0 ? csum / wsum : 0, eq = csum > 0 ? esum / csum : 0;
                float pot = 2 * S + 1.5f - BLIND[pos] - BLIND[j];
                ev += reach * pc * (eq * pot - S);
                reach *= 1 - pc;
            }
            ev += reach * (1.5f - BLIND[pos]);
            brPush[h] = ev > -BLIND[pos] ? 1.0f : 0.0f;
        }
        for (int h = 0; h < NCLS; h++) {
            push[h] += step * (brPush[h] - push[h]);
            for (int j = pos + 1; j < 6; j++) call[j][h] += step * (brCall[j][h] - call[j][h]);
        }
    }
}

static float share(const float *strat) {
    float n = 0;
    for (int h = 0; h < NCLS; h++) n += strat[h] * hcCombos(h);
    return n / 1326;
}

int main(int argc, char **argv) {
    int boards = argc > 1 ? atoi(argv[1]) : 3000;
    Rng rng;
    rngSeed(&rng, 12345);
    buildEquity(boards, &rng);
    static unsigned char maxDepth[NPOS][NCLS];       // in half big blinds; 0 = never push
    static const char *PN[NPOS] = { "UTG", "HJ", "CO", "BTN", "SB" };
    fprintf(stderr, "push range (%% of hands) at 5 / 10 / 15 / 20 BB\n");
    for (int pos = 0; pos < NPOS; pos++) {
        for (int d = 0; d < NDEPTH; d++) {
            float S = 1.5f + 0.5f * d, push[NCLS];
            solve(pos, S, push);
            for (int h = 0; h < NCLS; h++) if (push[h] >= 0.5f) maxDepth[pos][h] = (unsigned char)(3 + d);
            if (d == 7 || d == 17 || d == 27 || d == 37) fprintf(stderr, "%s %4.1f BB: %5.1f%%%s", PN[pos], S, share(push) * 100, d == 37 ? "\n" : "   ");
        }
    }
    printf("// pushfold.c - GENERATED by tools/nash.c (make nash): Nash equilibrium push/fold ranges.\n");
    printf("// Folded to you with S big blinds (no antes): move all-in with a hand when S <= its entry / 2.\n");
    printf("// 0 = never. 50 = 25 BB or more.\n");
    printf("#include \"pushfold.h\"\n\nconst unsigned char PUSH_HALF_BB[5][169] = {\n");
    for (int pos = 0; pos < NPOS; pos++) {
        printf("    { // %s\n     ", PN[pos]);
        for (int h = 0; h < NCLS; h++) printf(" %2d,%s", maxDepth[pos][h], h % 13 == 12 ? (h == NCLS - 1 ? "\n" : "\n     ") : "");
        printf("    },\n");
    }
    printf("};\n");
    return 0;
}
