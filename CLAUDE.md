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

Menu (F9): 9 category submenus, each showing a live "<Challenge Name> X /
10" status line plus "Advance <Challenge Name> N" and "Complete <Challenge
Name> N" actions, where N is the next rank a click would target (e.g.
"Advance Bandit 4") -- renamed 2026-09-19 from the generic "Rank X / Y" /
"Advance Rank" / "Complete Challenge" so the player always sees which
challenge and rank they're acting on without needing to recall which
submenu they're in.

**Advance/Complete semantics redefined (2026-09-19).** Originally "Advance
Rank" pushed a rank's goal(s) straight to their full target in one call
(completing that whole rank), and "Complete Challenge" looped that same
call until the category hit its max rank (all 10 at once). Live testing
showed both halves of this were wrong: jumping straight to rank 10 in one
menu click is not what the user wants -- they want to advance their
CURRENT in-progress goal one real step at a time (e.g. "killed 1 more
rabbit" toward a "kill 4 rabbits" goal), the same granularity a real
playthrough would produce. So the mapping flipped: **"Advance Rank" now
applies one small step** (`WriteMode::Step` -- e.g. +1 to a stat, +1
script-goal progress, +1 collectable found) that usually does NOT
complete the rank by itself; **"Complete Challenge" now does what the old
"Advance Rank" did** -- push the current rank's goal(s) to their full
target in one call, completing just that ONE rank (never loops across
multiple ranks). `WriteKind::PointToPointHook` and `HorseCompendium` have
no meaningful sub-unit to step through (each is one atomic, all-or-
nothing native call), so both modes apply identically for those two kinds
only. See `ChallengeCheat.cpp`'s `ApplyRankProgress`/`WriteMode` and
`ChallengeCheat.h`'s `AdvanceRank()`/`CompleteChallenge()` doc comments.

**Also fire-and-forget now, not polling.** The original design polled
`CHAL_GET_NUM_RANKS_COMPLETED` for up to ~3 seconds after every write and
reported on-screen failure if the counter hadn't moved yet. Live testing
(2026-09-19) showed this was actively wrong, not just slow: writes that
were genuinely landing and completing the rank (visible on screen a
moment later) still got reported as "could not advance," because the
challenge system's own background re-evaluation regularly takes longer
than the poll window -- and a Step-mode write is frequently not even
*trying* to complete the rank in one call, so a rank-counter check was
never the right success signal for it either. Both actions now apply
their write(s) and return immediately without polling. The menu shows
NO on-screen status text on success (`MenuItemActionStatus` only pops up
non-empty results) -- a message only ever appears when the action
genuinely could not be attempted at all: a live requirement gate (not on
horseback / not riding a train), the category already at max rank, or no
known write for that goal yet. Full detail always still goes to the log
regardless.

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
- **26 goals with no `<scoreParam>`-based mechanism at design time** (10
  compendium, 9 item-collection list, 4 "reach threshold on N different
  stats in a named group", 3 timed point-to-point rides). Three of these
  four buckets are now resolved: the 3 timed point-to-point rides via
  `TimedRideHook` (see below), 9 of the 10 compendium goals -- all of
  Horseman rank 10's required breeds -- via `WriteKind::HorseCompendium`
  spawning a breed ped and calling `COMPENDIUM::COMPENDIUM_HORSE_WILD_BROKEN`
  on it (see the CONFIRMED LIVE section below), and (2026-09-18) the 9
  Explorer item-collection-list goals (`ACW_EXPL_Rank_{02..10}_Treasure`,
  all `StatsGoalParamIntGroupSum` over the same 18-item
  `StatsGoalScoreSourceGroupCollectableList` pool) via
  `WriteKind::Collectable`, which calls
  `COLLECTABLE::_COLLECTABLE_INCREMENT_NUM_FOUND` -- the exact native
  `treasure_hunter.ysc.c` (~line 218) calls once per real loot pickup --
  directly on a distinct pool item per rank. The 10th compendium goal
  (`ACW_HORSE_Rank_10_Arabian`) is commented out in the real
  `challenges_sp.meta` and isn't required by any live rank, so it's moot.
  (The 4 group-stat goals were later resolved -- see the 1.0 status below.)

`src/ChallengeCheat.cpp`'s `kKnownWrites` table (226 rows) was generated
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
| Bandit, Explorer, Gambler, Herbalist, Horseman, Master Hunter, Sharpshooter, Survivalist | full 1-10, **all CONFIRMED LIVE via Advance (1.0)** |
| Weapons Expert | full 1-10, live-tested (1.0) |

**1.0 status (2026-09-18): every category covers ranks 1-10 and was
live-tested.** The group-stat gaps in the older text below were closed:
Herbalist 6/9 (15 distinct / all 43 herbs, `PICK/HERB_*`, names from
`beat_friendly_outdoorsman.ysc.c`'s `func_363`), Master Hunter 3 (12
`TRACKED/AT_<animal>`, gated on `CHAL_CTX_SCOPED_KIT` via
`SCRIPT::_IS_GOAL_CONTEXT_ACTIVE`), Sharpshooter 2 (`KILLED/AT_<animal>`
distinct + `KILLS/DEADEYE` bind, Dead Eye gate). Animal/herb group
membership isn't in either meta file; the names are informed guesses that
worked live.

**Write kinds beyond plain `Stat`** (all in `kKnownWrites`; pick by how
the goal's real `<scoreParam>` behaves, not by guessing):
- `StatSum` -- IntSum (completeAll=false): Step credits only the goal's
  first row per click (+1 on every row would be +N total). Rows that only
  BIND/gate a goal (e.g. Hunter 6's `KILLS/SN_BOW`) stay plain `Stat` so
  Step always writes them -- `CHECK_FOR_SCORE_WHEN_BIND_PROGRESS` needs the
  bind stat to move.
- `StatDistinct` -- goals that count DISTINCT items (group sum of a
  capped template, or IntSum completeAll=true): Step credits one item per
  click via an in-memory per-goal counter (not persisted; Complete finishes
  after a restart). The goal only counts picks made after it activated, so
  never skip a stat because its current value is already >= 1.
- `StatAtOnce` -- records / expiring reset windows (biggest fish, Weapons
  Expert 6): no usable partial step, Step applies the full value like
  Complete.
- `HorseCompendium` Step credits one breed per click, preferring the
  game's own `CHAL_IS_GOAL_ACTIVE` per-breed state, else a counter.
- Bind behavior `CHECK_FOR_SCORE_WHEN_BIND_NOT_PROGRESS` (Sharpshooter
  1/3): the bind stat (`KILLED/AT_BAT`) must NOT be written.
- Advance with nothing left to credit returns a visible failure reason
  instead of silently doing nothing.

**Category unlock:** on a new game a category root is hidden/locked and its
goals inactive (`CHAL_IS_GOAL_ACTIVE` false), so writes do nothing.
`ApplyRankProgress` calls `UNLOCK_SET_VISIBLE`/`UNLOCK_SET_UNLOCKED` on the
root first (what story scripts do) and waits 250 ms.

**`TimedRideHook` lifetime (1.0):** `Arm()` on the button press, stays armed
until the engine's check fires for the target RDX (or 15 s), then
`Update()` (every script tick) disables + removes + uninitializes on the
script thread. The old RAII scope tore it down ~35 ms after the click,
before the engine's periodic pass -- broke Horseman 3/6/9 once Advance
became fire-and-forget. Live-confirmed 3/6/9 individually.

**Live gates:** OnMount, OnMovingTrain, DeadeyeActive (must be aiming with
Dead Eye already on -- scripted activation drops right off; never toggle
it, Sharpshooter 2/9 reset on activation changes), ScopedKit.

Every `ApplyRankProgress` also logs the rank counter and each goal's
`CHAL_IS_GOAL_ACTIVE` before writing -- the first thing to read when a
rank "does nothing".

**Horseman ranks 3/6/9 (`ACW_HORSE_Rank_{03,06,09}_TimedRide`) -- no
native/script mechanism, but a real one found anyway (2026-09-17).**
These are `StatsGoalPointToPoint` goals -- a ride between two named
regions (rank 3: Valentine -> Rhodes, 300s; rank 6: Strawberry -> Saint
Denis, 540s; rank 9: Van Horn -> Blackwater, 1020s), on mount and not
following the game's own scripted road/rail route, with NO `<scoreParam>`
in `goals_sp.meta` at all -- structurally a different goal shape than
every other `StatsGoal`-based entry in this mod. Reversed all 1,638
decompiled SP scripts for these goal names, `PointToPoint`, `TimedRide`,
and both region volume names each rank uses: zero hits anywhere -- no
script starts, stops, or reacts to this goal in any way. Every documented
native lever was also tried and disproven, including
`CHAL_ADD_GOAL_PROGRESS_INT(chalHash, joaat("ACW_HORSE_Rank_03_TimedRide"), 1)`
(same generic native that works for real `StatsGoalScoreSourceScript`
goals) -- applied twice, rank counter read 2 both before and after both
attempts. `StatsGoalPointToPoint` doesn't consult the generic
goal-progress register that native writes to, and its completion is
evaluated entirely by native engine code with no script or documented
native anywhere near it.

The user then took a different approach: built an LML mod overriding
`goals_sp.meta` to replace rank 3's `durationSeconds` (an int, inferred
from the schema's float-formatting convention -- floats always emit
`.000000`, this field never does) from `300` to a distinctive marker
value (`13371337`, chosen to fit `int32` safely and be essentially
impossible to collide with naturally-occurring game data), then used
Cheat Engine's "find out what accesses this address" on the live-resident
value. That led directly to `sub_140BAC640` (RDR2.exe+0xBAC640 in the
analyzed build). Verified byte-for-byte against the user's own IDA
database (`D:\Backup\Stuff\RDR2 Shit\EXEs\1491.50\RDR2_Dumped.exe.i64`,
build 1491.50 -- read via `idat.exe` headless/batch mode + an IDAPython
script, on an isolated scratch copy, never the live file) that the
function's real shape is:

```c
if ( !a1[3].m128i_i64[0] || !a1[3].m128i_i64[1] )   // two null checks, offsets 0x30/0x38 from a1
    return (char)sub_140B9842C(a1);                   // fallback, no tracking context
return a1_vtable[0x210](a1);                          // real per-goal check (virtual call)
```
i.e. two `cmp qword ptr [rcx+30h],0` / `[rcx+38h],0` checks, each
followed by `jz loc_140BAC929` (the fallback-and-return path), gate
whether a virtual call through `a1`'s own vtable at `+0x210` -- almost
certainly the actual per-goal evaluator -- ever runs at all.

**First fix attempt (superseded): a raw jz->jmp byte patch, with a real
bug.** The user's first live-confirmed fix converted the FIRST `jz` (at
RDR2.exe+0xBAC65E) into an unconditional jump, forcing the fallback path
regardless of whether that pointer is actually null -- a hand-encoded
2-byte opcode flip (`0F 84` -> `90 E9`), scoped via RAII to only the few
seconds `AdvanceRank()` was polling. This DID complete Horseman rank 3
live. But it had a real bug: advancing rank 3 was later observed to also
silently complete ranks 6 and 9, with no hook ever installed for them
specifically (architecturally impossible for the byte-patch version to
have touched them, since it only ever installed while `AdvanceRank()`'s
`targetRank` was exactly 3, 6, or 9 in turn). The user correctly
suspected the patch itself was the cause, not coincidental timing.

**Root cause, confirmed via a pure-logging MinHook detour (zero effect on
behavior, just observed arguments):** `sub_140BAC640` isn't called once
per `AdvanceRank()` attempt -- it's called once per PointToPoint goal
INSTANCE during a single shared periodic re-evaluation pass. Three calls
were logged back-to-back in the exact same tick, one per goal, each with
a different `a1` (goal-state struct pointers ~0x70 bytes apart) and,
critically, a different RDX: `0x161`/`0x164`/`0x167` for ranks 3/6/9
respectively -- consistently spaced by 3, a per-goal index/slot IDA's own
decompile of `sub_140BAC640` never showed (it only recognized `a1`/RCX,
but the Microsoft x64 ABI always passes arg2 in RDX regardless of
whether the callee's own code visibly reads it -- declaring a second
parameter on the detour observes it for free, no inline asm needed,
which MSVC doesn't support on x64 anyway). The raw byte patch had no way
to tell these three calls apart, so it forced success for whichever
goal(s) happened to be evaluated during its brief active window -- often
more than the one requested.

**Current fix: a selective MinHook detour, not a byte patch.**
`src/TimedRideHook.h`/`.cpp` (MinHook vendored as a real git submodule at
`external/minhook`) hooks `sub_140BAC640` at its true entry point and
inspects RDX itself. Only when RDX matches the specific goal currently
being targeted does it short-circuit to `return (char)sub_140B9842C(a1)`
-- exactly replicating the original function's own fallback behavior,
just selectively, via `sub_140B9842C`'s own address (also located by AOB
signature, extended far enough into the function body to be unique on
its own -- a shorter attempt at this signature matched 3 different
functions in the image). Every other call (any other RDX, or no target
set at all) passes straight through to the real evaluation, completely
unaffected -- so advancing rank 3 can no longer touch ranks 6/9. Both
signatures confirmed unique (exactly 1 match each) across the whole
~115MB image via the same `idat.exe` + IDAPython scan used throughout
this investigation. The hook itself (`TimedRideHook::EnsureInstalled()`)
is installed once and can stay installed for the mod's whole lifetime --
it's a no-op unless a `TimedRideHook::ScopedTarget` is currently alive;
only the target RDX is scoped (via RAII, same pattern as before) to the
few seconds `AdvanceRank()` is polling for one specific rank.
`WriteKind::PointToPointHook` rows in `kKnownWrites` store their target
RDX in the `value` field (353/356/359 decimal = 0x161/0x164/0x167).
`TimedRideHook::Uninstall()` is called from `main.cpp`'s `DllMain` on
`DLL_PROCESS_DETACH` -- fully disables/removes the hook and calls
`MH_Uninitialize()`, since MinHook is a process-wide library that
shouldn't outlive this ASI's own load. Skipped on process exit (non-null
`lpReserved`), when every other thread is already gone.

**CONFIRMED LIVE (2026-09-17 night, pre-refactor): the selective
`TimedRideHook` works through the mod's own `AdvanceRank()`.** Horseman
ranks 1-10 were all tested live end-to-end with the hook in place,
including ranks 3/6/9 individually -- the "completes 6/9 for free" bug
from the byte-patch version is fixed. **Not yet re-tested since the
`04504c7` "Improve challenge code maintainability" refactor commit** --
the core mechanism (RDX-selective detour, ScopedTarget RAII) is
unchanged by that refactor, so this is expected to still work, but it
hasn't been re-confirmed in game post-refactor.

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
was changed at the time to poll for up to ~3 seconds (`WAIT(500)` x6)
before concluding a write didn't take, instead of checking once
immediately. **That polling was itself removed 2026-09-19** -- see
"Advance/Complete semantics redefined" above -- once live testing showed
even 3 seconds wasn't reliably enough and the whole approach was fighting
an async system instead of just trusting it; both actions are
fire-and-forget now.

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
`CAIConditionPlayerIsDeadeyeActive` (Sharpshooter rank 9). `OnMount` has
a simple native check (`PED::IS_PED_ON_MOUNT(PLAYER::PLAYER_PED_ID())`)
-- `AdvanceRank()` checks this BEFORE writing anything for a rank that
needs it, and returns a specific on-screen message ("You must be on
horseback...") via `GetLastAdvanceFailureReason()` instead of a bare
failure. **`OnMovingTrain` got the same treatment 2026-09-18** via
`PLAYER::IS_PLAYER_RIDING_TRAIN(PLAYER::PLAYER_ID())` (found in
`natives.h`, documented inline as "Returns true if the player is riding
a train") -- gates Sharpshooter rank 3 the same way `OnMount` gates its
own ranks, with its own on-screen message. Builds clean; not yet
live-tested. `DeadeyeActive` (Sharpshooter rank 9) still has no native
check wired up -- the write is attempted anyway with just a log NOTE, so
it may still silently fail if the condition isn't met; no native for it
has been identified yet.

**CONFIRMED LIVE (2026-09-19): Horseman rank 10's 9 horse-breed compendium
goals, via `WriteKind::HorseCompendium`.** Rank 10 requires ALL 9 of
`ACW_HORSE_Rank_10_{American_Paint,American_Standardbred,Appaloosa,
Hungarian,Kentucky,Morgan,Mustang,Nokota,Tennessee}` (`Arabian` is
commented out in the real `challenges_sp.meta`, so only 9 are actually
required) -- `kKnownWrites`' original generator never emitted a row for
these because each goal's only `<scoreParam>` leaf is a
`StatsGoalScoreSourceCompendium` (e.g. `CMPNDM_AMPAINT`/`CMPNDM_AMPAINT_RARE`
for the Paint goal), a leaf type the generator only handled for
Stat/Script. The real mechanism: `player_horse.ysc.c` (~line 6546) marks
a breed's compendium entry "broken" by calling
`COMPENDIUM::COMPENDIUM_HORSE_WILD_BROKEN(pedIndex)` on the wild horse
ped that was just tamed -- the native reads THAT ped's own model to
figure out which breed/entry to credit; it isn't told the breed
directly. `WriteKind::HorseCompendium` (and its `SpawnAndBreakCompendiumHorse()`
helper in `ChallengeCheat.cpp`) replicates this: spawns a short-lived,
no-longer-needed ped of the target breed's model a few meters from the
player, calls the native on it once, then deletes it before a render
frame -- the player never sees it appear. Live-tested end to end: 8 of 9
breeds worked on the first attempt.

**Mustang needed a second attempt -- a real "which coat counts as wild"
gotcha, not a bug in the mechanism itself.** The first Mustang `modelHash`
(`0x62121AEC`, decoded via joaat as `A_C_HORSE_MUSTANG_BUCKSKIN`) spawned
fine and the native call completed without error (logged), but
`CMPNDM_MUSTANG`/`CMPNDM_MUSTANG_RARE` never actually flipped -- silently
wrong, not a crash. Grepping all 1,638 decompiled SP scripts for every
Mustang coat name referenced in a wild-population switch/case showed
exactly 4: `a_c_horse_mustang_{grullodun,tigerstripedbay,goldendun,wildbay}`
-- Buckskin never appears among them anywhere. Buckskin is a real,
loadable, spawnable ped model (so `CREATE_PED` succeeded and gave no
indication anything was wrong) -- it's just not one of the coats the wild
ambient population (or `COMPENDIUM_HORSE_WILD_BROKEN`'s own internal
wild-coat table) ever generates for Mustangs, so the native's internal
model->breed/coat lookup had nothing to match it against. Switched the
row to `0x7E4DF66E` (`a_c_horse_mustang_wildbay`, one of the 4 real wild
coats) -- confirmed live working. **Lesson for any future breed/animal
goal that needs a spawned stand-in ped: the model must be a coat/variant
the game's own wild population actually uses, not just any valid model
of the right species/breed -- cross-check against a real script's own
switch-case coat list before picking one, not just against the breed
name.**

Still open: whether the real reward (item/recipe/cosmetic) is actually
granted alongside a cheat-driven rank completion, not just the rank
counter and pause-menu display -- not yet independently confirmed.

## Localization (game-sourced text)

Game-owned text (category names, the Dead Eye term) is the game's EXACT
per-language wording, not a translation. Every label in
`challenges_sp.meta`/`goals_sp.meta` (`challengeNameLabel`,
`rankDescLabel`, goal `*DescriptionLabel`, ... ~365 unique) is a GXT key
whose text lives in `global.yldb` inside each language's
`x64/data/lang/<lang>_rel.rpf` in `update_3.rpf` (checked first; `data_0.rpf`
has the base copies). `RDR2RPFTool --extract` the nested RPF, then the
`global.yldb` from it, into `D:\Backup\Stuff\RDR2 Shit\GXT\global_<lang>.yldb`.
`tools/dump_labels.py` parses those (a RAGE RSC8 memory dump: virtual base
0x50000000; each label is a node `{u32 joaat(label), pad, u64 cell}`; the
string record `{ptr,len}` is at `cell+0x20`, chars at `ptr+16`, len includes
the NUL) and writes `GXT\labels.json` -- all 365 labels x 13 languages
resolve with zero misses/ambiguities. **Pitfall:** an earlier version guessed
the record by "pair at node-0x10, else node-0x30" -- it worked for ~80% of
entries but silently returned a NEIGHBOR's string for the rest (e.g. French
rank 2 objective came back "Passager"). Always resolve via the cell pointer.
Cross-check: numbers in each `*_obj` text match English's (only spelled-out
numbers and Japanese "1日" differ). `tools/gen_localization.py`
emits `src/LocalizationData.h` (do not hand-edit). The menu uses
the category names, Dead Eye, and each category's next-rank objective
(`rankDescLabel`, shown by `MenuItemParagraph`, which word-wraps and grows its
row height; wrap width is the `[General] WrapWidth` INI setting (default 50; 0 = no wrapping; otherwise units per line), an ESTIMATE --
tune in-game). Goal descriptions (`*DescriptionLabel`, with `~1~` tokens) are
in `labels.json` but unused. NBSPs are converted to spaces. The Dead Eye label
name is unknown -- matched by text ("Dead Eye" hash 0x6788AE48 gives the real
term in all languages). Mod-own verbs/messages live in `Localization.cpp`
(LLM-assisted, unreviewed). Not yet verified in-game per language.

## Coding conventions

Strings: prefer `std::string_view` for read-only text (params, static tables,
`Localization::Text/CategoryName/RankObjective` returns). Per-frame code must
not allocate: `Localization::Text` is cached per `Refresh()`, and
`MenuItemParagraph` takes a `string_view` callback and copies only on change.
`g_distinctStepsCredited` is keyed by `rage::Joaat(goal name)`, not a string.
Tables holding views of literals (`kKnownWrites`) are safe as `string_view`
keys/set members.

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

Runtime log: `<game folder>\ChallengeCheat.log` -- or
`%LOCALAPPDATA%\RDR2ASIMods\ChallengeCheat.log` when the game folder isn't
writable (e.g. a C:\Program Files install; the file's first line then names
the rejected path). See `src/LogFallback.h`, vendored identically into every
sibling project.

`tests/LogFallbackTests.vcxproj` checks that logging falls back to
`%LOCALAPPDATA%\RDR2ASIMods\` instead of throwing when the game folder can't
be written (it points the logger at `C:\Windows\System32` -- skipped when run
elevated -- and at a path through a regular file). Same test, vendored into
every sibling project:

```
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" tests\LogFallbackTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
bin\Debug\LogFallbackTests.exe
```

## Source layout

Mostly minimal -- almost everything goes through stock `STATS::CHAL_*`
natives, so almost none of the scrThread-pool/AOB-scanning/INI-config
infrastructure the sibling advisor mods carry is needed (an earlier pass
of this file did include that infrastructure preemptively; it was
removed once it became clear nothing in this mod actually used it). The
one exception, added 2026-09-17: Horseman ranks 3/6/9's completion check
turned out to have no native or script surface at all (see
`ChallengeCheat.cpp`'s header comment) -- reaching it required vendoring
`PatternScan.h/.cpp` back in (AOB signature scanning, unchanged from
PokerCheat/DominoCheat's own copy) plus a new `TimedRideHook.h/.cpp` that
does a scoped binary patch of the actual native function.

- `src/main.cpp` -- `DllMain`, registers `ScriptMain` and the keyboard
  handler (both configurations -- see this file's own header comment for
  why, unlike its siblings).
- `src/script.h`/`script.cpp` -- entry point and the F9 menu shell: 9
  category submenus (built in a loop over `ChallengeCheat::Category`),
  each with a live `MenuItemLabel` rank readout plus `MenuItemActionStatus`
  Advance/Complete actions. Both the rank readout and the two action
  captions are named after the category (2026-09-19, e.g. "Bandit 3 / 10",
  "Advance Bandit 4", "Complete Bandit 4") instead of the generic "Rank X /
  Y" -- `MenuItemActionStatus` in `scriptmenu.h` now takes a caption
  CALLBACK (recomputed every draw, same as `MenuItemLabel`'s) instead of a
  fixed string, so the button label's rank number stays live as ranks
  complete. Fire-and-forget (2026-09-19, see "Advance/Complete semantics
  redefined" above): `DoAdvanceRank`/`DoCompleteChallenge` return an EMPTY
  string on success, which shows NO status popup at all
  (`MenuItemActionStatus::OnSelect` only calls `SetStatusText` for a
  non-empty result) -- full detail always still goes to the log. On
  failure they always return `ChallengeCheat::GetLastAdvanceFailureReason()`,
  which `ApplyRankProgress` now guarantees is non-empty on every failure
  path (already maxed, a live requirement gate, or no known write).
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
- `src/Config.h`/`.cpp` + `external/inipp` (git submodule) -- inipp-backed
  `ChallengeCheat.ini` next to the `.asi`, same load-and-rewrite pattern as
  the siblings. Release-visible (unlike theirs). One setting so far:
  `[General] MenuKey` (default `F9`), read by `MenuInput::MenuSwitchPressed`.
  Loaded lazily on first `Get()` from the script thread, NOT from `DllMain`
  (MenuKey parsing calls `VkKeyScanW`).
  **INI behavior:** `Config::Reload()` parses the file (missing file/key ->
  defaults), resolves each value, then REWRITES the file with the resolved
  values, so a bad `MenuKey` is corrected on disk and the file is created on
  first use. There is no hot reload -- `Reload()` runs once, on the first
  `Get()`; edits need a game restart. Any exception during load is logged and
  the previous values kept. To add a setting: add a field to `Config::Values`,
  read/validate it in `ReloadImpl()` (falling back to the default and logging
  on bad input), write the resolved value back to its section, and mirror it
  in the Nexus description's Configuration section and `docs/CHANGELOG.md`.
- `src/KeyNames.h`/`.cpp` -- keycap-style key name <-> VK parser for
  `MenuKey`: F1-F24, A-Z/0-9, NUMPAD*, named keys with keycap aliases
  (PAGEDOWN/PGDN, not VK_NEXT), and single unshifted punctuation chars via
  `VkKeyScanW` (active layout). Rejects keys the menu already uses for
  navigation (numpad 2/4/5/6/8, arrows, Enter); invalid values log a
  warning and fall back to F9. Note: NUMPAD keys need NumLock on.
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
  original Config/GamePointers/PatternScan modules that used them were
  deleted as dead weight -- `PatternScan` itself came back (see below).
- `src/PatternScan.h`/`.cpp` -- vendored unchanged from PokerCheat/
  DominoCheat's own copy: a minimal AOB (array-of-bytes) scanner over
  RDR2.exe's loaded image (`FindInMainModule`), plus RIP-relative operand
  resolution (`ResolveRip`, unused by this mod so far).
- `external/minhook` -- a real git submodule (`.gitmodules`, not vendored
  by copying like everything else under `external\`), pointed at
  TsudaKageyu/minhook. Used for one thing: a proper, trampoline-generating
  inline hook on `sub_140BAC640` (see `TimedRideHook` below) instead of a
  hand-encoded byte patch. Only `src/hook.c`, `src/buffer.c`,
  `src/trampoline.c`, and `src/hde/hde64.c` (+ their headers) are built --
  the x86-only `hde32.c` isn't needed for this x64-only project.
- `src/TimedRideHook.h`/`.cpp` -- a selective MinHook detour on
  `sub_140BAC640` (RDR2.exe+0xBAC640 in the analyzed build), the native
  function that gates Horseman ranks 3/6/9's completion. An EARLIER
  version of this file did a raw 2-byte opcode patch (`0F 84` jz -> `90
  E9` nop+jmp) on one conditional jump inside the function -- this worked
  live but had a real bug (advancing rank 3 silently completed ranks 6
  and 9 too, since the patch couldn't tell which of the 3 timed-ride
  goals a given call was for). The current version hooks the whole
  function via MinHook and inspects RDX (a per-goal index, live-confirmed
  via a logging-only detour: 0x161/0x164/0x167 for ranks 3/6/9), only
  forcing `sub_140B9842C(a1)`'s fallback result when RDX matches the
  specific goal being targeted (`TimedRideHook::ScopedTarget`, RAII-scoped
  to the few seconds `AdvanceRank()` is polling -- see
  `WriteKind::PointToPointHook` in `ChallengeCheat.cpp`, which stores each
  rank's target RDX in the `value` field). The hook itself
  (`TimedRideHook::EnsureInstalled()`) can stay installed for the mod's
  whole lifetime since it's a no-op unless a target is set.
  `TimedRideHook::Uninstall()` is called from `main.cpp`'s `DllMain` on
  `DLL_PROCESS_DETACH` to fully disable/remove the hook and call
  `MH_Uninitialize()`. See this file's own header comment for the full
  discovery trail (LML-planted marker value in `goals_sp.meta` -> Cheat
  Engine "find out what accesses this address" -> manual patch-and-verify
  -> the byte-patch's own "completes 6/9 for free" bug -> a logging-only
  MinHook detour explaining why (RDX) -> this selective version). Every
  AOB signature involved (the hook target, `sub_140B9842C`'s own address,
  and the original jz-anchored one from the superseded version) was
  confirmed unique across the whole ~115MB image via a headless
  `idat.exe` + IDAPython scan against the user's own IDA database, never
  opening the live file directly.

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

Everything in ranks 1-10 is live-tested as of 1.0. Remaining:

1. **Confirm real reward-grant**, not just the rank counter/pause-menu
   display -- whether the item/recipe/cosmetic is granted for a
   cheat-driven completion (biggest question for the timed-ride hook, which
   bypasses the engine's own completion check).
2. Make Advance's per-goal counters (`StatDistinct`, `HorseCompendium`)
   survive a game restart if that ever proves annoying (currently
   Complete covers it).
3. Consider `StatAtOnce` for other short-window reset goals if Advance
   feels wrong (Weapons Expert 2 has a 10 s window; Horseman 2 a 15 s
   one and was fine live).

## Releasing

Add a `## [X.Y.Z] - date` entry to `docs/CHANGELOG.md`, commit it, then push an
`X.Y` tag (`git tag -a X.Y -m "X.Y.Z - summary"`, `git push origin
master X.Y`). `.github/workflows/release.yml` then runs every unit test
project under `tests/`, builds the Release `.asi` on a GitHub Windows runner
(deploy step off), and publishes a GitHub release named "ChallengeCheat X.Y.Z" with
that changelog entry as its notes and the `.asi` attached. To release an
existing tag again, use "Run workflow" on the Actions tab and enter the tag.
The projects target toolset v145 (VS 2026); if the runner only has an older
Visual Studio, the workflow builds with v143 instead.
