#pragma once

#include <cstdint>

// TimedRideHook -- a selective MinHook detour on RDR2.exe's own internal
// completion-gate check for the 3 Horseman "timed ride" goals
// (ACW_HORSE_Rank_{03,06,09}_TimedRide, all StatsGoalPointToPoint -- no
// <scoreParam>, evaluated live by native engine code, no script or
// documented native anywhere near it). See ChallengeCheat.cpp's own file
// header comment for the full research trail.
//
// sub_140BAC640 (RDR2.exe+0xBAC640 in the analyzed build) is that
// completion check. Its real shape, confirmed directly against the user's
// own IDA database (2026-09-17):
//
//   if ( !a1[3].m128i_i64[0] || !a1[3].m128i_i64[1] )   // no tracking context
//       return (char)sub_140B9842C(a1);                 // fallback path
//   return a1_vtable[0x210](a1);                        // real per-goal check (virtual call)
//
// An EARLIER version of this file (2026-09-17, superseded) forced the
// first null-check's `jz` to an unconditional jump via a raw 2-byte
// opcode patch -- live-confirmed to complete Horseman rank 3, but with an
// unintended side effect: advancing rank 3 also silently completed ranks
// 6 and 9 later, with no hook ever installed for them specifically. A
// logging-only MinHook detour (this file's own prior version,
// TimedRideProbe) explained why: sub_140BAC640 is called once per
// PointToPoint goal instance during a SHARED periodic re-evaluation pass
// -- three calls observed back-to-back in the same tick, one per goal,
// each with a DIFFERENT `a1` and a different RDX value (0x161/0x164/0x167
// for ranks 3/6/9 respectively, live-confirmed 2026-09-17 -- consistently
// spaced by 3, strongly suggesting RDX is a per-goal index/slot). The old
// byte patch had no way to distinguish which call it was affecting, so it
// forced success for whichever goal(s) happened to be evaluated during
// its brief patch window -- often more than one.
//
// This version fixes that: instead of patching an instruction
// unconditionally, it hooks the whole function via MinHook (proper
// trampoline generation, not hand-rolled) and inspects RDX itself. Only
// when RDX matches the SPECIFIC goal currently being targeted does it call
// `sub_140B9842C(a1)` -- the original function's own unlock/fallback path.
// Every other call (any other RDX, i.e. any other goal, or no target set
// at all) passes straight through to the real function, unaffected.
//
// LIFETIME (fixed 2026-09-18): the engine only re-evaluates these goals on
// its own periodic pass, well after the menu click that requested the
// unlock has returned. An earlier version tied the hook's life to the
// AdvanceRank()/CompleteChallenge() call itself (RAII) and tore it down as
// that call returned (~35 ms later) -- before the engine ever called the
// check -- so nothing ever matched. Now: Initialize() (script start) does
// the signature scans and creates the hook disabled; Arm() on the button
// press just targets + enables it, and it stays armed until the detour has
// fired for its target (or a timeout expires); Update(), called every script
// tick, then DISABLES it from the script thread. The hook stays created
// until Uninstall(), so repeat clicks pay no scan/init cost. The detour
// itself never tears anything down -- freeing the trampoline it is still
// executing inside would be unsafe.
namespace TimedRideHook
{
	using ChallengeState = void;
	using TimedRideGoalIndex = std::uint64_t;

	// __fastcall is a no-op on x64 (one calling convention only) -- kept
	// because these are native RDR2 function signatures recovered from IDA.
	using TimedRideChallengeCheckFn = char(__fastcall*)(ChallengeState* challengeState, TimedRideGoalIndex timedRideGoalIndex);
	using UnlockChallengeFn = std::uint64_t(__fastcall*)(ChallengeState* challengeState);

	// Resolves both signatures (two full-image AOB scans), initializes
	// MinHook and creates the hook DISABLED. Call once from the script
	// thread at startup so the scan cost isn't paid on a button press.
	// Returns false if a signature wasn't found or MinHook failed; Arm()
	// retries in that case.
	bool Initialize();

	// Targets one timed-ride goal index and enables the (already created)
	// hook -- no scanning. Returns false (nothing left armed) if the hook
	// couldn't be created or enabled. Re-arming while already armed just
	// retargets.
	bool Arm(TimedRideGoalIndex targetTimedRideGoalIndex);

	// Call every script tick. Once the armed target has been unlocked, or
	// the timeout has passed, DISABLES the hook (it stays created for the
	// next Arm()). Cheap no-op when nothing is armed.
	void Update();

	// Full teardown (disable, remove, MH_Uninitialize). Call from DllMain's DLL_PROCESS_DETACH --
	// MinHook is process-wide and shouldn't outlive this ASI.
	void Uninstall();
}
