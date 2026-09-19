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
#include "Localization.h"
#include "TimedRideHook.h"

#include <array>
#include <string>

namespace
{
	MenuController g_menuController;
	MenuBase* g_mainMenu = nullptr;
	std::array<MenuBase*, static_cast<size_t>(ChallengeCheat::Category::Count)> g_categoryMenus{};

	// Captions are rebuilt every draw, so append into one string rather than
	// chaining operator+ temporaries.
	std::string FormatRank(ChallengeCheat::Category category)
	{
		const auto rankInfo = ChallengeCheat::GetRankInfo(category);
		std::string out(ChallengeCheat::GetDisplayName(category));
		return out.append(" ").append(std::to_string(rankInfo.completed))
			.append(" / ").append(std::to_string(rankInfo.max));
	}

	// The rank an Advance/Complete click on this category would target right
	// now -- clamped to the category's max so it never reads e.g. "11" once
	// already fully completed (that click still runs and reports "already
	// fully completed" via GetLastAdvanceFailureReason(), this is just what
	// the button caption shows beforehand).
	int NextRank(ChallengeCheat::Category category)
	{
		const auto rankInfo = ChallengeCheat::GetRankInfo(category);
		int next = rankInfo.completed + 1;
		return next > rankInfo.max ? rankInfo.max : next;
	}

	std::string FormatActionCaption(Localization::Msg verb, ChallengeCheat::Category category)
	{
		std::string out(Localization::Text(verb));
		return out.append(" ").append(ChallengeCheat::GetDisplayName(category))
			.append(" ").append(std::to_string(NextRank(category)));
	}

	std::string FormatFailure(ChallengeCheat::Category category)
	{
		std::string out(ChallengeCheat::GetDisplayName(category));
		return out.append(": ").append(ChallengeCheat::GetLastAdvanceFailureReason());
	}

	// Fire-and-forget (2026-09-19): an empty return shows NO status popup at
	// all (MenuItemActionStatus only calls SetStatusText for a non-empty
	// result) -- on screen text should only ever appear when the action
	// genuinely could not be attempted (a live requirement gate, already
	// maxed, or no known write for the goal), never as a "still working on
	// it" guess. See ChallengeCheat.h's AdvanceRank()/CompleteChallenge()
	// doc comments for why polling for confirmation was removed.
	std::string DoAdvanceRank(ChallengeCheat::Category category)
	{
		if (ChallengeCheat::AdvanceRank(category))
			return {};
		return FormatFailure(category);
	}

	std::string DoCompleteChallenge(ChallengeCheat::Category category)
	{
		if (ChallengeCheat::CompleteChallenge(category) > 0)
			return {};
		return FormatFailure(category);
	}

	MenuBase* BuildCategoryMenu(ChallengeCheat::Category category)
	{
		MenuBase* menu = new MenuBase(new MenuItemTitle(std::string(ChallengeCheat::GetDisplayName(category))));
		menu->AddItem(new MenuItemLabel([category]() { return FormatRank(category); }));
		// The game's own objective text for the rank a click would target.
		menu->AddItem(new MenuItemParagraph([category]()
		{
			return Localization::RankObjective(static_cast<int>(category), NextRank(category)); // static data
		}));
		menu->AddItem(new MenuItemActionStatus(
			[category]() { return FormatActionCaption(Localization::Msg::Advance, category); },
			[category]() { return DoAdvanceRank(category); }));
		menu->AddItem(new MenuItemActionStatus(
			[category]() { return FormatActionCaption(Localization::Msg::Complete, category); },
			[category]() { return DoCompleteChallenge(category); }));
		g_menuController.RegisterMenu(menu); // required for MenuItemMenu::OnSelect's PushMenu to accept it
		return menu;
	}

	void BuildMenu()
	{
		g_mainMenu = new MenuBase(new MenuItemTitle("Challenge Cheat"));

		for (int i = 0; i < static_cast<int>(ChallengeCheat::Category::Count); i++)
		{
			auto category = static_cast<ChallengeCheat::Category>(i);
			g_categoryMenus[i] = BuildCategoryMenu(category);
			g_mainMenu->AddItem(new MenuItemMenu(std::string(ChallengeCheat::GetDisplayName(category)), g_categoryMenus[i]));
		}

		g_menuController.RegisterMenu(g_mainMenu);
	}
}

void ScriptMain()
{
	Log::Write("ChallengeCheat started");

	BuildMenu();

	// Pay the AOB-scan + MinHook setup cost now, not on the first
	// Horseman 3/6/9 click.
	TimedRideHook::Initialize();

	while (true)
	{
		if (!g_menuController.HasActiveMenu() && MenuInput::MenuSwitchPressed())
			g_menuController.PushMenu(g_mainMenu);

		g_menuController.Update();

		// Tears down the timed-ride hook once it has fired (or timed out).
		TimedRideHook::Update();

		WAIT(0);
	}
}
