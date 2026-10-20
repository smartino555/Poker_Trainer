// glossary.c - every poker term the game uses, in plain English
#include "glossary.h"
#include <ctype.h>
#include <string.h>

static const char *CAT_NAME[GC_N] = {
    "Basics", "Positions", "Preflop", "Betting after the flop", "Hands & boards", "Math & odds", "Strategy & theory", "Players & the table", "This trainer",
};

static const GlossEntry G[] = {
    // ---------------------------------------------------------------- basics
    { "Hole cards", GC_BASICS, "hole cards", "Your private cards: 2 in Hold'em, 4 in Omaha. Nobody else sees them unless the hand goes to showdown." },
    { "Board", GC_BASICS, "the board|on the board", "The shared cards in the middle that everyone uses: 3 on the flop, 1 on the turn, 1 on the river. Also called community cards." },
    { "Preflop", GC_BASICS, "", "The first betting round, after the hole cards are dealt and before any board cards. Most of your money-saving decisions happen here." },
    { "Flop", GC_BASICS, "", "The first three board cards, dealt together, and the betting round that follows." },
    { "Turn", GC_BASICS, "", "The fourth board card and the betting round after it. Bets usually get bigger here." },
    { "River", GC_BASICS, "", "The fifth and last board card and the final betting round. No more cards to come, so draws are dead: you either have the best hand or you don't." },
    { "Street", GC_BASICS, "street", "One betting round: preflop, flop, turn or river." },
    { "Blinds", GC_BASICS, "blinds", "Forced bets posted before the cards are dealt so there is always something to win. The small blind is half the big blind." },
    { "Big blind (BB) as a unit", GC_BASICS, "", "Poker players measure stacks and bets in big blinds so advice works at any stakes. A 100 BB stack at $1/$2 is $200; a 2.5 BB raise is $5." },
    { "Pot", GC_BASICS, "", "All the chips bet so far in the hand. The winner takes it." },
    { "Check", GC_BASICS, "", "Pass the action without betting. Only possible when nobody has bet on this street." },
    { "Call", GC_BASICS, "", "Match the current bet to stay in the hand." },
    { "Bet / raise", GC_BASICS, "", "A bet is the first money put in on a street; a raise increases someone else's bet. In this game the raise amount is always 'raise TO' (the total)." },
    { "Fold", GC_BASICS, "", "Give up the hand and any chips you've already put in. Folding is free - it's often the best play." },
    { "All-in", GC_BASICS, "all-in|all in|jam", "Betting every chip you have. You can't be forced out of the hand after that; if others keep betting, a side pot is made." },
    { "Side pot", GC_BASICS, "side pot", "When a player is all-in for less, they can only win the chips they matched. Extra bets between the other players go into a separate side pot." },
    { "Showdown", GC_BASICS, "showdown", "When betting is finished on the river (or everyone is all-in), the remaining players show their cards and the best hand wins." },
    { "Muck", GC_BASICS, "", "To fold or throw away your cards without showing them." },
    { "Kicker", GC_BASICS, "kicker", "The side card that breaks ties between equal hands. With A-K on a K-7-2 board you have top pair with an ace kicker - it beats K-Q." },
    { "Split pot", GC_BASICS, "split|tie", "When two players have exactly the same five-card hand, they share the pot." },
    { "No-Limit", GC_BASICS, "no-limit", "You may bet any amount, up to all your chips, at any time. The standard Hold'em format." },
    { "Pot-Limit", GC_BASICS, "pot-limit|pot limit|raise the pot", "The biggest bet or raise allowed is the size of the pot (counting your call first). Standard for Omaha." },
    { "Min-raise", GC_BASICS, "min-raise|minimum raise", "The smallest legal raise: at least the size of the previous bet or raise. If the bet is 6, the next raise must be to at least 12." },
    { "Effective stack", GC_BASICS, "effective stack", "The smaller of your stack and your opponent's - the most you can actually win or lose against them. All stack-depth advice uses it." },
    { "Hand rankings", GC_BASICS, "", "Best to worst: straight flush, four of a kind, full house, flush, straight, three of a kind, two pair, one pair, high card." },

    // ---------------------------------------------------------------- positions
    { "Position", GC_POSITIONS, "position", "Where you sit relative to the dealer button. It decides who acts last after the flop - and acting last, with more information, is a big edge." },
    { "In position (IP)", GC_POSITIONS, "in position|act last", "You act after your opponent on every street after the flop. You see what they do first, can check behind for a free card and control the pot size." },
    { "Out of position (OOP)", GC_POSITIONS, "out of position", "You must act first after the flop. It makes every hand harder to play, so you need stronger hands to get involved." },
    { "Button (BTN)", GC_POSITIONS, "button|btn", "The dealer seat. It acts last on every street after the flop - the most profitable seat at the table, so you play the most hands here." },
    { "Cutoff (CO)", GC_POSITIONS, "cutoff", "The seat just right of the button. Second-best position: only the button and blinds act after you preflop." },
    { "Hijack (HJ)", GC_POSITIONS, "hijack", "Two seats right of the button (6-handed). A middle position: open a bit wider than UTG." },
    { "Under the Gun (UTG)", GC_POSITIONS, "under the gun|utg", "First to act preflop. Everyone else acts after you, so open only strong hands (about 18% at 6-max)." },
    { "Small blind (SB)", GC_POSITIONS, "small blind", "Posts half a big blind and acts first after the flop - the worst seat. Folded to you, prefer raise-or-fold over calling." },
    { "Big blind (BB)", GC_POSITIONS, "big blind", "Posts a full blind and acts last preflop. You already have money in, so you get a discount and defend wide - but you're out of position afterwards." },
    { "Early / late position", GC_POSITIONS, "early position|late position", "Early = UTG and Hijack: many players behind, play tight. Late = Cutoff and Button: few behind, play loose." },
    { "Heads-up", GC_POSITIONS, "heads-up|heads up", "Just two players, in a hand or at the table. Heads-up the button is also the small blind and acts first preflop but last afterwards." },
    { "6-max", GC_POSITIONS, "6-max|6-handed", "A table with at most six players - the most common online format. The coach's charts are built for it." },
    { "Multiway", GC_POSITIONS, "multiway", "A pot with three or more players. Someone usually connects with the board, so bluff less and value-bet only solid hands." },
    { "Closing the action", GC_POSITIONS, "close the action|closes the action", "You're the last player who can act in a betting round, so nobody can raise behind you. The big blind closes the action preflop." },

    // ---------------------------------------------------------------- preflop
    { "Open (raise first in)", GC_PREFLOP, "opened|opens|open from|opening range|unopened", "The first raise into an unopened pot (RFI = raise first in). The standard open is about 2.5 BB." },
    { "Limp", GC_PREFLOP, "limp|limper", "Just calling the big blind preflop instead of raising. Usually a sign of a weak or passive player; good players rarely open-limp." },
    { "Over-limp", GC_PREFLOP, "over-limp", "Limping after others have limped. Fine with speculative hands (small pairs, suited connectors) that want a cheap multiway flop." },
    { "Isolate (iso-raise)", GC_PREFLOP, "isolate|iso", "Raising over limpers to play the pot heads-up against a weak player, with position and the initiative. Standard size: 3 BB + 1 BB per limper." },
    { "Cold call / flat", GC_PREFLOP, "flat|cold call", "Calling a raise rather than re-raising. A cold call is calling a raise when you have no money in the pot yet." },
    { "3-bet", GC_PREFLOP, "3-bet|3-bets|re-raise", "The first re-raise preflop (the blinds count as the first 'bet', the open as the second). Size: about 3x the open in position, 4x out of position." },
    { "4-bet / 5-bet", GC_PREFLOP, "4-bet|5-bet", "A re-raise of a 3-bet (4-bet), and of a 4-bet (5-bet, usually all-in). These ranges are very strong: QQ+, AK and a few bluffs." },
    { "Squeeze", GC_PREFLOP, "squeeze", "A 3-bet after an open and one or more callers. The callers' ranges are capped, so it often wins the pot right away; make it bigger (add 1x the open per caller)." },
    { "Steal / re-steal", GC_PREFLOP, "steal|re-steal", "A steal is an open from late position mainly to win the blinds. A re-steal is a 3-bet (often all-in when short) against a likely steal." },
    { "Blind defense", GC_PREFLOP, "defend", "Calling or 3-betting from the blinds against a steal. The big blind gets a great price, so it defends a wide range." },
    { "Push/fold", GC_PREFLOP, "push/fold|push or fold|shove|shoves", "With about 12 big blinds or less, raising and folding to a re-raise wastes chips, so you either move all-in or fold. The coach uses solved (Nash equilibrium) push ranges." },
    { "Range chart", GC_PREFLOP, "chart", "A 13x13 grid of all 169 starting hands, coloured by what to do. Open the Charts button to see the coach's charts." },
    { "Suited / offsuit", GC_PREFLOP, "suited|offsuit", "Suited: both hole cards of the same suit (AKs). Offsuit: different suits (AKo). Suited hands make flushes, worth roughly 2-4% more equity." },
    { "Pocket pair", GC_PREFLOP, "pocket", "Two hole cards of the same rank, like 77. Small pairs mostly hope to flop a set (about 1 time in 8.5)." },
    { "Broadway", GC_PREFLOP, "broadway", "Cards T through A. Broadway hands (KQ, AJ, QT...) make top pairs and the highest straight." },
    { "Suited connectors", GC_PREFLOP, "connector", "Suited cards next to each other in rank, like 87s. They rarely make big pairs but flop straights and flushes that win big pots - best played in position with deep stacks." },
    { "Set mining", GC_PREFLOP, "set mining", "Calling with a small pair just to hit three of a kind. It needs deep stacks: you hit 12% of the time, so you need about 15x the call left to win." },
    { "Premium hands", GC_PREFLOP, "premium", "The very best starting hands: AA, KK, QQ, AK (and often JJ). They are raised and re-raised for value." },
    { "Starting-hand strength (top X%)", GC_PREFLOP, "of starting hands|top ~", "Where your hand ranks among all starting hands. 'Top 5%' means only 5% of hands are as strong. AA is the top 0.5%." },

    // ---------------------------------------------------------------- postflop betting
    { "Continuation bet (c-bet)", GC_POSTFLOP, "c-bet|continuation bet", "A flop bet by the preflop raiser. Their range is usually stronger, so a small c-bet (about 1/3 pot) works often - especially on dry, high-card boards." },
    { "Barrel", GC_POSTFLOP, "barrel", "Betting again on the next street after betting the last one. A 'double barrel' bets flop and turn; a 'triple barrel' bets all three streets." },
    { "Stab / probe", GC_POSTFLOP, "stab|probe", "Betting into a pot after the other player showed weakness by checking. A probe is a bet out of position after the preflop raiser checked back the previous street." },
    { "Donk bet", GC_POSTFLOP, "donk", "Betting into the preflop raiser from out of position before they can c-bet. Uncommon in solid strategy, often a sign of a medium or weak hand from recreational players." },
    { "Check-raise", GC_POSTFLOP, "check-raise", "Checking, then raising after an opponent bets. A strong move from out of position, with very strong hands and good draws." },
    { "Check behind", GC_POSTFLOP, "check behind|checking behind|checked behind", "Checking as the last player to act, which ends the betting round. Often used to take a free card or to get a medium hand to showdown cheaply." },
    { "Float", GC_POSTFLOP, "float", "Calling a bet (usually in position) with a weak hand, planning to take the pot away later when the opponent shows weakness." },
    { "Slowplay / trap", GC_POSTFLOP, "trap|slowplay", "Playing a very strong hand passively (checking or just calling) to let opponents bet or catch up. Works best on dry boards where few free cards can hurt you." },
    { "Value bet", GC_POSTFLOP, "value", "A bet you make because worse hands will call it. The rule: bet for value when you're ahead of the range that calls - not just ahead of everything they might have." },
    { "Thin value", GC_POSTFLOP, "thin value|thinner", "A value bet with a hand that's only a little ahead of the calling range - like second pair on a safe river against a player who calls too much." },
    { "Bluff", GC_POSTFLOP, "bluff", "A bet with a weak hand to make better hands fold. A bluff needs to work more than bet / (pot + bet) of the time to show a profit on its own." },
    { "Semi-bluff", GC_POSTFLOP, "semi-bluff", "A bet with a draw. You win when they fold now, and when called you can still hit and win - the best kind of bluff." },
    { "Free card", GC_POSTFLOP, "free card|cheap card", "A turn or river card you see without paying because everyone checked. Giving free cards to draws is costly; betting charges them." },
    { "Pot control", GC_POSTFLOP, "keep the pot small|pot control", "Checking or calling with a medium hand to keep the pot small, so you don't lose a big pot to a better hand." },
    { "Bluff catcher", GC_POSTFLOP, "bluff catcher|bluff-catcher", "A hand that beats only bluffs - it loses to every value bet. You call with it when the price is right and the bettor bluffs enough." },
    { "Showdown value", GC_POSTFLOP, "showdown value|win at showdown", "A hand that can win if the hand is checked down, like a weak pair. Such hands usually check: they're too weak to bet, too good to bluff with." },
    { "Bet sizing", GC_POSTFLOP, "1/3 pot|2/3 pot|3/4 pot|pot-sized|sizing", "Bets are described as a fraction of the pot. Small (1/3) on dry boards and with your whole range; bigger (2/3 to pot) on wet boards and with polarized ranges." },
    { "Overbet", GC_POSTFLOP, "overbet", "A bet bigger than the pot. Used with polarized ranges (nuts or air) when your opponent's range is capped." },
    { "Give up", GC_POSTFLOP, "give up|giving up", "Stop betting a hand that can't win: check and fold to a bet. Knowing when to give up a bluff saves a lot of money." },

    // ---------------------------------------------------------------- hands & boards
    { "Top pair", GC_HANDS, "top pair", "A pair using the highest card on the board. Strong on the flop, but beaten by overpairs, two pair and sets - beware big action." },
    { "Overpair", GC_HANDS, "overpair", "A pocket pair higher than every board card (QQ on J-7-2). Usually a strong one-pair hand." },
    { "Underpair", GC_HANDS, "underpair", "A pocket pair below the top board card (66 on K-9-3). Weak: it mostly wants to get to showdown cheaply." },
    { "Middle / bottom pair", GC_HANDS, "middle pair|bottom|weak pair", "A pair with the second or lowest board card. Has some showdown value but rarely wants to build a big pot." },
    { "Two pair", GC_HANDS, "two pair", "Two different pairs. Strong when both pairs use your hole cards; much weaker when one pair is on the board (everyone has it)." },
    { "Set", GC_HANDS, "set of|a set", "Three of a kind made with a pocket pair plus one board card. Very strong and well hidden." },
    { "Trips", GC_HANDS, "trips", "Three of a kind made with one hole card and a pair on the board. Strong, but your kicker matters and it's easier to read than a set." },
    { "Straight / flush / full house", GC_HANDS, "straight|flush|full house|full-house", "Straight: five cards in a row (A can be high or low). Flush: five of a suit. Full house ('boat'): three of a kind plus a pair." },
    { "The nuts", GC_HANDS, "the nuts|nut ", "The best possible hand on this board. A 'nut flush draw' draws to the ace-high flush. Non-nut hands lose big pots, especially in Omaha." },
    { "Draw", GC_HANDS, "draw", "A hand that isn't made yet but can improve to a strong hand, like four cards to a flush." },
    { "Flush draw", GC_HANDS, "flush draw", "Four cards of a suit, needing one more: 9 outs, about 35% by the river from the flop." },
    { "Open-ended straight draw (OESD)", GC_HANDS, "open-ended", "Four cards in a row that either end can complete (8-9-T-J needs a 7 or Q): 8 outs, about 31% from the flop." },
    { "Gutshot", GC_HANDS, "gutshot", "A straight draw that needs one specific inside rank (8-9-J-Q needs a T): 4 outs, about 16% from the flop." },
    { "Wrap", GC_HANDS, "wrap", "Omaha: a straight draw where your cards surround the board cards, with 9 to 20 outs - often a favourite even against a set." },
    { "Combo draw", GC_HANDS, "combo draw| + ", "A hand with two draws at once, such as a flush draw plus a straight draw (15 outs). Often a favourite against one pair." },
    { "Backdoor draw", GC_HANDS, "backdoor", "A draw that needs both the turn and the river (three cards to a flush on the flop). Worth only about 4% but adds to a hand's playability." },
    { "Overcards", GC_HANDS, "overcard", "Hole cards higher than every board card. Two overcards have 6 outs to top pair." },
    { "Dry board", GC_HANDS, "dry", "A board with few draws, like K-7-2 of three suits. The current best hand is likely to stay best; bet small." },
    { "Wet board", GC_HANDS, "wet", "A board full of draws, like J-T-8 with two hearts. Hands change a lot on later streets; bet bigger to charge draws, or check more." },
    { "Rainbow / two-tone / monotone", GC_HANDS, "rainbow|two-tone|monotone|three of a suit|four of a suit", "Rainbow: all board cards different suits. Two-tone: two of one suit (flush draws possible). Monotone: three of a suit (a flush is already possible)." },
    { "Paired board", GC_HANDS, "paired", "A board with two cards of the same rank. Full houses and trips become possible; fewer hands connect, so bluffs and c-bets work more often." },
    { "Connected board", GC_HANDS, "connected", "Board cards close in rank (9-8-6), making straights and straight draws likely." },
    { "Rundown (Omaha)", GC_HANDS, "rundowns|rundown", "Four connected cards like J-T-9-8. They make many straights and wraps." },
    { "Double-suited (Omaha)", GC_HANDS, "double-suited|single-suited", "Omaha hands with two cards of one suit and two of another (both can make flushes). Single-suited has one suited pair." },
    { "Dangler (Omaha)", GC_HANDS, "dangler", "An Omaha card that doesn't work with the other three (like the 2 in A-K-Q-2). It weakens the hand." },
    { "Blockers", GC_HANDS, "blocker|blocks", "Cards in your hand that make certain opponent hands impossible or less likely. Holding the ace of hearts means nobody else has the nut heart flush." },

    // ---------------------------------------------------------------- math & odds
    { "Equity", GC_MATH, "equity", "Your share of the pot if all remaining cards were dealt with no more betting: how often you'd win (ties count half). 60% equity in a $100 pot is worth $60 on average." },
    { "Pot odds", GC_MATH, "pot odds|equity needed|you need", "The price of a call. Needed equity = call / (pot after you call). Calling $50 into a $100 pot plus the $50 bet: 50 / 200 = 25%." },
    { "Implied odds", GC_MATH, "implied odds", "The extra money you expect to win on later streets when you hit. They let you call draws a little lighter with deep stacks - never on the river." },
    { "Reverse implied odds", GC_MATH, "reverse implied", "When you hit, but a better hand gets paid off from you (a low flush against the nut flush). Weak draws and dominated hands suffer from it." },
    { "Outs", GC_MATH, "outs", "Unseen cards that improve your hand to a likely winner. A flush draw has 9 outs, an open-ended straight draw 8, a gutshot 4." },
    { "Rule of 4 and 2", GC_MATH, "rule of 4 and 2", "Quick odds: outs x 4 on the flop is roughly your chance to hit by the river; outs x 2 on the turn is your chance on the river. 9 outs -> ~36% / ~18%." },
    { "Expected value (EV)", GC_MATH, "on average|ev |+ev|-ev", "The average result of a play if you made it many times. Good poker is choosing the highest-EV option, whatever happens in this one hand." },
    { "Stack-to-pot ratio (SPR)", GC_MATH, "stack-to-pot|spr|stack / pot", "Effective stack divided by the pot on the flop. Low SPR (under 2): top pair can happily go all-in. High SPR (over 6): you want a much stronger hand to play for stacks." },
    { "Minimum defense frequency (MDF)", GC_MATH, "mdf|minimum defense", "Facing a bet B into a pot P, you must continue with about P / (P + B) of your range, or the bettor profits by bluffing with any two cards. Against a pot-sized bet: 50%." },
    { "Fold equity", GC_MATH, "fold equity|they fold", "The value you gain from your opponent folding. A bet's total value = what you win when they fold + your equity when they call." },
    { "Combos", GC_MATH, "combo", "The number of ways a hand can be dealt: a pair has 6 combos, a suited hand 4, an offsuit hand 12. Counting combos tells you how likely each hand in a range is." },
    { "Variance", GC_MATH, "variance", "The short-term swings of luck. Even a big winner has losing weeks; judge your play by decisions, not results." },
    { "bb/100", GC_MATH, "bb/100", "Win rate: big blinds won per 100 hands. +5 bb/100 is a solid online win rate; the Hard bot's +180 against weak bots shows how much they leak." },

    // ---------------------------------------------------------------- strategy & theory
    { "Range", GC_THEORY, "range", "All the hands a player could have in a spot, given everything they've done. Good players think 'what is my opponent's range?' instead of guessing one hand." },
    { "Range advantage", GC_THEORY, "range advantage|range is stronger", "When one player's whole range hits a board better. The preflop raiser usually has it on high-card boards (A-K-7), which is why they c-bet often there." },
    { "Nut advantage", GC_THEORY, "nut advantage", "Having more of the very strongest hands possible on a board. It lets you bet big or overbet." },
    { "Capped range", GC_THEORY, "capped", "A range that can't contain the strongest hands because of earlier actions (a player who just called preflop rarely has AA). Capped ranges are easy to pressure." },
    { "Polarized / linear range", GC_THEORY, "polarized|polar|linear", "Polarized: only very strong hands and bluffs (no medium hands) - typical for big bets and river bets. Linear: the best hands down to some cut-off, typical for small bets and preflop opens." },
    { "Balance", GC_THEORY, "balance|balanced", "Mixing value hands and bluffs in the right ratio so opponents can't exploit you by always folding or always calling." },
    { "GTO (game theory optimal)", GC_THEORY, "gto|game theory", "A strategy that can't be exploited: against it, every opponent strategy does at best break even. Real GTO strategies come from solvers and mix many plays at set frequencies." },
    { "Nash equilibrium", GC_THEORY, "nash|equilibrium|solved", "A set of strategies where nobody can gain by changing theirs alone. The coach's short-stack push/fold ranges are computed this way (tools/nash.c)." },
    { "Solver", GC_THEORY, "solver", "Software that computes near-GTO strategies for a spot by simulating the whole game tree. The coach is solver-inspired: its push/fold is solved, the rest follows solver-style rules." },
    { "Exploit", GC_THEORY, "exploit", "Deviating from balanced play to take advantage of an opponent's mistakes: value-bet calling stations relentlessly, bluff players who fold too much." },
    { "Mixed strategy", GC_THEORY, "mix|mixing|frequency", "Playing the same hand different ways at set frequencies (bet 65%, check 35%). It keeps you unpredictable. 'mix ~35%' on the coach panel is how often to take the alternative." },
    { "Initiative", GC_THEORY, "initiative|aggressor", "Being the last player to bet or raise. The aggressor can win the pot with a bet when nobody connects - that's why raising beats calling preflop." },
    { "Equity realization", GC_THEORY, "realize|playability", "How much of your raw equity you actually win in practice. Hands in position, suited and connected realize more; weak offsuit hands out of position realize less." },
    { "Protection", GC_THEORY, "protect|charge the draws", "Betting a made hand so drawing hands pay a price to see more cards, instead of letting them hit for free." },
    { "Bluff-to-value ratio", GC_THEORY, "bet / (pot + 2|bluffs (bet", "On the river, a balanced betting range has bet / (pot + 2 x bet) bluffs: 25% for a half-pot bet, 33% for a pot-sized bet. Then the caller's bluff catchers break even." },

    // ---------------------------------------------------------------- players & table
    { "VPIP", GC_PLAYERS, "vpip", "Voluntarily Put money In Pot: the % of hands a player calls or raises preflop (blinds don't count). Solid 6-max players: about 22-28%. 40%+ is very loose." },
    { "PFR", GC_PLAYERS, "pfr", "Pre-Flop Raise: the % of hands a player raises preflop. A big gap between VPIP and PFR (40 / 8) means they call too much - a 'calling station'." },
    { "HUD", GC_PLAYERS, "hud", "Heads-Up Display: statistics shown under each opponent (here VPIP / PFR and the number of hands seen). The coach widens or narrows their ranges from these." },
    { "Tight / loose", GC_PLAYERS, "tight|loose", "How many hands a player plays. Tight: few, strong hands. Loose: many hands." },
    { "Passive / aggressive", GC_PLAYERS, "passive|aggressive", "How a player puts money in. Passive: checks and calls. Aggressive: bets and raises. Aggression wins pots without the best hand; passivity only wins at showdown." },
    { "TAG / LAG", GC_PLAYERS, "tag|lag", "Tight-aggressive: plays few hands but plays them hard - the solid winning style. Loose-aggressive: many hands, lots of pressure - profitable but harder to play well." },
    { "Nit", GC_PLAYERS, "nit", "A very tight, cautious player. Steal their blinds and fold to their big bets." },
    { "Calling station", GC_PLAYERS, "calling station|station", "A player who calls far too often and rarely raises or folds. Don't bluff them; bet your good hands big and often." },
    { "Maniac", GC_PLAYERS, "maniac", "A hyper-aggressive player who bets and raises constantly. Call down lighter and let them bluff into you." },
    { "Tilt", GC_PLAYERS, "tilt", "Playing worse because of emotion - usually after a bad beat or a losing run. The single biggest leak for most real-life players." },
    { "Tell", GC_PLAYERS, "", "A clue to a player's hand: bet sizing patterns, timing, and in live games physical behaviour. Betting patterns are far more reliable than body language." },
    { "Bankroll", GC_PLAYERS, "bankroll", "Money set aside only for poker. Keep at least 20-40 buy-ins for the cash-game stake you play so normal variance can't break you." },
    { "Buy-in / rebuy", GC_PLAYERS, "buy-in|rebuy", "The chips you sit down with (here measured in big blinds; 100 BB is standard). A rebuy is buying more after you lose your stack." },
    { "Rake", GC_PLAYERS, "rake", "The fee the casino or site takes from each pot. It makes marginal calls and loose play even worse in real games." },
    { "Bad beat / cooler", GC_PLAYERS, "bad beat|cooler", "A bad beat: losing as a big favourite when the opponent hits a long shot. A cooler: two very strong hands collide and the money goes in no matter what. Both are variance, not mistakes." },

    // ---------------------------------------------------------------- this trainer
    { "Grades", GC_APP, "", "Best: the coach's play. Good: a solid alternative. Inaccuracy: a small leak (often the right action at a poor size). Mistake: costs real money. Blunder: a big error, like folding a monster." },
    { "Coach recommends", GC_APP, "", "The play the coach would make, with its size. 'mix ~X%' means a balanced player sometimes takes the listed alternative instead, about X% of the time." },
    { "Hints / Quiz mode", GC_APP, "", "Hints on: the recommendation shows before you act. Quiz mode: it stays hidden and you get a 'think it through' checklist instead; you're graded after you act." },
    { "Leak", GC_APP, "leak", "A repeated mistake that costs money over time, like calling without the odds. The session panel tracks your two biggest leaks." },
    { "Bot levels", GC_APP, "", "Easy: loose-passive calling station. Medium: tight but ignores position and ranges. Hard: plays the coach's strategy, mixes bluffs and adapts." },
};

#define NG (int)(sizeof G / sizeof G[0])

int glossCount(void) { return NG; }
const GlossEntry *glossGet(int i) { return i >= 0 && i < NG ? &G[i] : &G[0]; }
const char *glossCatName(int cat) { return cat >= 0 && cat < GC_N ? CAT_NAME[cat] : ""; }

int glossLookup(const char *term) {
    for (int i = 0; i < NG; i++) if (strcasecmp(G[i].term, term) == 0) return i;
    return -1;
}

// case-insensitive search for 'needle' (length n) in 'hay'; whole words unless the needle starts/ends with a non-letter
static const char *findWord(const char *hay, const char *needle, int n, bool words) {
    if (n <= 0) return NULL;
    for (const char *p = hay; *p; p++) {
        if (strncasecmp(p, needle, n) != 0) continue;
        if (!words) return p;
        bool okL = p == hay || !isalnum((unsigned char)p[-1]) || !isalnum((unsigned char)needle[0]);
        bool okR = !isalnum((unsigned char)p[n]) || !isalnum((unsigned char)needle[n - 1]);
        // allow simple plurals / verb forms: "range" matches "ranges", "bluff" matches "bluffs"
        if (!okR && (p[n] == 's' || p[n] == 'd' || p[n] == 'e') && !isalnum((unsigned char)p[n + 1])) okR = true;
        if (!okR && strncasecmp(p + n, "ing", 3) == 0 && !isalnum((unsigned char)p[n + 3])) okR = true;
        if (okL && okR) return p;
    }
    return NULL;
}

bool glossMatches(int i, const char *q) {
    if (!q || !q[0]) return true;
    int n = (int)strlen(q);
    return findWord(G[i].term, q, n, false) || findWord(G[i].keys, q, n, false) || findWord(G[i].def, q, n, false);
}

int glossFind(const char *text, int *out, int max) {
    const char *first[NG];
    int n = 0;
    for (int i = 0; i < NG; i++) {
        first[i] = NULL;
        const char *k = G[i].keys;
        while (*k) {
            const char *e = strchr(k, '|');
            int len = e ? (int)(e - k) : (int)strlen(k);
            const char *p = findWord(text, k, len, true);
            if (p && (!first[i] || p < first[i])) first[i] = p;
            k += len + (e ? 1 : 0);
        }
    }
    // in order of first mention
    for (;;) {
        int best = -1;
        for (int i = 0; i < NG; i++) if (first[i] && (best < 0 || first[i] < first[best])) best = i;
        if (best < 0 || n >= max) break;
        out[n++] = best;
        first[best] = NULL;
    }
    return n;
}
