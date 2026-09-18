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
	constexpr const char* kTimedRideChallengeCheckSignature =
		"48 89 5C 24 ? 48 89 6C 24 ? 56 41 54 41 56 48 81 EC";

	// sub_140B9842C's own prologue, extended far enough into the function
	// body (through its `mov rdi, rcx` / vtable-call setup) to be unique
	// on its own -- an earlier, shorter attempt at this signature matched
	// 3 different functions in the image. Also verified unique, 2026-09-17.
	constexpr const char* kUnlockChallengeSignature =
		"48 89 5C 24 ? 48 89 7C 24 ? 55 48 8D 6C 24 ? 48 81 EC ? ? ? ? ? ? ? 48 8B F9 FF 90";
}

namespace TimedRideHook
{
	struct ScopedTimedRideUnlock::Runtime
	{
		TimedRideChallengeCheckFn originalTimedRideChallengeCheck = nullptr;
		UnlockChallengeFn unlockChallenge = nullptr;
		void* timedRideChallengeCheckAddress = nullptr;
		bool minHookInitialized = false;
		bool timedRideChallengeCheckHookCreated = false;
		std::atomic_bool timedRideChallengeCheckHookEnabled{ false };

		// 0 = no target (every call passes through unaffected). Set/cleared
		// by ScopedTimedRideUnlock. Written from the script thread
		// (AdvanceRank), read from whatever thread calls sub_140BAC640 (the
		// game's own update/tick thread, not the script thread) -- genuinely
		// cross-thread, hence atomic rather than a plain field.
		std::atomic<TimedRideGoalIndex> targetTimedRideGoalIndex{ 0 };
	};

	ScopedTimedRideUnlock::Runtime& ScopedTimedRideUnlock::GetRuntime()
	{
		static Runtime runtime;
		return runtime;
	}

	void ScopedTimedRideUnlock::DisableTimedRideChallengeCheckHook(const char* reason)
	{
		Runtime& runtime = GetRuntime();
		if (!runtime.timedRideChallengeCheckAddress)
			return;

		if (!runtime.timedRideChallengeCheckHookEnabled.exchange(false, std::memory_order_acq_rel))
			return;

		MH_STATUS status = MH_DisableHook(runtime.timedRideChallengeCheckAddress);
		if (status != MH_OK && status != MH_ERROR_DISABLED)
		{
			Log::Write("TimedRideHook: MH_DisableHook failed at {:#x} after {} ({})",
				reinterpret_cast<std::uintptr_t>(runtime.timedRideChallengeCheckAddress), reason, MH_StatusToString(status));
			runtime.timedRideChallengeCheckHookEnabled.store(true, std::memory_order_release);
			return;
		}

		Log::Write("TimedRideHook: disabled timed-ride challenge-check hook after {}.", reason);
	}

	char __fastcall ScopedTimedRideUnlock::TimedRideChallengeCheckDetour(ChallengeState* challengeState, TimedRideGoalIndex timedRideGoalIndex)
	{
		Runtime& runtime = GetRuntime();
		TimedRideGoalIndex targetGoalIndex = runtime.targetTimedRideGoalIndex.load(std::memory_order_acquire);
		if (targetGoalIndex != 0 && timedRideGoalIndex == targetGoalIndex && runtime.unlockChallenge)
		{
			runtime.targetTimedRideGoalIndex.store(0, std::memory_order_release);
			std::uint64_t result = runtime.unlockChallenge(challengeState);
			DisableTimedRideChallengeCheckHook("target match");
			Log::Write("TimedRideHook: matched timed-ride goal index {:#x} (challengeState={:#x}) -- "
				"called unlock-challenge function, disabled hook, returning {:#x}",
				timedRideGoalIndex, reinterpret_cast<std::uintptr_t>(challengeState), result);
			return static_cast<char>(result & 0xFF);
		}

		return runtime.originalTimedRideChallengeCheck(challengeState, timedRideGoalIndex);
	}

	bool ScopedTimedRideUnlock::CreateTimedRideChallengeCheckHook()
	{
		Runtime& runtime = GetRuntime();
		if (runtime.timedRideChallengeCheckHookCreated)
			return true;

		MH_STATUS initStatus = MH_Initialize();
		if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
		{
			Log::Write("TimedRideHook: MH_Initialize failed ({})", MH_StatusToString(initStatus));
			return false;
		}
		runtime.minHookInitialized = true;

		auto timedRideChallengeCheckAddr = PatternScan::FindInMainModule(kTimedRideChallengeCheckSignature);
		if (!timedRideChallengeCheckAddr)
		{
			Log::Write("TimedRideHook: timed-ride challenge-check signature not found (game build may have changed) -- not installed.");
			return false;
		}

		auto unlockChallengeAddr = PatternScan::FindInMainModule(kUnlockChallengeSignature);
		if (!unlockChallengeAddr)
		{
			Log::Write("TimedRideHook: unlock-challenge signature not found (game build may have changed) -- not installed.");
			return false;
		}

		runtime.unlockChallenge = reinterpret_cast<UnlockChallengeFn>(*unlockChallengeAddr);
		runtime.timedRideChallengeCheckAddress = reinterpret_cast<void*>(*timedRideChallengeCheckAddr);

		MH_STATUS createStatus = MH_CreateHook(
			runtime.timedRideChallengeCheckAddress,
			reinterpret_cast<void*>(&ScopedTimedRideUnlock::TimedRideChallengeCheckDetour),
			reinterpret_cast<void**>(&runtime.originalTimedRideChallengeCheck));
		if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED)
		{
			Log::Write("TimedRideHook: MH_CreateHook failed at {:#x} ({})",
				*timedRideChallengeCheckAddr, MH_StatusToString(createStatus));
			runtime.timedRideChallengeCheckAddress = nullptr;
			runtime.unlockChallenge = nullptr;
			return false;
		}

		runtime.timedRideChallengeCheckHookCreated = true;
		Log::Write("TimedRideHook: created timed-ride challenge-check hook at {:#x} (RDR2.exe+{:#x}), "
			"unlock-challenge function at {:#x} (RDR2.exe+{:#x})",
			*timedRideChallengeCheckAddr,
			*timedRideChallengeCheckAddr - reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)),
			*unlockChallengeAddr,
			*unlockChallengeAddr - reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)));
		return true;
	}

	bool ScopedTimedRideUnlock::EnableTimedRideChallengeCheckHook(TimedRideGoalIndex targetTimedRideGoalIndex)
	{
		if (targetTimedRideGoalIndex == 0)
		{
			Log::Write("TimedRideHook: refusing to enable hook for invalid timed-ride goal index 0.");
			return false;
		}

		if (!CreateTimedRideChallengeCheckHook())
			return false;

		Runtime& runtime = GetRuntime();
		runtime.targetTimedRideGoalIndex.store(targetTimedRideGoalIndex, std::memory_order_release);

		MH_STATUS enableStatus = MH_EnableHook(runtime.timedRideChallengeCheckAddress);
		if (enableStatus != MH_OK && enableStatus != MH_ERROR_ENABLED)
		{
			runtime.targetTimedRideGoalIndex.store(0, std::memory_order_release);
			Log::Write("TimedRideHook: MH_EnableHook failed at {:#x} for timed-ride goal index {:#x} ({})",
				reinterpret_cast<std::uintptr_t>(runtime.timedRideChallengeCheckAddress),
				targetTimedRideGoalIndex,
				MH_StatusToString(enableStatus));
			return false;
		}

		runtime.timedRideChallengeCheckHookEnabled.store(true, std::memory_order_release);
		Log::Write("TimedRideHook: enabled for timed-ride goal index {:#x}.", targetTimedRideGoalIndex);
		return true;
	}

	void Uninstall()
	{
		ScopedTimedRideUnlock::Runtime& runtime = ScopedTimedRideUnlock::GetRuntime();
		runtime.targetTimedRideGoalIndex.store(0, std::memory_order_release);
		ScopedTimedRideUnlock::DisableTimedRideChallengeCheckHook("module unload");

		if (runtime.timedRideChallengeCheckHookCreated && runtime.timedRideChallengeCheckAddress)
		{
			MH_STATUS status = MH_RemoveHook(runtime.timedRideChallengeCheckAddress);
			if (status != MH_OK && status != MH_ERROR_NOT_CREATED)
			{
				Log::Write("TimedRideHook: MH_RemoveHook failed at {:#x} ({})",
					reinterpret_cast<std::uintptr_t>(runtime.timedRideChallengeCheckAddress), MH_StatusToString(status));
			}
		}

		if (runtime.minHookInitialized)
		{
			MH_Uninitialize();
		}

		runtime.minHookInitialized = false;
		runtime.timedRideChallengeCheckHookCreated = false;
		runtime.timedRideChallengeCheckHookEnabled.store(false, std::memory_order_release);
		runtime.timedRideChallengeCheckAddress = nullptr;
		runtime.originalTimedRideChallengeCheck = nullptr;
		runtime.unlockChallenge = nullptr;
		Log::Write("TimedRideHook: uninstalled.");
	}

	ScopedTimedRideUnlock::ScopedTimedRideUnlock(TimedRideGoalIndex targetTimedRideGoalIndex)
		: m_ready(EnableTimedRideChallengeCheckHook(targetTimedRideGoalIndex))
	{
	}

	ScopedTimedRideUnlock::~ScopedTimedRideUnlock()
	{
		if (!m_ready)
			return;

		Runtime& runtime = GetRuntime();
		runtime.targetTimedRideGoalIndex.store(0, std::memory_order_release);
		DisableTimedRideChallengeCheckHook("scoped timed-ride unlock ended without a target match");
	}

	bool ScopedTimedRideUnlock::IsReady() const
	{
		return m_ready;
	}
}
