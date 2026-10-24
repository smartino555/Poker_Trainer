// ai.h - bot players (Easy / Medium / Hard)
#pragma once
#include "strategy.h"

Action aiDecide(const Game *g, int p, Rng *rng);
const char *levelName(int lv);
bool aiClassic(int variant);    // Hold'em / Omaha: the full coach and its bots (other games use basic.c)
