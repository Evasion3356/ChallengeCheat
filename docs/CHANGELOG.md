# Changelog

All notable user-facing changes to ChallengeCheat are recorded here. Format
loosely follows [Keep a Changelog](https://keepachangelog.com/).

## [1.0.0] - 2026-09-18

First release.

### Added
- F9 menu with one submenu per singleplayer Challenge category (Bandit,
  Explorer, Gambler, Herbalist, Horseman, Master Hunter, Sharpshooter,
  Survivalist, Weapons Expert). Each shows a live "<Challenge> X / 10"
  status line.
- "Advance <Challenge> N": applies one small step toward the current
  rank's goal (e.g. one more kill, one more item found), like a real
  playthrough would.
- "Complete <Challenge> N": completes the current rank only (never
  jumps ahead several ranks).
- Both actions are fire-and-forget: no popup on success; a message is
  shown only when the action can't be attempted (not on horseback, not
  riding a train, category already maxed, or no known method for that
  goal).
- Progress is applied by writing the same stats and natives the game's
  own scripts use, so it goes through the game's normal challenge
  evaluation.
- All nine Challenges support ranks 1-10 and were tested in game via
  Advance, including the Horseman timed rides (3/6/9), the rank 10
  wild-horse breeds, Explorer's treasure ranks, Herbalist's herb goals,
  Master Hunter's tracking goals and Sharpshooter's Dead Eye goals.
- Advance takes the smallest real step (one herb, one breed, one kill);
  goals with no partial step (records, timed windows) apply in full.
- `ChallengeCheat.ini` (created next to the `.asi` on first run) with a
  configurable `MenuKey` under `[General]` (default `F9`).
- Menu styled after RDR2's own menu chrome; navigate with NUMPAD 8/2,
  select with NUMPAD 5, back with NUMPAD 0/Backspace.
- Log written to `ChallengeCheat.log` in the game folder.

### Known limitations
- Some ranks need a live condition, and the menu tells you when it isn't
  met: Horseman ranks 1, 4, 7 and 8 need you on horseback; Sharpshooter
  rank 3 needs you on a moving train; Sharpshooter ranks 2 and 9 need you
  aiming with Dead Eye already on; Master Hunter rank 3 needs binoculars
  or a scope raised.
- On a new game, a category stays hidden until the story reaches it; the
  mod reveals and unlocks it automatically the first time you use it.
- Advance's "one item at a time" counters (distinct herbs, horse breeds)
  aren't saved across a game restart; use Complete to finish a rank after
  one.
- Whether the item/cosmetic rewards for a cheat-completed rank are
  granted (versus only the rank counter advancing) has not been
  independently confirmed.
