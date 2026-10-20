// ai.h - bot players (Easy / Medium / Hard)
#pragma once
#include "strategy.h"

Action aiDecide(const Game *g, int p, Rng *rng);
const char *levelName(int lv);
