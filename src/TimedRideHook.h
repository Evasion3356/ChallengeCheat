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
// when RDX matches the SPECIFIC goal currently being targeted (see
// ScopedTimedRideUnlock) does it call `sub_140B9842C(a1)` -- the original
// function's own unlock/fallback path -- then immediately disables the
// hook before returning that result. Every other call (any other RDX, i.e.
// any other goal, or no target set at all) passes straight through to the
// real function, completely unaffected.
namespace TimedRideHook
{
	using ChallengeState = void;
	using TimedRideGoalIndex = std::uint64_t;

	// __fastcall is a no-op on x64 (one calling convention only) -- kept
	// because these are native RDR2 function signatures recovered from IDA.
	using TimedRideChallengeCheckFn = char(__fastcall*)(ChallengeState* challengeState, TimedRideGoalIndex timedRideGoalIndex);
	using UnlockChallengeFn = std::uint64_t(__fastcall*)(ChallengeState* challengeState);

	// Fully removes the hook and uninitializes MinHook. Call exactly once,
	// on module unload (DllMain's DLL_PROCESS_DETACH) -- MinHook is a
	// process-wide library, not something to leave installed/initialized
	// past this ASI's own lifetime.
	void Uninstall();

	// RAII: constructing an instance locates/creates the MinHook hook if
	// needed, arms it for one timed-ride goal index, and enables it. The
	// first call to sub_140BAC640 whose observed RDX equals
	// `targetTimedRideGoalIndex` calls sub_140B9842C(a1), disables the hook,
	// and returns that result. Every other RDX value passes straight
	// through to the real function, unaffected. If no matching call arrives
	// before the instance is destroyed, the destructor clears the target and
	// disables the hook.
	class ScopedTimedRideUnlock
	{
	public:
		explicit ScopedTimedRideUnlock(TimedRideGoalIndex targetTimedRideGoalIndex);
		~ScopedTimedRideUnlock();

		bool IsReady() const;

		ScopedTimedRideUnlock(const ScopedTimedRideUnlock&) = delete;
		ScopedTimedRideUnlock& operator=(const ScopedTimedRideUnlock&) = delete;

	private:
		friend void Uninstall();

		struct Runtime;

		static Runtime& GetRuntime();
		static bool CreateTimedRideChallengeCheckHook();
		static bool EnableTimedRideChallengeCheckHook(TimedRideGoalIndex targetTimedRideGoalIndex);
		static void DisableTimedRideChallengeCheckHook(const char* reason);
		static char __fastcall TimedRideChallengeCheckDetour(ChallengeState* challengeState, TimedRideGoalIndex timedRideGoalIndex);

		bool m_ready = false;
	};
}
