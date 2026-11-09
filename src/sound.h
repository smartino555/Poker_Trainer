// sound.h - sound effects, synthesized at startup (no audio files)
#pragma once
#include <stdbool.h>

void sndInit(void);              // after InitWindow; silently does nothing if there is no audio device
void sndClose(void);
void sndUpdate(float dt);        // once per frame: plays queued deal sounds
void sndDeal(int cards);         // soft card flicks for a deal (one per card, at most a few), unevenly spaced
void sndWin(void);               // cheer
void sndLose(void);              // sad trombone
void sndSetMuted(bool muted);
bool sndMuted(void);
