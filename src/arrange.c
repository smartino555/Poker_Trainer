// arrange.c - reordering the player's hole cards
#include "arrange.h"
#include "cards.h"
#include <math.h>

#define DRAG_START 6.0f     // pointer travel before a press becomes a drag

void arrReset(Arrange *a, int n) {
    a->n = n < 1 ? 1 : n > ARR_MAX ? ARR_MAX : n;
    for (int k = 0; k < ARR_MAX; k++) a->order[k] = k;
    a->press = a->sel = -1;
    a->dragging = a->markMode = false;
    a->marks = 0;
}

void arrMove(Arrange *a, int from, int to) {
    if (from < 0 || to < 0 || from >= a->n || to >= a->n || from == to) return;
    int v = a->order[from];
    if (from < to) for (int i = from; i < to; i++) a->order[i] = a->order[i + 1];
    else for (int i = from; i > to; i--) a->order[i] = a->order[i - 1];
    a->order[to] = v;
}

void arrSwap(Arrange *a, int s1, int s2) {
    if (s1 < 0 || s2 < 0 || s1 >= a->n || s2 >= a->n) return;
    int t = a->order[s1];
    a->order[s1] = a->order[s2];
    a->order[s2] = t;
}

static int sortKey(int c, int mode) {
    return mode == SORT_SUIT ? cardSuit(c) * 16 + cardRank(c) : cardRank(c) * 4 + cardSuit(c);
}

void arrSort(Arrange *a, const int *hole, int mode) {
    if (mode == SORT_NONE) return;
    for (int i = 1; i < a->n; i++)
        for (int j = i; j > 0 && sortKey(hole[a->order[j]], mode) > sortKey(hole[a->order[j - 1]], mode); j--) {
            int t = a->order[j]; a->order[j] = a->order[j - 1]; a->order[j - 1] = t;
        }
    a->sel = -1;
}

bool arrInput(Arrange *a, int under, bool pressed, bool down, float mx, float my, int target) {
    bool changed = false;
    if (pressed && under >= 0) {
        a->press = under;
        a->pressX = mx;
        a->pressY = my;
    }
    if (a->press >= 0 && down && !a->dragging && fabsf(mx - a->pressX) + fabsf(my - a->pressY) > DRAG_START) {
        a->dragging = true;
        a->sel = -1;
    }
    if (a->press >= 0 && !down) {                       // released
        if (a->dragging) {
            if (target >= 0 && target != a->press) { arrMove(a, a->press, target); changed = true; }
        } else if (a->press == under) {                  // a click
            if (a->markMode) a->marks ^= 1 << a->order[under];
            else if (a->sel < 0) a->sel = under;
            else if (a->sel == under) a->sel = -1;
            else { arrSwap(a, a->sel, under); a->sel = -1; changed = true; }
        }
        a->press = -1;
        a->dragging = false;
    }
    return changed;
}
