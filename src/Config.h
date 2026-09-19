/*
	Lightweight INI-backed config, same inipp-based load/rewrite pattern as
	PokerCheat/BlackjackCheat/DominoCheat's own Config. Unlike those, this
	mod's INI is a user-facing setting in Release too, not a Debug-only
	tuning surface (the F9 menu is Release-visible for the same reason --
	see ChallengeCheat's CLAUDE.md).

	ChallengeCheat.ini lives next to the .asi. Missing/invalid values fall
	back to defaults and the file is rewritten with the resolved values, so
	it always reflects what the mod is actually using.

	Loaded lazily on the first Get() (from the script thread), NOT from
	DllMain -- MenuKey parsing calls into user32 (VkKeyScanW), which has no
	business running under the loader lock.
*/

#pragma once

#include <windows.h>
#include <string>

namespace Config
{
	struct Values
	{
		// Virtual-key code that opens/closes the menu. INI key: [General]
		// MenuKey, a keycap-style name (see KeyNames.h).
		DWORD MenuKey = VK_F9;

		// Overrides the menu language. INI key: [General] Language --
		// "auto" (default) follows the game's own UI language, or one of
		// en-US, fr-FR, de-DE, it-IT, es-ES, pt-BR, pl-PL, ru-RU, ko-KR,
		// zh-TW, ja-JP, es-MX, zh-CN. See Localization.h.
		std::string Language = "auto";

		// How many "units" of text fit on one line of the wrapped rank
		// objective (a Latin/Cyrillic character = 1, a CJK/Hangul/kana one =
		// 2). An estimate -- there is no text-measuring call -- so raise it if
		// lines wrap too early, lower it if they overflow the box. 0 = never
		// wrap (one line per objective; the longest run off the box). INI key:
		// [General] WrapWidth (0, or 10-120).
		int WrapWidth = 50;
	};

	void Reload();
	const Values& Get();
}
