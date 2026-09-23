# Changelog

All notable user-facing changes to ChallengeCheat are recorded here. Format
loosely follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

## [1.2.0] - 2026-09-23

### Changed
- Advance on record- and amount-type goals now applies the full value in
  one click instead of adding 1 per click, which would have taken
  thousands of clicks: Bandit 5 (highest bounty), Bandit 7 (hold-up cash),
  Horseman 4 (lasso drag distance), Sharpshooter 4 and 6 (furthest kill).
- The menu stays on screen while an action waits on the game, e.g.
  Complete on Horseman 10 while the horse models load.

### Fixed
- Advance's per-item progress (distinct herbs, animals, horse breeds) now
  resets when you load a save or start a new game, instead of claiming
  items were already credited in a different save.
- The Horseman timed-ride unlock (ranks 3/6/9) can no longer fire twice
  for one click, and on a game build whose code doesn't match exactly it
  now refuses to install rather than guess.
- Closing the game no longer tears down the timed-ride hook while the
  game is already shutting down.
- If the game folder can't be written (e.g. a `C:\Program Files` install,
  or a read-only/locked log file), the log now goes to
  `%LOCALAPPDATA%\RDR2ASIMods\ChallengeCheat.log` instead, and its first line
  names the path that couldn't be used.

## [1.1.0] - 2026-09-19

### Added
- Localization into all 13 languages RDR2 ships with. Category names and
  the Dead Eye term are the game's own per-language wording (extracted
  from its text files, not translated); the Advance/Complete verbs and
  the "can't do that yet" messages are LLM-assisted translations, not yet
  reviewed by native speakers.
- Each category submenu now shows the game's own objective text for the
  next rank (e.g. "Hold up 5 townsfolk"), word-wrapped in the game's
  language.
- `[General] WrapWidth` in `ChallengeCheat.ini` (default 50; 0 = never
  wrap; or 10-120): how many characters fit on one line of the objective text.
  With wrapping off (0), the longest objectives run past the menu box.
- `[General] Language` in `ChallengeCheat.ini` (`auto` follows the game's
  UI language; or `en-US`, `fr-FR`, `de-DE`, `it-IT`, `es-ES`, `pt-BR`,
  `pl-PL`, `ru-RU`, `ko-KR`, `zh-TW`, `ja-JP`, `es-MX`, `zh-CN`). Needs a
  game restart; non-Latin overrides only render correctly when the game
  itself is set to that language.

### Changed
- Faster rank/goal lookups behind the menu (fewer repeated table scans
  per frame and per click). No behavior change.

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
