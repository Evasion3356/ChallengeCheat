/*
	ChallengeCheat -- ScriptHookRDR2 ASI mod that advances/completes RDR2's
	singleplayer Challenges through the game's own real STATS::CHAL_* native
	progress system. See ChallengeCheat.cpp's file header comment for the
	full research trail and exactly which ranks are currently covered.

	Unlike PokerCheat/BlackjackCheat/DominoCheat, the menu here is NOT a
	Debug-only dev-tuning surface -- it's the entire product (there's no
	passive "advisor" behavior to auto-enable in Release the way those
	mods' HUDs work). So the menu itself, and Advance Rank/Complete
	Challenge, are built and available in BOTH Debug and Release.

	Press F9 in-game to open the menu (NUMPAD 8/2 to move, NUMPAD 5 to
	select, NUMPAD 0/Backspace/F9 to back out -- same controls as PokerCheat/
	BlackjackCheat/DominoCheat's own menus, which this is built on).
*/

#include "scriptmenu.h" // pulls in script.h (natives/types/enums/main) and keyboard.h
#include "Log.h"
#include "ChallengeCheat.h"

#include <array>
#include <sstream>

namespace
{
	MenuController g_menuController;
	MenuBase* g_mainMenu = nullptr;
	std::array<MenuBase*, static_cast<size_t>(ChallengeCheat::Category::Count)> g_categoryMenus{};

	std::string FormatRank(ChallengeCheat::Category category)
	{
		std::ostringstream oss;
		oss << "Rank " << ChallengeCheat::GetRanksCompleted(category)
			<< " / " << ChallengeCheat::GetMaxRanks(category);
		return oss.str();
	}

	std::string DoAdvanceRank(ChallengeCheat::Category category)
	{
		if (ChallengeCheat::AdvanceRank(category))
		{
			return std::string(ChallengeCheat::GetDisplayName(category)) + ": advanced to rank "
				+ std::to_string(ChallengeCheat::GetRanksCompleted(category)) + " (see log)";
		}
		const std::string& reason = ChallengeCheat::GetLastAdvanceFailureReason();
		if (!reason.empty())
			return std::string(ChallengeCheat::GetDisplayName(category)) + ": " + reason;
		return std::string(ChallengeCheat::GetDisplayName(category)) + ": could not advance -- see log for why";
	}

	std::string DoCompleteChallenge(ChallengeCheat::Category category)
	{
		int advanced = ChallengeCheat::CompleteChallenge(category);
		if (advanced > 0)
		{
			return std::string(ChallengeCheat::GetDisplayName(category)) + ": advanced " + std::to_string(advanced)
				+ " rank(s), now " + std::to_string(ChallengeCheat::GetRanksCompleted(category)) + "/"
				+ std::to_string(ChallengeCheat::GetMaxRanks(category)) + " (see log)";
		}
		const std::string& reason = ChallengeCheat::GetLastAdvanceFailureReason();
		if (!reason.empty())
			return std::string(ChallengeCheat::GetDisplayName(category)) + ": " + reason;
		return std::string(ChallengeCheat::GetDisplayName(category)) + ": could not advance any further -- see log for why";
	}

	MenuBase* BuildCategoryMenu(ChallengeCheat::Category category)
	{
		MenuBase* menu = new MenuBase(new MenuItemTitle(ChallengeCheat::GetDisplayName(category)));
		menu->AddItem(new MenuItemLabel([category]() { return FormatRank(category); }));
		menu->AddItem(new MenuItemActionStatus("Advance Rank", [category]() { return DoAdvanceRank(category); }));
		menu->AddItem(new MenuItemActionStatus("Complete Challenge", [category]() { return DoCompleteChallenge(category); }));
		g_menuController.RegisterMenu(menu); // required for MenuItemMenu::OnSelect's PushMenu to accept it
		return menu;
	}

	void BuildMenu()
	{
		g_mainMenu = new MenuBase(new MenuItemTitle("ChallengeCheat"));

		for (int i = 0; i < static_cast<int>(ChallengeCheat::Category::Count); i++)
		{
			auto category = static_cast<ChallengeCheat::Category>(i);
			g_categoryMenus[i] = BuildCategoryMenu(category);
			g_mainMenu->AddItem(new MenuItemMenu(ChallengeCheat::GetDisplayName(category), g_categoryMenus[i]));
		}

		g_menuController.RegisterMenu(g_mainMenu);
	}
}

void ScriptMain()
{
	Log::Write("ChallengeCheat started");

	BuildMenu();

	while (true)
	{
		if (!g_menuController.HasActiveMenu() && MenuInput::MenuSwitchPressed())
			g_menuController.PushMenu(g_mainMenu);

		g_menuController.Update();

		WAIT(0);
	}
}
