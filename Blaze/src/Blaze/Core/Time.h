#pragma once
#include "Blaze/Core/Core.h"

namespace Blaze
{
	// Global play-time clock. Hosts call Tick(rawDt) once per frame;
	// gameplay systems should use GetDeltaTime() (respects scale + pause).
	class BLAZE_API Time
	{
	public:
		static void Tick(float rawDeltaSeconds);
		static void Reset();

		static float GetDeltaTime();
		static float GetUnscaledDeltaTime();
		static float GetTime();
		static float GetUnscaledTime();

		static float GetTimeScale();
		static void SetTimeScale(float scale);

		static bool IsPaused();
		static void SetPaused(bool paused);
		static void Pause();
		static void Unpause();

	private:
		static float s_DeltaTime;
		static float s_UnscaledDeltaTime;
		static float s_Time;
		static float s_UnscaledTime;
		static float s_TimeScale;
		static bool s_Paused;
	};
}
