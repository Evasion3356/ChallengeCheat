#include "TimedRideHook.h"
#include "PatternScan.h"
#include "Log.h"

#include <MinHook.h>
#include <windows.h>
#include <atomic>
#include <cstdint>

namespace
{
	// sub_140BAC640's own prologue -- the function's TRUE entry point (not
	// a point partway through it), since MinHook needs this to build a
	// trampoline. Verified unique across the whole ~115MB image (single
	// match, at the expected address) via a headless idat.exe + IDAPython
	// scan, 2026-09-17 -- see TimedRideHook.h's header comment.
	constexpr const char* kHookSignature =
		"48 89 5C 24 ? 48 89 6C 24 ? 56 41 54 41 56 48 81 EC";

	// sub_140B9842C's own prologue, extended far enough into the function
	// body (through its `mov rdi, rcx` / vtable-call setup) to be unique
	// on its own -- an earlier, shorter attempt at this signature matched
	// 3 different functions in the image. Also verified unique, 2026-09-17.
	constexpr const char* kFallbackSignature =
		"48 89 5C 24 ? 48 89 7C 24 ? 55 48 8D 6C 24 ? 48 81 EC ? ? ? ? ? ? ? 48 8B F9 FF 90";

	// __fastcall is a no-op on x64 (one calling convention only) -- kept
	// for readability/consistency with IDA's own signature guesses.
	using HookedFn = char(__fastcall*)(void* a1, std::uint64_t rdx);
	using FallbackFn = std::uint64_t(__fastcall*)(void* a1);

	HookedFn g_original = nullptr;
	FallbackFn g_fallback = nullptr;
	bool g_installed = false;

	// 0 = no target (every call passes through unaffected). Set/cleared by
	// ScopedTarget. Written from the script thread (AdvanceRank), read
	// from whatever thread calls sub_140BAC640 (the game's own update/tick
	// thread, not the script thread) -- genuinely cross-thread, hence
	// atomic rather than a plain global.
	std::atomic<std::uint64_t> g_targetRdx{ 0 };

	char __fastcall Detour(void* a1, std::uint64_t rdx)
	{
		std::uint64_t target = g_targetRdx.load(std::memory_order_relaxed);
		if (target != 0 && rdx == target)
		{
			std::uint64_t result = g_fallback ? g_fallback(a1) : 0;
			Log::Write("TimedRideHook: matched targeted rdx={:#x} (a1={:#x}) -- forced fallback path, "
				"sub_140B9842C returned {:#x}", rdx, reinterpret_cast<std::uintptr_t>(a1), result);
			return static_cast<char>(result & 0xFF);
		}

		return g_original(a1, rdx);
	}
}

namespace TimedRideHook
{
	bool EnsureInstalled()
	{
		if (g_installed)
			return true;

		MH_STATUS initStatus = MH_Initialize();
		if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
		{
			Log::Write("TimedRideHook: MH_Initialize failed ({})", static_cast<int>(initStatus));
			return false;
		}

		auto hookAddr = PatternScan::FindInMainModule(kHookSignature);
		if (!hookAddr)
		{
			Log::Write("TimedRideHook: hook signature not found (game build may have changed) -- not installed.");
			return false;
		}

		auto fallbackAddr = PatternScan::FindInMainModule(kFallbackSignature);
		if (!fallbackAddr)
		{
			Log::Write("TimedRideHook: fallback signature not found (game build may have changed) -- not installed.");
			return false;
		}

		g_fallback = reinterpret_cast<FallbackFn>(*fallbackAddr);

		void* target = reinterpret_cast<void*>(*hookAddr);
		MH_STATUS createStatus = MH_CreateHook(target, reinterpret_cast<void*>(&Detour), reinterpret_cast<void**>(&g_original));
		if (createStatus != MH_OK)
		{
			Log::Write("TimedRideHook: MH_CreateHook failed at {:#x} ({})", *hookAddr, static_cast<int>(createStatus));
			g_fallback = nullptr;
			return false;
		}

		MH_STATUS enableStatus = MH_EnableHook(target);
		if (enableStatus != MH_OK)
		{
			Log::Write("TimedRideHook: MH_EnableHook failed at {:#x} ({})", *hookAddr, static_cast<int>(enableStatus));
			MH_RemoveHook(target);
			g_fallback = nullptr;
			return false;
		}

		g_installed = true;
		Log::Write("TimedRideHook: installed at {:#x} (RDR2.exe+{:#x}), fallback at {:#x} (RDR2.exe+{:#x})",
			*hookAddr, *hookAddr - reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)),
			*fallbackAddr, *fallbackAddr - reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)));
		return true;
	}

	void Uninstall()
	{
		if (!g_installed)
		{
			// Still safe/correct to uninitialize even if nothing was ever
			// installed -- MH_Uninitialize on an unused/already-clean
			// state is a documented no-op-ish success, not an error case
			// worth special-casing here.
			MH_Uninitialize();
			return;
		}

		auto hookAddr = PatternScan::FindInMainModule(kHookSignature);
		if (hookAddr)
		{
			MH_DisableHook(reinterpret_cast<void*>(*hookAddr));
			MH_RemoveHook(reinterpret_cast<void*>(*hookAddr));
		}

		MH_Uninitialize();

		g_installed = false;
		g_original = nullptr;
		g_fallback = nullptr;
		g_targetRdx.store(0, std::memory_order_relaxed);
		Log::Write("TimedRideHook: uninstalled.");
	}

	ScopedTarget::ScopedTarget(std::uint64_t targetRdx)
	{
		g_targetRdx.store(targetRdx, std::memory_order_relaxed);
	}

	ScopedTarget::~ScopedTarget()
	{
		g_targetRdx.store(0, std::memory_order_relaxed);
	}
}
