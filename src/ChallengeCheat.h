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

	// STATS::CHAL_GET_NUM_RANKS_COMPLETED / CHAL_GET_MAX_RANKS passthroughs
	// for the category's root hash. Safe to call every frame (menu status
	// line does exactly that).
	int GetRanksCompleted(Category category);
	int GetMaxRanks(Category category);

	// Attempts to push the category from its current completed-rank count to
	// the next rank, by incrementing (via STATS::_STAT_ID_INCREMENT_INT/_FLOAT)
	// every real stat, or adding progress (via CHAL_ADD_GOAL_PROGRESS_INT) to
	// every real script-sourced goal, known for that specific next rank --
	// see ChallengeCheat.cpp's kKnownWrites table, generated directly from
	// goals_sp.meta/challenges_sp.meta (the REAL files, extracted from the
	// game's own RPFs -- see that file's header comment) and covering 112 of
	// the game's 138 total defined goals. Some goals also require a live
	// condition at the moment of writing (e.g. being on horseback) -- see
	// GetLastAdvanceFailureReason(). Returns true only if
	// CHAL_GET_NUM_RANKS_COMPLETED actually increased afterward (polls for a
	// few seconds first -- the real completion check doesn't fire in the
	// same tick as the write) -- never reports success on a guess.
	bool AdvanceRank(Category category);

	// A short, human-readable reason for the most recent AdvanceRank() call
	// on ANY category returning false, or empty if the last call succeeded
	// or none has run yet. Currently only populated for a live-requirement
	// gate that can be checked in advance (e.g. "requires being on
	// horseback") -- a generic timeout/no-known-goal failure leaves this
	// empty and the caller should point the player at the log instead.
	const std::string& GetLastAdvanceFailureReason();

	// Repeatedly calls AdvanceRank() until CHAL_GET_NUM_RANKS_COMPLETED
	// reaches CHAL_GET_MAX_RANKS or a rank fails to advance. Returns the
	// number of ranks actually advanced this call.
	int CompleteChallenge(Category category);
}
