# Poker Trainer (raylib / C)

Poker against 1–5 bots in every common form of the game: 12 games and 2 mixed rotations. There are three modes. **Play** is just you and the bots. **Learn the rules** has a rules coach that explains every step of the hand as it happens. **Training** adds a strategy coach for every decision: graded in Hold'em and Omaha, tips in the other games.

## The games

Rules follow Wikipedia's article on each game. The rule sheet for any game is under **Rules** (H) in the app.

| Menu tab | Game | Betting (default first) |
|---|---|---|
| Board | Texas Hold'em | No-Limit, Pot-Limit, Fixed |
| Board | Omaha (4 cards, use exactly 2) | Pot-Limit, No-Limit, Fixed |
| Board | Omaha Hi-Lo (8 or better) | Fixed, Pot-Limit |
| Board | 5-Card Omaha ("Big O") | Pot-Limit, No-Limit |
| Board | Short Deck (6+): 36 cards, a flush beats a full house | No-Limit, Pot-Limit, Fixed |
| Stud | Seven-Card Stud | Fixed |
| Stud | Stud Hi-Lo (8 or better) | Fixed |
| Razz | Razz: seven-card stud for the lowest hand (ace-to-five) | Fixed |
| Draw | Five-Card Draw | Fixed, Pot-Limit, No-Limit |
| Draw | 2-7 Triple Draw | Fixed, Pot-Limit |
| Draw | 2-7 Single Draw | No-Limit, Pot-Limit, Fixed |
| Draw | Badugi | Fixed, Pot-Limit |
| Mixed | HORSE: limit Hold'em, Omaha Hi-Lo, Razz, Stud, Stud Hi-Lo | each game's own |
| Mixed | 8-Game: 2-7 Triple Draw, limit Hold'em, Omaha Hi-Lo, Razz, Stud, Stud Hi-Lo, NL Hold'em, PLO | each game's own |

Razz has its own tab in the menu, but underneath it is a stud game. Mixed games play one orbit of each game; the table title and the top bar show the game now and the next one. One house rule applies: no cards are burned before each street or draw. Burning doesn't change the odds, and every rule sheet notes it.

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

The build is four static files in `build/web/`: `index.html`, `index.js`, `index.wasm` and `index.data` (the fonts), about 770 KB together. No server code is needed: any static host works. Serve `.wasm` as `application/wasm`. The files must be loaded over http(s), not opened from disk.

**Adding it to a website.** Keep the four files together in one folder. Either link to that folder's `index.html`, or embed it in a page:

```html
<iframe src="/play/poker/index.html" title="Poker Trainer"
        style="width:100%; aspect-ratio:16/9; border:0" allow="fullscreen"></iframe>
```

A ready-built copy of these four files is committed in `build/web/`, so the site can use them without installing Emscripten.

The game scales to whatever size it is given; the table is laid out for 16:9 and is letterboxed otherwise. It's best at 960×540 or larger, and it's built for a mouse and keyboard, not phones. The page template is `web/shell.html` (loading bar, error message, keyboard focus, and it stops the arrow keys from scrolling the host page). In the browser the game works exactly like the desktop version, including every keyboard shortcut. Nothing is saved between visits. If a file fails to download or the browser lacks WebGL, the page shows an error message instead of an endless loading bar.

Two extra test targets cover the web build:
- `make test-wasm` runs the whole test suite compiled to WebAssembly under Node, with the web build's 1 MB stack and stack-overflow checks on (wasm is a 32-bit target, so this also catches integer-size differences).
- `make test-web` (needs Node; installs Playwright and Chromium into `tests/` on first use) plays the real page in headless Chromium: it loads at several sizes and pixel ratios with a sharp full-size canvas; starts a game with the mouse; plays 40 hands of Omaha against Hard bots with the keyboard, with no errors and no frame over 1 s; starts Seven-Card Stud, 2-7 Triple Draw (Training, with draws) and Razz (Learn the rules) from the menu's tabs and plays 30 rounds of each the same way; checks that typing, including spaces, reaches the glossary search; checks that arrow keys don't scroll a host page; checks that separate visits deal different cards; and checks that a missing file shows an error.

`make shots` builds the screenshot version (`-DAUTOTEST`) and runs it: it plays through every screen by itself (the menu tabs, a Learn-mode stud hand, a Training triple-draw hand with a draw, an Omaha Hi-Lo split pot, HORSE, the rule sheets and lessons) and saves PNGs to `build/shots/`. It opens a window, so it needs a display.

`make sim` builds a headless bot-vs-bot simulator: `./sim 20000 0` runs 20,000 Hold'em hands with two bots of each level. The game number follows `src/variants.h` (0 Hold'em, 1 Omaha, 2 Omaha Hi-Lo, 3 5-Card Omaha, 4 Short Deck, 5 Stud, 6 Stud Hi-Lo, 7 Razz, 8 Five-Card Draw, 9 2-7 Triple Draw, 10 2-7 Single Draw, 11 Badugi), or `h` for HORSE and `e` for 8-Game; `SIM_LIMIT=0|1|2` forces no-limit, pot-limit or fixed limit. `./sim 10000 0 3 2 2 2 2 2` puts one Hard bot against five Mediums, and `SIM_BB=15 ./sim ...` starts everyone with 15 big blinds. It checks that no chips are created or lost and prints each level's win rate in bb/100.

`make nash` re-solves the short-stack push/fold ranges (`tools/nash.c`, about 20 s) and rewrites `src/pushfold.c`.

`make test` runs the test suite (`tests/test_main.c`, about 6,780 checks, ~10 s):
- **Hand evaluators:** exact category counts over all 2,598,960 five-card hands (and all 376,992 short-deck hands), plus edge cases (wheel, steel wheel, kickers, Omaha's 2+3 rule). Ace-to-five and 8-or-better lows, deuce-to-seven (A-2-3-4-5 is ace high, 2-3-4-5-6 a straight), badugi, and Omaha Hi-Lo using different hole cards for high and low.
- **Rules of every game on rigged decks:** stud and Razz bring-in (with suit ties), who acts first on each street, the open-pair big bet, completing, the fixed-limit cap and no cap heads-up, draw order with the deck running out (no card ever appears twice), hi-lo splits, quartering, no-low scoops, odd chips and side pots, and the mixed-game rotation.
- **Rules coach:** a complete rule sheet for every game and structure, facts from Wikipedia each sheet must state, narration of every engine event, "your turn" text and showdown explanations, fuzzed over every game, structure and rotation; no text is ever cut off.
- **Range parser and charts:** combo counts, and no hand listed as both a raise and a call.
- **Betting rules on rigged decks:** blinds and action order at every table size, heads-up order, BB option, min-raises, short all-ins that don't reopen betting, pot-limit maximums, side pots, split pots and odd chips. A bet nobody can match is returned as soon as the betting round ends, not at showdown.
- **Random-action fuzzing:** every game in every structure it allows, checking that no chips are created or lost, no invalid states appear, and no uncallable chips sit in the pot between streets.
- **Equity:** checked against known matchups (AA vs KK, AA vs a random hand, ...).
- **Outs and nut detection.**
- **Coach:** scenario tests, plus well-formed advice in every spot the bots create (no explanation cut off, a valid quiz checklist for every decision). Pot odds only count what a call can actually win: a short stack facing a bigger all-in needs ~47%, not ~4%.
- **Push/fold solver:** the solved ranges match published Nash charts (small blind ~58% at 10 BB), and the coach follows them exactly; short all-ins, re-steals and value bets judged against the calling range.
- **Glossary:** every jargon word the coach uses has an entry, every tooltip term exists, and term detection matches whole words only.
- **Bots:** they never send an illegal action, in any game.
- **Card arranging:** drag, click-swap and sort for 2 to 7 cards, and marking cards to throw away in a draw.

## Table setup (menu)

| Setting | Options |
|---|---|
| Game | A tab per family (Board, Stud, Razz, Draw, Mixed), then the game, with a one-line description |
| Betting | Only the structures that game is played with (see the table above); mixed games use each game's own |
| Mode | Play, Learn the rules, or Training (coach) |
| Opponents | 1 (heads-up) to 5 (6-max) |
| Bot skill | Easy / Medium / Hard per seat (click a bot to cycle, or set all) |
| Blinds / Limits | $1/$2 up to $250/$500. In fixed-limit games the small and big bet are one and two big blinds; stud shows its ante and bring-in |
| Buy-in | 20–300 big blinds (bots buy in for the same amount and reload when they go broke) |
| Other | game speed, four-color deck, auto-deal |

## Bot difficulty

| Level | Behaviour |
|---|---|
| Easy | Loose-passive calling station: plays about half its hands, limps, calls with any pair or draw and rarely raises. |
| Medium | Tight and straightforward, but ignores position, and judges its hand against *random* cards rather than the hands its opponents actually show, so it pays off strong bets. |
| Hard | Plays the coach's strategy (position charts, range-based equity, sized bets, mixed bluffs) and adjusts to opponents: it stops bluffing calling stations and value-bets them thinner. |

In Hold'em and Omaha the bots are the ones described above. In the other games, Easy calls too much, Medium plays its share of the pot against random hands, and Hard plays the coach's tips, reading what the others' bets, up cards and draws say about their hands.

In simulation, Hold'em (30k hands, 2 bots of each level): Hard about +186 bb/100, Medium −51, Easy −135. Hard vs five Mediums: about +295 bb/100. Starting at 15 BB: Hard +87. Omaha (15k hands): Hard +354, Medium −21, Easy −333. Every other game also ranks Hard > Medium > Easy (for example Seven-Card Stud and 2-7 Triple Draw at 3k hands: Hard +42 / +72, Medium +34 / +49, Easy −76 / −121).

## Learn the rules mode

- **Rules tab.** A running list of what happened this hand, explained in plain English with the real names and cards: the forced bets, the deal, who must bring in and why ("the 2 of clubs, the lowest up card; clubs is the lowest suit, which breaks the tie"), who acts first on each street, the cap, all-ins and side pots, each draw. The newest line is highlighted. A concept gets the long explanation the first time it comes up in a game, a short one after that.
- **Your turn.** What you may do right now and why: "Fold, call $5, or raise to $10. Fixed limit: on 5th street every bet is the big bet. One more raise caps the betting."
- **Who won and why.** After each hand: which hand won each pot and what decided it (the hand type, the kicker, whether a low qualified, splits, quarters, scoops and odd chips).
- **Pause at each street.** The game stops whenever new cards come, so you can read; press Continue (or Space), or turn Pause off in the panel.
- **Rule sheets.** The full rules of the game open before your first hand of it (in a mixed game, as each new game starts). H opens them at any time, and the Rules list has every game.
- Terms the rules coach uses appear as chips under it: hover for the definition, click for the glossary.

## Training mode

Training has the same Rules tab as Learn mode, plus a coach. In Hold'em and Omaha (high only) it's the graded coach described below. In the other games the coach gives **tips without grades**: what your hand is under this game's ranking ("8-6-5-3-2 low", "High: Flush. Low: 8-6-4-3-A"), your share of the pot against the hands the others could hold (weighted by how they've bet, their up cards and how many cards they drew), the price of calling, a suggested play, and which cards to throw away in a draw (outlined in green; "Coach's cards" marks them for you). The session line counts how often you played the same as the tip. In mixed games the coach switches between the two as the game changes.

- **Coach panel.** For each decision it shows the situation (e.g. "Facing a 2.5 BB open from CO"), your hand and draws (outs and the exact chance to hit), pot odds (the equity you need), your equity against the opponents' estimated ranges, and the stack-to-pot ratio. Then it shows its recommended play, a grade for every option, and the reasons.
- **Hints on / Quiz mode.** With hints on, the recommendation is shown before you act, the best button is outlined and a green tick on the slider marks the suggested size. In quiz mode the recommendation stays hidden until you act.
- **Grades.** Each play is graded Best / Good / Inaccuracy / Mistake / Blunder. Good bet sizing matters: the right action at a poor size is an Inaccuracy.
- **Session score and leaks.** The panel keeps your hit rate and your two biggest leaks (e.g. "Calling without the odds", "Missing value bets").
- **Hand review.** After each hand you see your decisions with grades, and every bot's cards, including the ones that folded.
- **HUD.** VPIP / PFR stats appear under each bot. The coach uses them too, so a bot that plays loose gets a wider estimated range.
- **Charts (G).** 13×13 preflop charts: opening ranges by position, facing an open (3-bet / call), blind defense and facing a 3-bet. Your current hand is outlined in gold.
- **Think it through (quiz mode).** Instead of the answer, the coach panel lists the questions a strong player asks at this decision, filled in with the facts: your position and who acts behind you, the stack depth, what each opponent's betting line was, the board texture and who it favours, the price, and your plan for the next card.
- **Glossary (T).** About 190 terms in ten categories (basics, positions, preflop, betting after the flop, hands & boards, math & odds, strategy & theory, players & the table, games & rules, this trainer), searchable as you type.
- **Hover to learn.** Hover a stat box, a seat's position, a bot's VPIP / PFR line, a grade or the "mix" label for its definition. Under the coach's reasons, a TERMS row lists the jargon it used: hover for the definition, click to open the glossary there.
- **Lessons (L).** Sixteen lessons. Hold'em & Omaha basics: position, raise-or-fold (with push/fold and re-steals), pot odds & equity, value betting & sizing, bluffing & MDF, Omaha. At a real table: thinking through a hand step by step, ranges and combos, reading real players (player types, bet-sizing patterns, live tells), bankroll, variance & tilt, and how to keep improving (study routine, and live-casino adjustments and etiquette). Other games: seven-card stud (live cards, the bring-in, 5th street), hi-lo split games (scooping, counterfeiting), lowball and Razz (A-5 vs 2-7), draw games and Badugi (what to draw, reading draws), and mixed games (switching gears, fixed-limit strategy).

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


Rearrange your hole cards: drag a card to a new spot, or click two cards to swap them. **Sort** orders them by rank or by suit, and that choice carries over to later hands. Stud cards stay in the order they were dealt; your down cards are marked DOWN (only you see them).

**Fixed limit** has no slider: one Bet / Raise button for the set size, with a note saying how many raises are left before the cap. In stud, the bring-in player chooses **Bring in** or **Complete**; with an open pair on 4th street you choose the small or the big bet.

**Drawing:** click the cards you want to throw away (or press 1–5); they lift up and say TOSS. Then **Draw** (D or Enter), or **Stand pat** (P) to keep them all. Each seat shows how many cards that player drew.

F fold · C/K check, call or bring in · R or Enter bet/raise/complete · B big bet (open pair) · ↑/↓ or mouse wheel adjust size (or drag the slider, or use the presets) · 1–5 mark cards, D draw, P stand pat · Space next hand / continue · Esc menu · H rules of this game · T glossary · L lessons · G charts · M sound on/off

## Code layout

| File | Purpose |
|---|---|
| `src/variants.c` | The table of games: family, cards, deck, ranking, draws, streets, betting structures; the mixed rotations |
| `src/cards.c` | RNG and hand evaluators: high (5–7 cards, Omaha 2+3 with 4 or 5 cards), short deck, ace-to-five and 8-or-better low, deuce-to-seven, badugi |
| `src/range.c` | 169 Hold'em hand classes, range strings ("22+,A2s+,KTo+"), preflop strength tables, Omaha hand score |
| `src/equity.c` | Range-weighted Monte Carlo equity, opponent hand tiers, outs, nut check |
| `src/game.c` | Dealing, blinds and antes, stud bring-in and acting order, draws, betting rules (NL / PL / fixed limit with the cap, min-raise, short all-ins), side pots, hi-lo splits, mixed rotation; records events for the rules coach |
| `src/strategy.c` | The coach: charts, range estimates, recommendations, grades and explanations |
| `src/basic.c` | Every other game: Monte Carlo equity, what to draw, the tips and the bots |
| `src/rules.c` | The rules coach: rule sheets, narration of what happened, "your turn", showdown explanations |
| `src/ai.c` | Easy / Medium / Hard bots (Hold'em / Omaha), and the dispatch to `basic.c` for the other games |
| `src/pushfold.c` | Solved push/fold ranges (generated by `tools/nash.c`) |
| `src/glossary.c` | The glossary: terms, definitions, search and term detection in coach text |
| `src/arrange.c` | Your card order: drag, click-swap, sort and marking discards (no raylib, unit-tested) |
| `src/sound.c` | Sound effects (card deal, win cheer, losing trombone), synthesized at startup, so there are no audio files |
| `src/main.c` | raylib front end (desktop and web) |
| `web/shell.html` | Page template for the web build (`make web`) |
| `tools/sim.c` | Headless simulator |
| `tools/nash.c` | Push/fold equilibrium solver |
