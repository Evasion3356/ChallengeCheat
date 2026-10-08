#include "ChallengeCheatLog.h"

#include <atomic>

namespace ChallengeCheat::Log
{
	namespace
	{
		std::atomic<Sink> g_sink{ nullptr };
	}

	void SetSink(Sink sink)
	{
		g_sink.store(sink);
	}

	void Emit(std::string_view line)
	{
		if (const Sink sink = g_sink.load())
			sink(line);
	}
}
