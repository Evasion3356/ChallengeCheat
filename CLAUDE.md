# ChallengeCheat

A ScriptHookRDR2 ASI mod that advances/completes RDR2's 9 singleplayer
"Challenges" (Bandit, Explorer, Gambler, Herbalist, Horseman, Master
Hunter, Sharpshooter, Survivalist, Weapons Expert). Built as a sibling of
`../PokerCheat`/`../BlackjackCheat`/`../DominoCheat`, reusing the same
toolchain -- see those projects' `CLAUDE.md` files for the full backstory
on why this stack (ScriptHookRDR2 + native C++) was chosen.

**Unlike its siblings, this mod's menu is NOT a Debug-only dev-tuning
surface.** PokerCheat/BlackjackCheat/DominoCheat are passive advisors that
run unconditionally in Release with no menu at all; their F-key menus are
pure reversing tools. ChallengeCheat has no passive behavior -- "advance
this challenge" and "complete this challenge" are deliberate one-shot
player actions, so the F9 menu is built and available in BOTH Debug and
Release.

## Current status (2026-09-17, third design)

Builds clean in both Debug and Release, deploys via the standard
`BuildTools\Find-RDR2GameDir.ps1` pattern. **The current design (writing
real stats directly) has not been live-tested yet** -- an earlier design
(writing goal progress by name for every goal) WAS live-tested and
disproven; see below.

Menu (F9): 9 category submenus, each showing a live "Rank X / Y" status
line plus "Advance Rank" and "Complete Challenge" actions.

### The real goal registry -- three passes to get here

1. An exhaustive grep of all ~1,638 decompiled singleplayer scripts under
   `Scripts\1491.50\script_rel` found only 18 goal-hash strings anywhere
   in script text -- a real ceiling on what's recoverable from *scripts*
   (confirmed two independent ways: a literal-string search, and a full
   enumeration of every `STATS::CHAL_*` native call site).
2. The user extracted `challenges_sp.meta`/`goals_sp.meta` from disk and
   handed them over. Built entirely against them: every goal (regardless
   of its normal tracking mechanism) was force-completed via
   `STATS::CHAL_SET_GOAL_PROGRESS_INT(chalHash, joaat(goalName),
   desiredGoal)` -- the goal's own name taken directly as `goalHash`,
   the same generic native the real game already uses in scripts for a
   few goals it manually credits. Builds clean, **live-tested, and
   DISPROVEN**: 3 for 3 `AdvanceRank` attempts (Bandit rank 2, Explorer
   rank 5, Gambler rank 6) applied without error but never moved
   `CHAL_GET_NUM_RANKS_COMPLETED`. Investigating why led to the real
   problem: those two files were not vanilla at all -- their own header
   comment admitted it ("Gambler #2-10 auto complete") -- someone had
   already rewired several goals' `scoreParam` to check unrelated,
   trivially-true stats (Gambler rank 6's "win blackjack at Rhodes" goal
   was pointed at `TOTAL_PLAYING_GAME_TIME >= 10`). The user called this
   correctly ("BS from a mod") before more time went into the wrong data.
3. **Real files pulled directly from the game's own RPFs** using
   `..\external-tools\RDR2-RPF-Tool`, whose headless CLI mode
   (`HeadlessQuery.cs`) got a read-only load path added this session
   (`RPF8.LoadReadOnly`, `FileAccess.Read`+`FileShare.ReadWrite`) so it
   can search/extract while RDR2.exe has the RPF open. The real files
   live in `update_1.rpf` (checked *first*, per instruction -- later
   patches don't touch this path) at
   `common/data/stats_and_challenges/{goals,challenges}_sp.meta` -- not
   the base `data_0.rpf`. Both real files are archived at
   `D:\Backup\Stuff\RDR2 Shit\{goals,challenges}_sp.meta`; the fake ones
   are kept as `*_FAKE_MOD.meta` for reference. The real data changes the
   picture: only 10 of 138 goals are genuinely `StatsGoalScoreSourceScript`
   (the ones `CHAL_SET_GOAL_PROGRESS_INT`-by-name is actually correct
   for), not the small number the fake file implied were the exception --
   the other 102 covered goals are real `StatsGoalScoreSourceStat`
   entries with sane stat names (`KILLED`/`AT_RABBIT`,
   `LONGEST_DIST_DRAGGED`, `HOLD_UPS_IN_TOWN`, ...) that line up exactly
   with the plain-English challenge descriptions the user separately
   supplied (e.g. Horseman rank 4's `LONGEST_DIST_DRAGGED=1006` for
   "drag a victim 3,300 feet").

### Current mechanism (per goal, from real data)

- **102 `StatsGoalScoreSourceStat` goals**: increment the REAL named
  stat(s) directly via `STATS::_STAT_ID_INCREMENT_INT`/`_FLOAT` against a
  `StatId{BaseId, PermutationId}` built from each goal's real stat name(s)
  -- the same natives real script call sites use (`dominoes_sp.ysc.c`'s
  `func_606`, etc.). `STAT_ID_SET_INT`/`_FLOAT` was tried first and
  failed a live test (readback showed the write didn't even land);
  switching to the INCREMENT variant fixed it -- CONFIRMED LIVE
  2026-09-17, see "Current status" below.
- **10 `StatsGoalScoreSourceScript` goals**: `CHAL_SET_GOAL_PROGRESS_INT`
  by the goal's own name -- this IS the mechanism the vanilla game itself
  uses for these specifically (confirmed by `pause_menu.ysc.c`'s own
  `func_351` doing exactly this for `ACW_EXPL_Rank_01_Treasure`).
- **26 unsupported goals** (10 compendium, 9 item-collection list, 4
  "reach threshold on N different stats in a named group", 3 timed
  point-to-point rides with no `<scoreParam>` at all): no known
  mechanism yet. See the coverage table below.

`src/ChallengeCheat.cpp`'s `kKnownWrites` table (208 rows) was generated
by parsing both real meta files with Python (`xml.etree.ElementTree`):
`challenges_sp.meta`'s `<ranks><Item>` gives rank order and real goal
names; `goals_sp.meta`'s `<scoreParam>` tree is walked recursively per
goal, descending through wrapper types (`AIConditional`/`Binding`/
`Resetable`/`IntSum`/`IntGroupSum`) to emit one write row per
`StatsGoalScoreSourceStat` leaf (a sum goal's every contributing stat
gets pushed to the FULL combined target -- a safe overshoot) or one
`ScriptGoal` row for a `StatsGoalScoreSourceScript` leaf. See
`ChallengeCheat.cpp`'s own file header comment for the full derivation
trail.

**Coverage** (first blocking rank if not full 1-10, given a `Linear`
challenge type is assumed to require sequential completion -- itself
still unverified):

| Category | Coverage |
| --- | --- |
| Bandit | full 1-10 |
| Gambler | full 1-10 |
| Survivalist | full 1-10 |
| Weapons Expert | full 1-10 |
| Explorer | blocked at rank 2 (item-collection list) |
| Sharpshooter | blocked at rank 2 (mixed group-stat) |
| Master Hunter | blocked at rank 3 (group-stat) |
| Horseman | blocked at rank 3 (timed ride, no scoreParam) |
| Herbalist | blocked at rank 6 (group-stat) |

**CONFIRMED LIVE (2026-09-17): writing the real stat works.** First
attempt used `STATS::STAT_ID_SET_INT`/`_FLOAT` and failed -- readback
after the write showed the value hadn't even landed via that native. The
real script call sites (`dominoes_sp.ysc.c`'s `func_606`, etc.) only ever
use `_STAT_ID_INCREMENT_INT`/`_FLOAT` for progress, never the `SET`
variant -- switching to `INCREMENT` fixed the stat write itself (readback
confirmed the value landing and accumulating across calls). The rank
counter (`CHAL_GET_NUM_RANKS_COMPLETED`) still didn't move in the SAME
tick as the write, which looked like a second failure -- but a live
in-game check showed the real Progress/Challenges screen DID complete
the rank a moment later once the player waited, meaning the challenge
system re-evaluates on a short delay, not synchronously. `AdvanceRank()`
now polls for up to ~3 seconds (`WAIT(500)` x6) before concluding a write
didn't take, instead of checking once immediately.

**Live-tested end to end on 3 categories**: Bandit ranks 3-10 all
advanced correctly via the stat-increment path (8 consecutive rank
transitions). Gambler and Herbalist rank-6-and-up goals initially failed
the same way `CHAL_SET_GOAL_PROGRESS_INT` failed originally -- same
SET-vs-ADD split as the stat natives -- switched the `ScriptGoal` write
path to `CHAL_ADD_GOAL_PROGRESS_INT`, both categories then worked.

**CONFIRMED LIVE (2026-09-17): AIConditional wrappers are a REAL runtime
gate, not just descriptive.** Horseman rank 1
(`ACW_HORSE_Rank_01_Rabbits`) wraps its `KILLED`/`AT_RABBIT` stat check
in `StatsGoalParamAIConditional` / `CAIConditionIsOnMount`. Writing the
stat directly did nothing -- the goal only completed once the player
was actually on horseback at the moment of the write. Scanned all of
`goals_sp.meta` for every `AIConditional` wrapper's `<condition
type="...">` (not just the one that failed) and found exactly 3 kinds:
`CAIConditionIsOnMount` (4 goals, all Horseman: ranks 1, 4, 7, 8),
`CAIConditionGoalContext` (Sharpshooter rank 3 -- `CHAL_CTX_ON_MOVING_TRAIN`,
and Master Hunter rank 3, already unsupported for other reasons), and
`CAIConditionPlayerIsDeadeyeActive` (Sharpshooter rank 9). Only
`OnMount` has a simple native check (`PED::IS_PED_ON_MOUNT(PLAYER::PLAYER_PED_ID())`)
-- `AdvanceRank()` now checks this BEFORE writing anything for a rank
that needs it, and returns a specific on-screen message ("You must be
on horseback...") via `GetLastAdvanceFailureReason()` instead of a bare
failure. The other two condition types (moving train, Dead Eye active)
have no native check wired up yet -- the write is attempted anyway with
just a log NOTE, so those may still silently fail if the condition isn't
met; no native for either has been identified.

Still open: whether the real reward (item/recipe/cosmetic) is actually
granted alongside a cheat-driven rank completion, not just the rank
counter and pause-menu display -- not yet independently confirmed.

## Coding conventions

Same as PokerCheat/BlackjackCheat/DominoCheat's own: no C-style casts, no
C-style strings/buffers. Logging goes through `Log::Write` (spdlog,
fmt-style `{}` placeholders, compile-time checked).

## Build & deploy

```
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" ChallengeCheat.vcxproj /p:Configuration=Release /p:Platform=x64 /nologo /v:minimal
```

`PostBuildEvent` auto-locates the RDR2 install directory
(`BuildTools\Find-RDR2GameDir.ps1`) and copies the built `.asi` straight
in, every build regardless of whether the build itself was up to date.
`DisableFastUpToDateCheck` is set in `.vcxproj.user` so this also holds
for Visual Studio IDE builds. **RDR2.exe must be closed first** or the
copy fails with a file-in-use error.

A `Debug|x64` configuration also exists -- `/MTd` static debug CRT,
optimizations disabled, PDB deployed alongside the `.asi`. The menu and
Advance/Complete actions are identical in both configurations; there is
currently no Debug-only tooling in this mod at all (no raw script-memory
reading is needed -- everything goes through stock `STATS::` natives).

**F9** opens the menu (NUMPAD 8/2 move, NUMPAD 5 select, NUMPAD
0/Backspace/F9 back) -- chosen so PokerCheat (F10), BlackjackCheat (F11),
DominoCheat (F12), and this mod (F9) can all be loaded into the game at
once with no key collision.

Runtime log: `<game folder>\ChallengeCheat.log`.

## Source layout

Deliberately minimal -- this mod never reads raw script/game memory, only
calls stock `STATS::CHAL_*` natives, so it needs none of the
scrThread-pool/AOB-scanning/INI-config infrastructure the sibling
advisor mods carry (an earlier pass of this file did include that
infrastructure preemptively; it was removed once it became clear nothing
in this mod actually used it).

- `src/main.cpp` -- `DllMain`, registers `ScriptMain` and the keyboard
  handler (both configurations -- see this file's own header comment for
  why, unlike its siblings).
- `src/script.h`/`script.cpp` -- entry point and the F9 menu shell: 9
  category submenus (built in a loop over `ChallengeCheat::Category`),
  each with a live `MenuItemLabel` rank readout plus `MenuItemActionStatus`
  Advance/Complete actions that surface a short on-screen result via the
  menu controller's status-text popup (full detail always also goes to
  the log). On failure, prefers `ChallengeCheat::GetLastAdvanceFailureReason()`
  for the popup text when it's non-empty (currently only set for the
  "requires being on horseback" gate) over the generic "see log" message.
- `src/ChallengeCheat.h`/`.cpp` -- the actual logic. **Its file header
  comment is the single most important thing to read** -- the full
  research trail (native surface, root hashes, how the real goal
  registry was found and parsed, the `kKnownWrites` table itself, and
  the `Requirement` enum for goals with a live AIConditional gate).
- `src/scriptmenu.h`/`.cpp` -- vendored from PokerCheat/BlackjackCheat/
  DominoCheat with the F9 toggle key, plus two ChallengeCheat-specific
  additions kept in this file per its own established precedent:
  `MenuItemLabel` (a read-only row whose caption is recomputed from a
  callback every draw -- the live rank readout) and
  `MenuItemActionStatus` (like `MenuItemAction`, but the callback returns
  a result string shown via the transient status-text popup).
- `src/keyboard.h`/`.cpp` -- vendored unchanged from the sibling projects.
- `src/Log.h` -- file logger (`ChallengeCheat.log`), vendored unchanged
  in structure.
- `src/ExtraNatives.h` -- empty stub, same convention as the sibling
  projects. Not needed -- every native this mod calls is already
  declared in the stock `external\ScriptHookSDK\inc\natives.h`.
- `external\RDR-Classes\rage\joaat.hpp` -- the only piece of RDR-Classes
  this mod vendors (`rage::Joaat()`, used to hash every goal/stat/root
  name into the `Hash` values the natives take). `external\inipp\` and
  the RDR-Classes `script\`/`atArray.hpp` headers were removed after the
  Config/GamePointers/PatternScan modules that used them were deleted as
  dead weight.

## External resources

- `D:\Backup\Stuff\RDR2 Shit\challenges_sp.meta`,
  `D:\Backup\Stuff\RDR2 Shit\goals_sp.meta` -- **the real goal registry**,
  extracted from `update_1.rpf`'s
  `common/data/stats_and_challenges/{challenges,goals}_sp.meta` via
  `RDR2-RPF-Tool`'s headless CLI. This is the primary source of truth for
  this mod -- re-run the extraction (not a script search) if a future
  game build changes any goal name, stat, or target. The corresponding
  fake/modded copies the earlier design was wrongly built against are
  kept alongside as `challenges_sp_FAKE_MOD.meta`/`goals_sp_FAKE_MOD.meta`
  purely as a cautionary reference -- never parse these for real data.
- `..\external-tools\RDR2-RPF-Tool` -- the RPF extraction tool used
  above. `HeadlessQuery.cs`/`Core\RPF8.cs`'s `LoadReadOnly` (added this
  session) is what makes `--search`/`--list`/`--extract` work while
  RDR2.exe has the target RPF open. Usage:
  `RDR2RPFTool.exe --search "<rpf path>" <substring>` /
  `--extract "<rpf path>" "<entry name>" "<out file>"`. Only top-level
  entries are reachable (no nested-archive navigation) -- the game's own
  content is split across many small top-level RPFs
  (`update_1.rpf`..`update_4.rpf`, `data_0.rpf`, etc.) each containing
  further nested `.rpf` archives as entries, so finding a specific loose
  file means searching each top-level RPF in turn.
- `D:\Backup\Stuff\RDR2 Shit\Scripts\1491.50\script_rel\pause_menu.ysc.c`
  -- source of the confirmed category root-hash switch (~line 7147).
- `D:\Backup\Stuff\RDR2 Shit\Scripts\1491.50\script_rel\dominoes_sp.ysc.c`
  -- source of the `func_604`/`func_606` `_STAT_ID_INCREMENT_INT` pattern
  this design's stat-write mechanism is modeled on (`func_604(joaat(
  "WINS"), joaat("dominoes_no_draws"))` confirmed BOTH `StatId` fields
  are independently joaat-hashed strings, matching this mod's own
  construction).
- `..\Decompiler\rdr3-nativedb-data\natives.json` -- independent
  RDR2-specific native hash/name/param database; confirms every
  `STATS::CHAL_*`/`STAT_ID_*` native's exact signature and build number.

## Next concrete step

1. **Confirm real reward-grant, not just the rank counter/pause-menu
   display**: Bandit ranks 3-10, Gambler, and Herbalist have all been
   live-confirmed to advance the rank counter correctly, but whether the
   actual reward (item/recipe/cosmetic) is granted alongside a
   cheat-driven completion hasn't been independently checked yet.
2. Live-test Survivalist and Weapons Expert (the other two categories
   with full 1-10 coverage) and Horseman past rank 8 (ranks 1, 4, 7, 8
   need the player mounted -- now gated and message-prompted; ranks 2, 5
   are plain stats; rank 3, 6, 9 remain hard-blocked timed rides).
3. Check the categories that hit a real ceiling (Explorer at rank 2,
   Master Hunter at rank 3, Sharpshooter -- rank 3 needs the
   `OnMovingTrain` condition which isn't gated yet, rank 9 needs
   `DeadeyeActive` similarly ungated) stop exactly there rather than
   silently skipping past the unsupported/ungated one -- confirms the
   "Linear means sequential" assumption this whole ceiling table rests
   on.
4. Find native checks for the two remaining live `Requirement` types
   (`OnMovingTrain` -- `CHAL_CTX_ON_MOVING_TRAIN`, and `DeadeyeActive`)
   so Sharpshooter ranks 3 and 9 get the same pre-check/message treatment
   `OnMount` already has, instead of just a log NOTE and a possibly-silent
   failure.
5. Growing coverage past the 26 unsupported goals needs: for the 9
   Explorer item-collection-list goals, finding the native that marks a
   named collectable item as found; for the 4 group-stat goals, finding
   where a named stat GROUP's membership list is defined (not in either
   real meta file); for the 10 Horseman compendium goals, finding the
   compendium-entry-completion native; for the 3 Horseman timed rides,
   there may be no persisted-state mechanism to write at all (they're
   evaluated live against player position/time, per
   `StatsGoalPointToPoint`'s own `<startLocation>`/`<finishLocation>`/
   `<condition>` fields).
