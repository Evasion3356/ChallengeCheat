// Same inipp-based load/rewrite pattern as DominoCheat's Config.cpp -- see
// Config.h's header comment.

#include "Config.h"
#include "KeyNames.h"
#include "Log.h"

#include "..\external\inipp\inipp\inipp.h"

#include <fstream>
#include <string>
#include <exception>

namespace
{
	using Section = inipp::Ini<char>::Section;

	Config::Values g_values;
	bool g_loaded = false;

	const std::wstring& ResolveIniPath()
	{
		static const std::wstring path = []() -> std::wstring
		{
			HMODULE hModule = nullptr;
			GetModuleHandleExA(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCSTR>(&ResolveIniPath),
				&hModule);

			wchar_t modulePath[MAX_PATH] = {};
			GetModuleFileNameW(hModule, modulePath, MAX_PATH);

			wchar_t drive[_MAX_DRIVE], dir[_MAX_DIR];
			_wsplitpath_s(modulePath, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);

			return std::wstring(drive) + dir + L"ChallengeCheat.ini";
		}();

		return path;
	}

	void ReloadImpl()
	{
		inipp::Ini<char> ini;
		{
			std::ifstream is(ResolveIniPath());
			if (is)
				ini.parse(is);
		}

		const Config::Values defaults;
		Section& general = ini.sections["General"];

		g_values.MenuKey = defaults.MenuKey;
		auto it = general.find("MenuKey");
		if (it != general.end())
		{
			DWORD vk = 0;
			if (KeyNames::Parse(it->second, vk))
			{
				g_values.MenuKey = vk;
			}
			else
			{
				Log::Write("Config::Reload -- MenuKey '{}' is not a usable key name (unrecognized, or reserved for menu navigation) -- using {}",
					it->second, KeyNames::Format(defaults.MenuKey));
			}
		}
		general["MenuKey"] = KeyNames::Format(g_values.MenuKey);

		g_values.Language = defaults.Language;
		auto languageIt = general.find("Language");
		if (languageIt != general.end() && !languageIt->second.empty())
			g_values.Language = languageIt->second;
		general["Language"] = g_values.Language;

		g_values.WrapWidth = defaults.WrapWidth;
		auto wrapIt = general.find("WrapWidth");
		if (wrapIt != general.end())
		{
			try
			{
				const int width = std::stoi(wrapIt->second);
				if (width == 0 || (width >= 10 && width <= 120))
					g_values.WrapWidth = width;
				else
					Log::Write("Config::Reload -- WrapWidth {} is not 0 or 10-120 -- using {}", width, defaults.WrapWidth);
			}
			catch (const std::exception&)
			{
				Log::Write("Config::Reload -- WrapWidth '{}' is not a number -- using {}", wrapIt->second, defaults.WrapWidth);
			}
		}
		general["WrapWidth"] = std::to_string(g_values.WrapWidth);

		{
			std::ofstream os(ResolveIniPath(), std::ios::trunc);
			if (os)
				ini.generate(os);
			else
				Log::Write("Config::Reload -- failed to open ChallengeCheat.ini for writing");
		}

		Log::Write("Config::Reload -- MenuKey={} (vk 0x{:02X})", general["MenuKey"], g_values.MenuKey);
	}
}

namespace Config
{
	void Reload()
	{
		try
		{
			ReloadImpl();
		}
		catch (const std::exception& e)
		{
			Log::Write("Config::Reload -- std::exception: {} -- keeping previous config values", e.what());
		}
		catch (...)
		{
			Log::Write("Config::Reload -- unknown non-std exception -- keeping previous config values");
		}

		g_loaded = true;
	}

	const Values& Get()
	{
		if (!g_loaded)
			Reload();

		return g_values;
	}
}
