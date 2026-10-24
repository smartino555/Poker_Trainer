# Poker Trainer (raylib / C)

Texas Hold'em and Omaha against 1–5 bots. You can just **Play**, or pick **Training**, where a coach analyses every decision you make, grades it and explains the reasoning.

## Build & run (macOS)

```sh
brew install raylib          # one time
cd C_Games/Poker_Trainer
make
./poker
```

The game needs raylib 5.5 or newer. It looks for the `assets/` folder next to the executable.

## Web version

The same code compiles to WebAssembly with [Emscripten](https://emscripten.org):

```sh
brew install emscripten      # one time
make web                     # first run clones raylib 6.2 into build/raylib and builds it for the web
make serve-web               # play it at http://localhost:8000
make install-web WEB_DEST=path/to/site/public/play/poker   # copy the four files into a website
```

The build is four static files in `build/web/`: `index.html`, `index.js`, `index.wasm` and `index.data` (the fonts), about 570 KB together. No server code is needed: any static host works. Serve `.wasm` as `application/wasm`. The files must be loaded over http(s), not opened from disk.

**Adding it to a website.** Keep the four files together in one folder. Either link to that folder's `index.html`, or embed it in a page:

```html
<iframe src="/play/poker/index.html" title="Poker Trainer"
        style="width:100%; aspect-ratio:16/9; border:0" allow="fullscreen"></iframe>
```

A ready-built copy of these four files is committed in `build/web/`, so the site can use them without installing Emscripten.

The game scales to whatever size it is given; the table is laid out for 16:9 and is letterboxed otherwise. It's best at 960×540 or larger, and it's built for a mouse and keyboard, not phones. The page template is `web/shell.html` (loading bar, error message, keyboard focus, and it stops the arrow keys from scrolling the host page). In the browser the game works exactly like the desktop version, including every keyboard shortcut. Nothing is saved between visits. If a file fails to download or the browser lacks WebGL, the page shows an error message instead of an endless loading bar.

Two extra test targets cover the web build:
- `make test-wasm` runs the whole test suite compiled to WebAssembly under Node, with the web build's 1 MB stack and stack-overflow checks on (wasm is a 32-bit target, so this also catches integer-size differences).
- `make test-web` (needs Node; installs Playwright and Chromium into `tests/` on first use) plays the real page in headless Chromium: it loads at several sizes and pixel ratios with a sharp full-size canvas; starts a game with the mouse; plays 40 hands of Omaha against Hard bots with the keyboard, with no errors and no frame over 1 s; checks that typing, including spaces, reaches the glossary search; checks that arrow keys don't scroll a host page; checks that separate visits deal different cards; and checks that a missing file shows an error.

`make sim` builds a headless bot-vs-bot simulator: `./sim 20000 0` runs 20,000 Hold'em hands (`1` = Omaha) with two bots of each level. `./sim 10000 0 3 2 2 2 2 2` puts one Hard bot against five Mediums, and `SIM_BB=15 ./sim ...` starts everyone with 15 big blinds. It checks that no chips are created or lost and prints each level's win rate in bb/100.

`make nash` re-solves the short-stack push/fold ranges (`tools/nash.c`, about 20 s) and rewrites `src/pushfold.c`.

`make test` runs the test suite (`tests/test_main.c`, about 6,000 checks, ~10 s):
- **Hand evaluator:** exact category counts over all 2,598,960 five-card hands, plus edge cases (wheel, steel wheel, kickers, Omaha's 2+3 rule).
- **Range parser and charts:** combo counts, and no hand listed as both a raise and a call.
- **Betting rules on rigged decks:** blinds and action order at every table size, heads-up order, BB option, min-raises, short all-ins that don't reopen betting, pot-limit maximums, side pots, split pots and odd chips. A bet nobody can match is returned as soon as the betting round ends, not at showdown.
- **Random-action fuzzing:** about 45,000 hands in No-Limit and Pot-Limit for both games, checking that no chips are created or lost, no invalid states appear, and no uncallable chips sit in the pot between streets.
- **Equity:** checked against known matchups (AA vs KK, AA vs a random hand, ...).
- **Outs and nut detection.**
- **Coach:** scenario tests, plus well-formed advice in every spot the bots create (no explanation cut off, a valid quiz checklist for every decision). Pot odds only count what a call can actually win: a short stack facing a bigger all-in needs ~47%, not ~4%.
- **Push/fold solver:** the solved ranges match published Nash charts (small blind ~58% at 10 BB), and the coach follows them exactly; short all-ins, re-steals and value bets judged against the calling range.
- **Glossary:** every jargon word the coach uses has an entry, every tooltip term exists, and term detection matches whole words only.
- **Bots:** they never send an illegal action.
- **Card arranging:** drag, click-swap and sort.

## Table setup (menu)

| Setting | Options |
|---|---|
| Game | Texas Hold'em or Omaha (4 cards, use exactly 2) |
| Betting | No-Limit or Pot-Limit (Omaha defaults to Pot-Limit) |
| Mode | Play, or Training (coach) |
| Opponents | 1 (heads-up) to 5 (6-max) |
| Bot skill | Easy / Medium / Hard per seat (click a bot to cycle, or set all) |
| Blinds | $1/$2 up to $250/$500 |
| Buy-in | 20–300 big blinds (bots buy in for the same amount and reload when they go broke) |
| Other | game speed, four-color deck, auto-deal |

## Bot difficulty

| Level | Behaviour |
|---|---|
| Easy | Loose-passive calling station: plays about half its hands, limps, calls with any pair or draw and rarely raises. |
| Medium | Tight and straightforward, but ignores position, and judges its hand against *random* cards rather than the hands its opponents actually show, so it pays off strong bets. |
| Hard | Plays the coach's strategy (position charts, range-based equity, sized bets, mixed bluffs) and adjusts to opponents: it stops bluffing calling stations and value-bets them thinner. |

In simulation, Hold'em (30k hands, 2 bots of each level): Hard about +186 bb/100, Medium −51, Easy −135. Hard vs five Mediums: about +295 bb/100. Starting at 15 BB: Hard +87. Omaha (15k hands): Hard +354, Medium −21, Easy −333.

## Training mode

- **Coach panel.** For each decision it shows the situation (e.g. "Facing a 2.5 BB open from CO"), your hand and draws (outs and the exact chance to hit), pot odds (the equity you need), your equity against the opponents' estimated ranges, and the stack-to-pot ratio. Then it shows its recommended play, a grade for every option, and the reasons.
- **Hints on / Quiz mode.** With hints on, the recommendation is shown before you act, the best button is outlined and a green tick on the slider marks the suggested size. In quiz mode the recommendation stays hidden until you act.
- **Grades.** Each play is graded Best / Good / Inaccuracy / Mistake / Blunder. Good bet sizing matters: the right action at a poor size is an Inaccuracy.
- **Session score and leaks.** The panel keeps your hit rate and your two biggest leaks (e.g. "Calling without the odds", "Missing value bets").
- **Hand review.** After each hand you see your decisions with grades, and every bot's cards, including the ones that folded.
- **HUD.** VPIP / PFR stats appear under each bot. The coach uses them too, so a bot that plays loose gets a wider estimated range.
- **Charts (G).** 13×13 preflop charts: opening ranges by position, facing an open (3-bet / call), blind defense and facing a 3-bet. Your current hand is outlined in gold.
- **Think it through (quiz mode).** Instead of the answer, the coach panel lists the questions a strong player asks at this decision, filled in with the facts: your position and who acts behind you, the stack depth, what each opponent's betting line was, the board texture and who it favours, the price, and your plan for the next card.
- **Glossary (T).** About 150 terms in nine categories (basics, positions, preflop, betting after the flop, hands & boards, math & odds, strategy & theory, players & the table, this trainer), searchable as you type.
- **Hover to learn.** Hover a stat box, a seat's position, a bot's VPIP / PFR line, a grade or the "mix" label for its definition. Under the coach's reasons, a TERMS row lists the jargon it used: hover for the definition, click to open the glossary there.
- **Lessons (L).** Eleven lessons. Basics: position, raise-or-fold (with push/fold and re-steals), pot odds & equity, value betting & sizing, bluffing & MDF, Omaha. At a real table: thinking through a hand step by step, ranges and combos, reading real players (player types, bet-sizing patterns, live tells), bankroll, variance & tilt, and how to keep improving (study routine, and live-casino adjustments and etiquette).

### How "optimal" the coach is

Real game-theory-optimal play requires solver output for every spot. One part of the coach is actually solved; the rest follows solver-inspired rules:

- **Short stacks (solved):** folded to you with ≤ 12 BB, the coach uses Nash-equilibrium push/fold ranges for each seat, computed by `tools/nash.c` (fictitious play over all 169 hands, Monte Carlo equities with exact card removal, no antes). They match published charts: at 10 BB, UTG shoves ~14%, the button ~32% and the small blind ~59%. When a short stack shoves, the coach reads it as that solved range.
- **Re-steals (≤ 20 BB):** facing an open, the coach works out the expected value of shoving from how often the opener folds and your equity when called (the calling range depends on the price they get) and recommends all-in or fold. Only the big blind may call instead.
- **Calling all-ins:** a pure equity decision against the players who are actually in. Players still to act are assumed to fold.
- **Preflop at ~100 BB:** 6-max charts for opens, 3-bets, calls, 4-bets and blind defense, with wider heads-up defense against 3-bets. Omaha uses a starting-hand score (suitedness, connectedness, pairs, danglers) mapped to position thresholds.
- **Postflop:** Monte Carlo equity against ranges narrowed by each opponent's position, preflop action, bets/calls/raises on each street and HUD stats. Value bets and value raises are judged against the hands that would *call* (or continue against a raise), not against the opponent's whole range. That is the real test of a value bet, and it stops the coach from betting medium hands that only get called by better. On top of that sit rules for semi-bluffs, c-bets, turn barrels, small stabs when checked to, balanced river bluffing (bet / (pot + 2·bet) of the betting range) and MDF, plus board-texture-aware sizing (⅓ pot dry, ⅔–¾ wet).

Not solved: postflop play, Omaha preflop and anything with antes or ICM (tournaments). For those spots the coach is a strong, consistent baseline for learning the fundamentals, but not a solver.

## Controls

Sounds: a card flick for every card dealt, a cheer when you win a pot, and a sad trombone when you lose at showdown (not when you fold). M turns sound on and off. In the browser, sound starts after your first click or key press.


Rearrange your hole cards: drag a card to a new spot, or click two cards to swap them. **Sort** orders them by rank or by suit, and that choice carries over to later hands.

F fold · C/K check or call · R or Enter bet/raise · ↑/↓ or mouse wheel adjust size (or drag the slider, or use the presets) · Space next hand · Esc menu · H help · T glossary · L lessons · G charts · M sound on/off

## Code layout

| File | Purpose |
|---|---|
| `src/cards.c` | RNG and a bitmask hand evaluator (5–7 cards, Omaha 2+3) |
| `src/range.c` | 169 Hold'em hand classes, range strings ("22+,A2s+,KTo+"), preflop strength tables, Omaha hand score |
| `src/equity.c` | Range-weighted Monte Carlo equity, opponent hand tiers, outs, nut check |
| `src/game.c` | Dealing, blinds, betting rules (NL / PL, min-raise, short all-ins), side pots, showdown |
| `src/strategy.c` | The coach: charts, range estimates, recommendations, grades and explanations |
| `src/ai.c` | Easy / Medium / Hard bots |
| `src/pushfold.c` | Solved push/fold ranges (generated by `tools/nash.c`) |
| `src/glossary.c` | The glossary: terms, definitions, search and term detection in coach text |
| `src/arrange.c` | Your card order: drag, click-swap and sort logic (no raylib, unit-tested) |
| `src/sound.c` | Sound effects (card deal, win cheer, losing trombone), synthesized at startup, so there are no audio files |
| `src/main.c` | raylib front end (desktop and web) |
| `web/shell.html` | Page template for the web build (`make web`) |
| `tools/sim.c` | Headless simulator |
| `tools/nash.c` | Push/fold equilibrium solver |
