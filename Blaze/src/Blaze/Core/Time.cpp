#include "Time.h"
#include <algorithm>
#include <cmath>

namespace Blaze
{
	float Time::s_DeltaTime = 0.0f;
	float Time::s_UnscaledDeltaTime = 0.0f;
	float Time::s_Time = 0.0f;
	float Time::s_UnscaledTime = 0.0f;
	float Time::s_TimeScale = 1.0f;
	bool Time::s_Paused = false;

	void Time::Tick(float rawDeltaSeconds)
	{
		float raw = rawDeltaSeconds;
		if (!std::isfinite(raw) || raw < 0.0f)
			raw = 0.0f;
		if (raw > 0.1f)
			raw = 0.1f;

		s_UnscaledDeltaTime = raw;
		s_UnscaledTime += raw;

		const float scale = (s_TimeScale < 0.0f) ? 0.0f : s_TimeScale;
		s_DeltaTime = s_Paused ? 0.0f : raw * scale;
		s_Time += s_DeltaTime;
	}

	void Time::Reset()
	{
		s_DeltaTime = 0.0f;
		s_UnscaledDeltaTime = 0.0f;
		s_Time = 0.0f;
		s_UnscaledTime = 0.0f;
		s_TimeScale = 1.0f;
		s_Paused = false;
	}

	float Time::GetDeltaTime() { return s_DeltaTime; }
	float Time::GetUnscaledDeltaTime() { return s_UnscaledDeltaTime; }
	float Time::GetTime() { return s_Time; }
	float Time::GetUnscaledTime() { return s_UnscaledTime; }

	float Time::GetTimeScale() { return s_TimeScale; }

	void Time::SetTimeScale(float scale)
	{
		if (!std::isfinite(scale))
			scale = 1.0f;
		s_TimeScale = std::max(0.0f, scale);
	}

	bool Time::IsPaused() { return s_Paused; }
	void Time::SetPaused(bool paused) { s_Paused = paused; }
	void Time::Pause() { s_Paused = true; }
	void Time::Unpause() { s_Paused = false; }
}
