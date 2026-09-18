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

namespace Config
{
	struct Values
	{
		// Virtual-key code that opens/closes the menu. INI key: [General]
		// MenuKey, a keycap-style name (see KeyNames.h).
		DWORD MenuKey = VK_F9;
	};

	void Reload();
	const Values& Get();
}
