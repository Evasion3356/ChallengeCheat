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
// ScopedTarget) does it short-circuit to `return (char)sub_140B9842C(a1)`
// -- exactly replicating the original function's own fallback path, just
// selectively. Every other call (any other RDX, i.e. any other goal, or
// no target set at all) passes straight through to the real function,
// completely unaffected.
namespace TimedRideHook
{
	// Locates and hooks sub_140BAC640 (and resolves sub_140B9842C's own
	// address, needed by the detour) if not already done. Idempotent --
	// safe to call every time before use. Returns false if either AOB
	// signature isn't found or MinHook fails to create/enable the hook
	// (see the log either way); the hook has zero effect on any goal in
	// that case; callers should not count it as an applied write.
	bool EnsureInstalled();

	// Fully removes the hook and uninitializes MinHook. Call exactly once,
	// on module unload (DllMain's DLL_PROCESS_DETACH) -- MinHook is a
	// process-wide library, not something to leave installed/initialized
	// past this ASI's own lifetime.
	void Uninstall();

	// RAII: while an instance is alive, any call to sub_140BAC640 whose
	// observed RDX equals `targetRdx` gets short-circuited to
	// `return (char)sub_140B9842C(a1)`. Every other RDX value (including
	// while no ScopedTarget is alive at all) passes straight through to
	// the real function, unaffected -- so this only ever touches the one
	// specific goal being targeted, never "whichever goals happen to be
	// evaluated while this happens to be active" the way the old
	// unconditional patch did. Clears back to "no target" the moment the
	// instance is destroyed.
	class ScopedTarget
	{
	public:
		explicit ScopedTarget(std::uint64_t targetRdx);
		~ScopedTarget();

		ScopedTarget(const ScopedTarget&) = delete;
		ScopedTarget& operator=(const ScopedTarget&) = delete;
	};
}
