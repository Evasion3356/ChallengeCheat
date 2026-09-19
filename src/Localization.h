/*
	Localizes everything this mod draws on screen: the 9 category names, the
	Advance/Complete verbs and the "why didn't that work" messages.

	Two sources, kept deliberately separate:

	- Category names and the Dead Eye term are the GAME'S OWN per-language
	  wording, dumped from each language's global.yldb (data/lang/<lang>_rel.rpf
	  in update_3.rpf) by tools/dump_labels.py + tools/gen_localization.py into
	  the generated LocalizationData.h -- e.g. de-DE "Reitkunst" for Horseman,
	  fr-FR "Sang-froid" for Dead Eye. Not translations; do not hand-edit.

	- The Advance/Complete verbs and failure messages have no game equivalent,
	  so those tables in Localization.cpp are LLM-assisted translations, not yet
	  reviewed by a native speaker (same caveat as PokerCheat's). Fix a row
	  there directly if a wording is wrong. Where a message names a game
	  concept (Dead Eye, binoculars, train) it uses the game's own word.

	Language is auto-detected from the game's UI language via
	LANGUAGE::_GET_CURRENT_LANGUAGE_ID(); ChallengeCheat.ini's [General]
	Language key overrides it ("auto" or a code like "de-DE"). The game only
	loads the real font/glyph assets for its own configured language, so
	non-Latin overrides only render correctly with the game set to match.
*/

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Localization
{
	// Matches LANGUAGE::_GET_CURRENT_LANGUAGE_ID()'s return value (same
	// mapping as PokerCheat's Localization.h).
	enum class Language : std::int32_t
	{
		English = 0,             // en-US
		French = 1,              // fr-FR
		German = 2,              // de-DE
		Italian = 3,             // it-IT
		Spanish = 4,             // es-ES
		PortugueseBrazilian = 5, // pt-BR
		Polish = 6,              // pl-PL
		Russian = 7,             // ru-RU
		Korean = 8,              // ko-KR
		ChineseTraditional = 9,  // zh-TW
		Japanese = 10,           // ja-JP
		SpanishMexican = 11,     // es-MX
		ChineseSimplified = 12,  // zh-CN

		Count = 13
	};

	enum class Msg
	{
		Advance,           // button verb
		Complete,          // button verb
		AlreadyCompleted,  // "Already fully completed" (caller appends " (a/b)")
		NeedHorseback,
		NeedTrain,
		NeedBinoculars,
		NeedDeadEyeAiming,
		NeedDeadEye,
		AllItemsCredited,
		NoKnownWrite,

		Count
	};

	// Re-resolves from the ini override, else the game's UI language.
	// Script-thread only (calls a native); Current() does it lazily.
	void Refresh();
	Language Current();

	// The game's own name for a ChallengeCheat::Category (cast to int).
	// Points at static data.
	std::string_view CategoryName(int category);

	// The game's own objective text for a category's rank (rank is 1-based,
	// clamped to 1-10), e.g. "Hold up 5 townsfolk". Points at static data.
	std::string_view RankObjective(int category, int rank);

	// Localized mod-own text with any "{}" already filled in. Resolved once
	// per Refresh(), so this is allocation-free (safe to call every frame);
	// the view stays valid until the next Refresh().
	std::string_view Text(Msg msg);
}
