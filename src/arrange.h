// arrange.h - the player's own card order (display only) and the drag / click-swap / sort logic
#pragma once
#include <stdbool.h>

enum { SORT_NONE = -1, SORT_RANK, SORT_SUIT };
#define ARR_MAX 7        // most cards a player holds (stud)

typedef struct {
    int order[ARR_MAX];  // order[slot] = index into the seat's hole cards
    int n;
    bool markMode;       // draw phase: a click marks a card to throw away instead of selecting it for a swap
    int marks;           // marked cards, bit per hole index (the engine's discard mask)
    int press;           // slot pressed with the mouse (-1 none)
    int sel;             // slot picked for a click-swap (-1 none)
    bool dragging;
    float pressX, pressY;
} Arrange;

void arrReset(Arrange *a, int n);
void arrMove(Arrange *a, int from, int to);    // take the card out of 'from' and insert it at 'to'
void arrSwap(Arrange *a, int s1, int s2);
void arrSort(Arrange *a, const int *hole, int mode);   // SORT_RANK: high to low; SORT_SUIT: grouped by suit, high to low
// One frame of mouse input. In markMode a click toggles the card's mark; dragging still moves it. under = slot under the pointer (-1 none), target = slot a drag would drop
// into (only used while dragging). Returns true when the order changed.
bool arrInput(Arrange *a, int under, bool pressed, bool down, float mx, float my, int target);
