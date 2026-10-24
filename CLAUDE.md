# CLAUDE.md

Poker Trainer: poker against 1-5 bots in C + raylib, with a coach. Desktop build and a WebAssembly build. `README.md` is the user-facing documentation; this file is for working on the code.

## Commands

```sh
make            # build ./poker (raylib via pkg-config or Homebrew)
make run
make test       # unit, scenario and fuzz tests (tests/test_main.c), ~6,300 checks, must stay at 0 failed
make sim        # headless bot-vs-bot simulator: ./sim <hands> <game> [levels...]
                #   game: 0-11 (see enum in src/variants.h), h = HORSE, e = 8-Game
                #   SIM_LIMIT=0|1|2 forces NL / PL / fixed; SIM_BB=15 short stacks
make nash       # re-solve push/fold ranges (rewrites src/pushfold.c, ~20 s)
make web        # WebAssembly build into build/web (Emscripten); build/web is committed
make test-wasm  # test suite under node with the web stack size
make test-web   # Playwright smoke test of the web build (clicks menu coordinates!)
```

Launching the raylib window from Claude Code needs the sandbox disabled; `open ./poker` works.

## Architecture

Everything except `src/main.c` and `src/sound.c` is plain C with no raylib, so it is unit-tested.

| File | Role |
|---|---|
| `src/variants.c/.h` | The game table (`VariantInfo`): family (board / stud / draw), hole cards, deck size, ranking rule, draws, betting rounds, street names, allowed betting structures. Mixed rotations (HORSE, 8-Game). `V_HOLDEM = 0`, `V_OMAHA = 1` must keep their values. |
| `src/cards.c/.h` | RNG and evaluators: high, short deck, Omaha (2+3, 4 or 5 hole cards), ace-to-five low, 8-or-better low, deuce-to-seven, badugi. `evalCards()` handles every game. |
| `src/game.c/.h` | The engine: dealing, blinds/antes, stud bring-in and acting order, draws (with discard reshuffle), NL / PL / fixed-limit betting with the cap, side pots, hi-lo splits, mixed rotation. Records `GameEvent`s for the rules coach. |
| `src/strategy.c` | The graded coach (Hold'em and Omaha high only): charts, ranges, grades, explanations. The Hard bot plays its advice. |
| `src/equity.c`, `src/range.c`, `src/pushfold.c` | Hold'em/Omaha equity, ranges, solved push/fold tables. |
| `src/basic.c/.h` | Every other game: Monte Carlo equity (`basicEquity`), draw heuristics, simple advice (`adviseBasic`, no grades) and the bots for those games. |
| `src/rules.c/.h` | The rules coach: rule sheets per game, plain-English narration of engine events, "what you may do now", showdown explanations. |
| `src/ai.c` | Bot dispatch: `aiClassic()` games (Hold'em, Omaha) use the classic Easy/Medium/Hard bots; all others go to `basicBot()`. |
| `src/glossary.c` | Glossary terms; tests check every jargon word the coaches use has an entry. ASCII only (the UI font has no other glyphs). |
| `src/arrange.c` | Your card order (drag, click-swap, sort). |
| `src/main.c` | raylib UI: menu, table, controls, coach panel, overlays, AUTOTEST screenshot mode. |

### Engine conventions

- Betting rounds: board games use `ST_PREFLOP..ST_RIVER` (0-3); stud uses 0-4 for 3rd-7th street; draw games number rounds 0..draws. `ST_SHOWDOWN` is a sentinel (8). Use `varInfo(g->variant)->rounds`, not `ST_RIVER`, in code that isn't Hold'em/Omaha-only.
- Draw games alternate `PH_BET` and `PH_DRAW`; in the draw phase the actor sends `A_DRAW` with a bit mask of cards to throw away. `gameAdvance()` is called whenever `toAct == -1`.
- Fixed limit: `gameLegal()` returns `minTo == maxTo` (except the 4th-street open-pair choice); the cap is a bet + 3 raises (`nBets`), off when the round starts heads-up. Short all-ins follow the full-bet rule.
- Hand values compare as ints, higher is better in every game (low evaluators are inverted). Short-deck values carry their ranking order in bits 24+; `handCat()` masks it off.
- No burn cards (deliberate: they would break the rigged-deck tests and don't change the game).
- Tests rig decks with `rig()` (Hold'em/Omaha) or `g->preset` / `presetDeck()` (cards in dealing order) for stud and draw games.

## Conventions

- Match the existing style: dense C, short comments only where the rule isn't obvious, `snprintf` into fixed buffers (tests check coach text is never cut off).
- Rules must follow Wikipedia's articles on each game; note any house rule in the rule sheet.
- Bots must never send an illegal action (tested). New games or structures need a fuzz entry (`testFuzzAll`) and `make sim` should show Hard > Medium > Easy.
- After changing the UI, rebuild `build/web` with `make web` (it is committed) and update `tests/web_smoke.mjs` if menu coordinates move.

## Work in progress (Oct 2026)

Adding every game with a rules coach (plan: `~/.claude/plans/synchronous-watching-wind.md`). Engine, evaluators, bots, simulator and tests are done. `src/rules.c` is written but not yet built: add it to `ENGINE` in the Makefile. Still to do: glossary terms for the rules text, the UI in `src/main.c` (game picker, stud/draw tables, fixed-limit controls, Rules tab and "Learn the rules" mode), lessons, web build, README.
