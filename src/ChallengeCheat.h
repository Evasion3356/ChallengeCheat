#pragma once

#include <string>

// ChallengeCheat -- ScriptHookRDR2 ASI mod that advances/completes RDR2's
// singleplayer "Challenges" (Bandit, Explorer, Gambler, Herbalist, Horseman,
// Master Hunter, Sharpshooter, Survivalist, Weapons Expert). See
// ChallengeCheat.cpp's file header comment for the full research trail --
// the real goal registry was recovered directly from the game's own data
// files (challenges_sp.meta/goals_sp.meta, extracted from the RPFs), not
// guessed or reverse-engineered from scripts.

namespace ChallengeCheat
{
	// The 9 singleplayer challenge categories, in the exact order
	// pause_menu.ysc.c's own index -> root-hash switch uses (build 1491.50,
	// confirmed directly from the decompile, not guessed).
	enum class Category
	{
		Bandit,
		Explorer,
		Gambler,
		Herbalist,
		Horseman,
		MasterHunter,
		Sharpshooter,
		Survivalist,
		WeaponsExpert,
		Count
	};

	const char* GetDisplayName(Category category);

	struct RankInfo
	{
		int completed = 0;
		int max = 0;
	};

	// STATS::CHAL_GET_NUM_RANKS_COMPLETED / CHAL_GET_MAX_RANKS passthroughs
	// for the category's root hash. Safe to call every frame (menu status
	// line does exactly that).
	RankInfo GetRankInfo(Category category);
	int GetRanksCompleted(Category category);
	int GetMaxRanks(Category category);

	// Nudges the category's CURRENT rank forward by one small step (e.g. "hit
	// 1 more of the 4 rabbits needed") -- by incrementing (via
	// STATS::_STAT_ID_INCREMENT_INT/_FLOAT) every real stat, or adding
	// progress (via CHAL_ADD_GOAL_PROGRESS_INT) to every real script-sourced
	// goal, known for that specific next rank. See ChallengeCheat.cpp's
	// kKnownWrites table, generated directly from goals_sp.meta/
	// challenges_sp.meta (the REAL files, extracted from the game's own RPFs
	// -- see that file's header comment). A handful of write kinds
	// (PointToPointHook, HorseCompendium) have no meaningful sub-unit to
	// step through and always resolve fully on the first call regardless.
	// Some goals also require a live condition at the moment of writing
	// (e.g. being on horseback) -- see GetLastAdvanceFailureReason().
	// Fire-and-forget (2026-09-19): does NOT wait for or confirm
	// CHAL_GET_NUM_RANKS_COMPLETED moved -- a step usually isn't meant to
	// complete the rank, and even a write that DOES complete it is often not
	// reflected by the engine for several seconds, so polling here produced
	// false "could not advance" negatives on writes that were actually
	// landing. Returns true once the step's write(s) were applied; false
	// only when nothing could be attempted at all (already maxed, a live
	// requirement gate blocks it, or the goal has no known mechanism yet) --
	// check GetLastAdvanceFailureReason() in that case.
	bool AdvanceRank(Category category);

	// A short, human-readable reason for the most recent AdvanceRank() or
	// CompleteChallenge() call on ANY category returning false/0, or empty
	// if the last call succeeded or none has run yet. Always populated
	// whenever either call returns failure (already maxed, a live-
	// requirement gate, or no known write for that goal) -- that's also the
	// only case that should ever show anything on screen; a success leaves
	// this empty and shows no message (see script.cpp's Do*() helpers).
	const std::string& GetLastAdvanceFailureReason();

	// Pushes the category's CURRENT rank straight to completion in one shot
	// (the goal's full target, a safe overshoot for any "reach N" check) --
	// this is what AdvanceRank() itself did before 2026-09-19, when
	// AdvanceRank was redefined to mean a single small step instead.
	// Despite the name, this completes only the ONE current rank per call,
	// not the whole 1-10 category -- call it repeatedly (e.g. from the menu)
	// to clear an entire category rank by rank. Same fire-and-forget
	// contract as AdvanceRank(): returns 1 once the write(s) were applied,
	// without waiting for or confirming CHAL_GET_NUM_RANKS_COMPLETED moved;
	// 0 only when nothing could be attempted at all -- check
	// GetLastAdvanceFailureReason() in that case.
	int CompleteChallenge(Category category);
}
