/*
	Entry point. Registers ScriptMain as a ScriptHookRDR2 script thread and
	wires up the keyboard handler, same pattern as PokerCheat/DominoCheat's
	own main.cpp.
*/

#include "..\external\ScriptHookSDK\inc\main.h"
#include "script.h"
#include "keyboard.h"
#include "TimedRideHook.h"

BOOL APIENTRY DllMain(HMODULE hInstance, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		scriptRegister(hInstance, ScriptMain);
		// Unlike PokerCheat/BlackjackCheat/DominoCheat, the F9 menu here is
		// NOT a Debug-only dev tool -- it's the only way to use this mod at
		// all (see script.cpp's header comment), so the keyboard handler is
		// needed in Release too.
		keyboardHandlerRegister(OnKeyboardMessage);
		break;
	case DLL_PROCESS_DETACH:
		scriptUnregister(hInstance);
		keyboardHandlerUnregister(OnKeyboardMessage);
		// MinHook is a process-wide library -- fully disable/remove the
		// hook and uninitialize it here rather than leaving it installed
		// past this ASI's own lifetime (e.g. if the mod loader unloads
		// this DLL without the process exiting).
		// Skipped when the process is exiting (non-null lpReserved): every
		// other thread is already gone and the hook dies with the process,
		// so MinHook's thread suspension/patching under the loader lock
		// would only add risk.
		if (!lpReserved)
			TimedRideHook::Uninstall();
		break;
	}
	return TRUE;
}
