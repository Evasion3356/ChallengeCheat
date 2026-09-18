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
| Horseman | full 1-10 (ranks 3/6/9 via native binary patch, see below) |
| Herbalist | blocked at rank 6 (group-stat) |

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
shouldn't outlive this ASI's own load.

**Not yet live-tested through the mod's own `AdvanceRank()` -- only the
byte-patch version (now superseded) was confirmed live end-to-end. The
selective version builds clean in both configurations but the actual
fix for the "completes 6/9 for free" bug has not yet been re-verified
in game.**

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

1. **Live-test the `TimedRideHook` binary patch through the mod's own
   `AdvanceRank()` flow** (Horseman ranks 3, then 6, then 9) -- so far
   only a manual debugger patch-and-verify has been confirmed live, not
   the actual `WriteKind::PointToPointHook` code path. Watch the log for
   "TimedRideHook: patched at..." / "...restored original bytes at..." to
   confirm the AOB signature still resolves to the right address and the
   scoped patch/restore cycle works cleanly (no leftover patched bytes if
   `AdvanceRank()` returns early for any reason).
2. **Confirm real reward-grant, not just the rank counter/pause-menu
   display**: Bandit ranks 3-10, Gambler, and Herbalist have all been
   live-confirmed to advance the rank counter correctly, but whether the
   actual reward (item/recipe/cosmetic) is granted alongside a
   cheat-driven completion hasn't been independently checked yet -- worth
   checking for a `TimedRideHook`-completed rank too, since bypassing the
   engine's own completion check entirely is a bigger leap than any other
   write this mod does.
3. Live-test Survivalist and Weapons Expert (the other two categories
   with full 1-10 coverage) and Horseman past rank 8 (ranks 1, 4, 7, 8
   need the player mounted -- now gated and message-prompted; ranks 2, 5
   are plain stats; ranks 3, 6, 9 use `TimedRideHook`, see above).
4. Check the categories that hit a real ceiling (Explorer at rank 2,
   Master Hunter at rank 3, Sharpshooter -- rank 3 needs the
   `OnMovingTrain` condition which isn't gated yet, rank 9 needs
   `DeadeyeActive` similarly ungated) stop exactly there rather than
   silently skipping past the unsupported/ungated one -- confirms the
   "Linear means sequential" assumption this whole ceiling table rests
   on.
5. Find native checks for the two remaining live `Requirement` types
   (`OnMovingTrain` -- `CHAL_CTX_ON_MOVING_TRAIN`, and `DeadeyeActive`)
   so Sharpshooter ranks 3 and 9 get the same pre-check/message treatment
   `OnMount` already has, instead of just a log NOTE and a possibly-silent
   failure.
6. Growing coverage past the remaining 23 unsupported goals needs: for
   the 9 Explorer item-collection-list goals, finding the native that
   marks a named collectable item as found; for the 4 group-stat goals,
   finding where a named stat GROUP's membership list is defined (not in
   either real meta file); for the 10 Horseman compendium goals, finding
   the compendium-entry-completion native. (The 3 Horseman timed rides
   are no longer in this bucket -- see `TimedRideHook` above.)
