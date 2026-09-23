#include "TimedRideHook.h"
#include "PatternScan.h"
#include "Log.h"

#include <MinHook.h>
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <string_view>

namespace
{
	// sub_140BAC640's own prologue -- the function's TRUE entry point (not
	// a point partway through it), since MinHook needs this to build a
	// trampoline. Verified unique across the whole ~115MB image (single
	// match, at the expected address) via a headless idat.exe + IDAPython
	// scan, 2026-09-17 -- see TimedRideHook.h's header comment.
	constexpr std::string_view kTimedRideChallengeCheckSignature =
		"48 89 5C 24 ? 48 89 6C 24 ? 56 41 54 41 56 48 81 EC";

	// sub_140B9842C's own prologue, extended far enough into the function
	// body (through its `mov rdi, rcx` / vtable-call setup) to be unique
	// on its own -- an earlier, shorter attempt at this signature matched
	// 3 different functions in the image. Also verified unique, 2026-09-17.
	constexpr std::string_view kUnlockChallengeSignature =
		"48 89 5C 24 ? 48 89 7C 24 ? 55 48 8D 6C 24 ? 48 81 EC ? ? ? ? ? ? ? 48 8B F9 FF 90";
}

namespace TimedRideHook
{
	namespace
	{
		// How long an armed hook waits for the engine's periodic
		// re-evaluation to call the check with the target index before
		// giving up and tearing itself down.
		constexpr ULONGLONG kArmTimeoutMs = 15000;

		struct Runtime
		{
			TimedRideChallengeCheckFn originalTimedRideChallengeCheck = nullptr;
			UnlockChallengeFn unlockChallenge = nullptr;
			void* timedRideChallengeCheckAddress = nullptr;
			bool minHookInitialized = false;
			bool hookCreated = false;
			bool armed = false;
			ULONGLONG armedAtMs = 0;

			// 0 = no target (every call passes through unaffected). Written
			// from the script thread, read from whatever thread calls
			// sub_140BAC640 (the game's own update thread) -- genuinely
			// cross-thread, hence atomic.
			std::atomic<TimedRideGoalIndex> targetTimedRideGoalIndex{ 0 };
			std::atomic_bool matched{ false };
		};

		Runtime& GetRuntime()
		{
			static Runtime runtime;
			return runtime;
		}

		char __fastcall TimedRideChallengeCheckDetour(ChallengeState* challengeState, TimedRideGoalIndex timedRideGoalIndex)
		{
			Runtime& runtime = GetRuntime();
			// compare_exchange rather than load-then-store: the engine can
			// evaluate goals on more than one thread, and only ONE call may
			// claim the target and force the unlock.
			TimedRideGoalIndex expected = timedRideGoalIndex;
			if (timedRideGoalIndex != 0 && runtime.unlockChallenge &&
				runtime.targetTimedRideGoalIndex.compare_exchange_strong(expected, 0, std::memory_order_acq_rel))
			{
				std::uint64_t result = runtime.unlockChallenge(challengeState);
				runtime.matched.store(true, std::memory_order_release);
				Log::Write("TimedRideHook: matched timed-ride goal index {:#x} (challengeState={:#x}) -- "
					"called unlock-challenge function, returning {:#x}",
					timedRideGoalIndex, reinterpret_cast<std::uintptr_t>(challengeState), result);
				return static_cast<char>(result & 0xFF);
			}

			return runtime.originalTimedRideChallengeCheck(challengeState, timedRideGoalIndex);
		}

		bool CreateHook(Runtime& runtime)
		{
			if (runtime.hookCreated)
				return true;

			MH_STATUS initStatus = MH_Initialize();
			if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
			{
				Log::Write("TimedRideHook: MH_Initialize failed ({})", MH_StatusToString(initStatus));
				return false;
			}
			runtime.minHookInitialized = true;

			auto checkAddr = PatternScan::FindInMainModule(kTimedRideChallengeCheckSignature);
			if (!checkAddr)
			{
				Log::Write("TimedRideHook: timed-ride challenge-check signature not found (game build may have changed) -- not installed.");
				return false;
			}

			auto unlockAddr = PatternScan::FindInMainModule(kUnlockChallengeSignature);
			if (!unlockAddr)
			{
				Log::Write("TimedRideHook: unlock-challenge signature not found (game build may have changed) -- not installed.");
				return false;
			}

			// Both signatures were verified unique in build 1491.50, but the
			// check-function one is a generic prologue. On any other build a
			// second match means we can't tell which function is the real
			// one, and hooking the wrong one would call the unlock function on
			// an unrelated object -- refuse instead.
			if (PatternScan::FindInMainModule(kTimedRideChallengeCheckSignature, *checkAddr + 1) ||
				PatternScan::FindInMainModule(kUnlockChallengeSignature, *unlockAddr + 1))
			{
				Log::Write("TimedRideHook: a signature matched more than once (game build may have changed) -- not installed.");
				return false;
			}

			runtime.unlockChallenge = reinterpret_cast<UnlockChallengeFn>(*unlockAddr);
			runtime.timedRideChallengeCheckAddress = reinterpret_cast<void*>(*checkAddr);

			MH_STATUS createStatus = MH_CreateHook(
				runtime.timedRideChallengeCheckAddress,
				reinterpret_cast<void*>(&TimedRideChallengeCheckDetour),
				reinterpret_cast<void**>(&runtime.originalTimedRideChallengeCheck));
			if (createStatus != MH_OK)
			{
				Log::Write("TimedRideHook: MH_CreateHook failed at {:#x} ({})", *checkAddr, MH_StatusToString(createStatus));
				runtime.timedRideChallengeCheckAddress = nullptr;
				runtime.unlockChallenge = nullptr;
				return false;
			}

			runtime.hookCreated = true;
			Log::Write("TimedRideHook: created timed-ride challenge-check hook at {:#x} (RDR2.exe+{:#x}), "
				"unlock-challenge function at {:#x} (RDR2.exe+{:#x})",
				*checkAddr,
				*checkAddr - reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)),
				*unlockAddr,
				*unlockAddr - reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)));
			return true;
		}

		// Script-thread only. Disables the hook and clears the target but
		// keeps it created, so the next Arm() is just an enable -- never
		// call from the detour.
		void Disarm(Runtime& runtime, std::string_view reason)
		{
			runtime.targetTimedRideGoalIndex.store(0, std::memory_order_release);

			if (runtime.hookCreated && runtime.timedRideChallengeCheckAddress)
			{
				MH_STATUS status = MH_DisableHook(runtime.timedRideChallengeCheckAddress);
				if (status != MH_OK && status != MH_ERROR_DISABLED)
					Log::Write("TimedRideHook: MH_DisableHook failed ({})", MH_StatusToString(status));
			}

			const bool wasArmed = runtime.armed;
			runtime.armed = false;
			runtime.matched.store(false, std::memory_order_release);

			if (wasArmed)
				Log::Write("TimedRideHook: disarmed ({}).", reason);
		}

		// Script-thread only. Full removal + MinHook uninitialize.
		void Teardown(Runtime& runtime, std::string_view reason)
		{
			Disarm(runtime, reason);

			if (runtime.hookCreated && runtime.timedRideChallengeCheckAddress)
			{
				MH_STATUS status = MH_RemoveHook(runtime.timedRideChallengeCheckAddress);
				if (status != MH_OK && status != MH_ERROR_NOT_CREATED)
					Log::Write("TimedRideHook: MH_RemoveHook failed ({})", MH_StatusToString(status));
			}

			if (runtime.minHookInitialized)
				MH_Uninitialize();

			const bool wasCreated = runtime.hookCreated;
			runtime.minHookInitialized = false;
			runtime.hookCreated = false;
			runtime.timedRideChallengeCheckAddress = nullptr;
			runtime.originalTimedRideChallengeCheck = nullptr;
			runtime.unlockChallenge = nullptr;

			if (wasCreated)
				Log::Write("TimedRideHook: removed and uninitialized ({}).", reason);
		}
	}

	bool Initialize()
	{
		return CreateHook(GetRuntime());
	}

	bool Arm(TimedRideGoalIndex targetTimedRideGoalIndex)
	{
		if (targetTimedRideGoalIndex == 0)
		{
			Log::Write("TimedRideHook: refusing to arm for invalid timed-ride goal index 0.");
			return false;
		}

		Runtime& runtime = GetRuntime();
		// Normally a no-op (Initialize() ran at script start); retries the
		// scan only if that attempt failed.
		if (!CreateHook(runtime))
			return false;

		runtime.matched.store(false, std::memory_order_release);
		runtime.targetTimedRideGoalIndex.store(targetTimedRideGoalIndex, std::memory_order_release);

		MH_STATUS enableStatus = MH_EnableHook(runtime.timedRideChallengeCheckAddress);
		if (enableStatus != MH_OK && enableStatus != MH_ERROR_ENABLED)
		{
			Log::Write("TimedRideHook: MH_EnableHook failed for timed-ride goal index {:#x} ({})",
				targetTimedRideGoalIndex, MH_StatusToString(enableStatus));
			Disarm(runtime, "enable failure");
			return false;
		}

		runtime.armed = true;
		runtime.armedAtMs = GetTickCount64();
		Log::Write("TimedRideHook: armed for timed-ride goal index {:#x} (waiting up to {} ms for the engine's next check).",
			targetTimedRideGoalIndex, kArmTimeoutMs);
		return true;
	}

	void Update()
	{
		Runtime& runtime = GetRuntime();
		if (!runtime.armed)
			return;

		if (runtime.matched.load(std::memory_order_acquire))
			Disarm(runtime, "target unlocked");
		else if (GetTickCount64() - runtime.armedAtMs > kArmTimeoutMs)
			Disarm(runtime, "timed out without the engine checking the target goal");
	}

	void Uninstall()
	{
		Teardown(GetRuntime(), "module unload");
	}
}
