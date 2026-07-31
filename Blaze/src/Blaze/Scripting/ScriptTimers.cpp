#include "ScriptTimers.h"
#include "Blaze/Logging/Log.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

namespace Blaze
{
	namespace
	{
		enum class EaseKind { Linear = 0, In, Out, InOut };
		enum class EntryKind { Timer = 0, TweenFloat, TweenVec2 };

		struct TimerEntry
		{
			EntryKind kind = EntryKind::Timer;
			int id = 0;
			bool alive = true;
			bool looping = false;
			float timeLeft = 0.0f;
			float rate = 0.0f;
			GameObject* owner = nullptr;
			sol::protected_function callback;

			float duration = 0.0f;
			float elapsed = 0.0f;
			float from = 0.0f;
			float to = 0.0f;
			float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
			EaseKind ease = EaseKind::Linear;
			sol::protected_function onUpdate;
			sol::protected_function onComplete;
		};

		std::vector<TimerEntry> s_Entries;
		int s_NextId = 1;
		bool s_Updating = false;
		std::vector<int> s_PendingClear;

		EaseKind ParseEase(std::string e)
		{
			for (char& c : e)
				c = (char)std::tolower((unsigned char)c);
			if (e.empty() || e == "linear" || e == "lerp")
				return EaseKind::Linear;
			if (e == "in" || e == "easein" || e == "inquad")
				return EaseKind::In;
			if (e == "out" || e == "easeout" || e == "outquad")
				return EaseKind::Out;
			if (e == "inout" || e == "easeinout" || e == "inoutquad")
				return EaseKind::InOut;
			return EaseKind::Linear;
		}

		float ApplyEase(EaseKind kind, float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			switch (kind)
			{
			case EaseKind::In: return t * t;
			case EaseKind::Out:
			{
				const float u = 1.0f - t;
				return 1.0f - u * u;
			}
			case EaseKind::InOut:
				return (t < 0.5f) ? (2.0f * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f);
			default: return t;
			}
		}

		TimerEntry* FindEntry(int id)
		{
			for (TimerEntry& e : s_Entries)
			{
				if (e.id == id && e.alive)
					return &e;
			}
			return nullptr;
		}

		void CallVoid(sol::protected_function& fn, const char* phase)
		{
			if (!fn.valid())
				return;
			sol::protected_function_result result = fn();
			if (!result.valid())
			{
				sol::error err = result;
				BZ_INFO("ScriptTimers {0}: {1}", phase, err.what());
			}
		}

		template<typename... Args>
		void CallArgs(sol::protected_function& fn, const char* phase, Args&&... args)
		{
			if (!fn.valid())
				return;
			sol::protected_function_result result = fn(std::forward<Args>(args)...);
			if (!result.valid())
			{
				sol::error err = result;
				BZ_INFO("ScriptTimers {0}: {1}", phase, err.what());
			}
		}
	}

	float ScriptTimers::EvaluateEase(const std::string& ease, float t)
	{
		return ApplyEase(ParseEase(ease), t);
	}

	int ScriptTimers::Delay(float seconds, const sol::protected_function& callback, GameObject* owner)
	{
		if (!callback.valid())
			return 0;
		TimerEntry entry;
		entry.kind = EntryKind::Timer;
		entry.id = s_NextId++;
		entry.alive = true;
		entry.looping = false;
		entry.timeLeft = std::max(seconds, 0.0f);
		entry.rate = entry.timeLeft;
		entry.owner = owner;
		entry.callback = callback;
		s_Entries.push_back(std::move(entry));
		return s_Entries.back().id;
	}

	int ScriptTimers::SetTimer(float rate, const sol::protected_function& callback, bool looping, GameObject* owner)
	{
		if (!callback.valid())
			return 0;
		TimerEntry entry;
		entry.kind = EntryKind::Timer;
		entry.id = s_NextId++;
		entry.alive = true;
		entry.looping = looping;
		entry.rate = std::max(rate, 0.0f);
		entry.timeLeft = entry.rate;
		entry.owner = owner;
		entry.callback = callback;
		s_Entries.push_back(std::move(entry));
		return s_Entries.back().id;
	}

	bool ScriptTimers::ClearTimer(int handle)
	{
		if (handle <= 0)
			return false;
		if (s_Updating)
		{
			s_PendingClear.push_back(handle);
			return FindEntry(handle) != nullptr || true;
		}
		TimerEntry* e = FindEntry(handle);
		if (!e)
			return false;
		e->alive = false;
		return true;
	}

	bool ScriptTimers::IsTimerActive(int handle)
	{
		return FindEntry(handle) != nullptr;
	}

	int ScriptTimers::TweenFloat(float from, float to, float duration,
		const sol::protected_function& onUpdate,
		const sol::protected_function& onComplete,
		const std::string& ease,
		GameObject* owner)
	{
		TimerEntry entry;
		entry.kind = EntryKind::TweenFloat;
		entry.id = s_NextId++;
		entry.alive = true;
		entry.duration = std::max(duration, 0.0f);
		entry.elapsed = 0.0f;
		entry.from = from;
		entry.to = to;
		entry.ease = ParseEase(ease);
		entry.onUpdate = onUpdate;
		entry.onComplete = onComplete;
		entry.owner = owner;
		s_Entries.push_back(std::move(entry));

		// Fire first sample immediately (t=0), UE timeline style.
		TimerEntry& live = s_Entries.back();
		CallArgs(live.onUpdate, "Tween", from, 0.0f);
		if (live.duration <= 1e-6f)
		{
			CallArgs(live.onUpdate, "Tween", to, 1.0f);
			CallVoid(live.onComplete, "TweenComplete");
			live.alive = false;
		}
		return live.id;
	}

	int ScriptTimers::TweenVec2(float x0, float y0, float x1, float y1, float duration,
		const sol::protected_function& onUpdate,
		const sol::protected_function& onComplete,
		const std::string& ease,
		GameObject* owner)
	{
		TimerEntry entry;
		entry.kind = EntryKind::TweenVec2;
		entry.id = s_NextId++;
		entry.alive = true;
		entry.duration = std::max(duration, 0.0f);
		entry.elapsed = 0.0f;
		entry.x0 = x0; entry.y0 = y0;
		entry.x1 = x1; entry.y1 = y1;
		entry.ease = ParseEase(ease);
		entry.onUpdate = onUpdate;
		entry.onComplete = onComplete;
		entry.owner = owner;
		s_Entries.push_back(std::move(entry));

		TimerEntry& live = s_Entries.back();
		CallArgs(live.onUpdate, "Tween", x0, y0, 0.0f);
		if (live.duration <= 1e-6f)
		{
			CallArgs(live.onUpdate, "Tween", x1, y1, 1.0f);
			CallVoid(live.onComplete, "TweenComplete");
			live.alive = false;
		}
		return live.id;
	}

	void ScriptTimers::ClearAll()
	{
		s_Entries.clear();
		s_PendingClear.clear();
	}

	void ScriptTimers::ClearForObject(GameObject* object)
	{
		if (!object)
			return;
		for (TimerEntry& e : s_Entries)
		{
			if (e.owner == object)
				e.alive = false;
		}
	}

	void ScriptTimers::Update(float dt, const std::function<void(GameObject*)>& setOwner)
	{
		if (dt < 0.0f)
			dt = 0.0f;

		s_Updating = true;
		for (TimerEntry& e : s_Entries)
		{
			if (!e.alive)
				continue;

			if (setOwner)
				setOwner(e.owner);

			if (e.kind == EntryKind::Timer)
			{
				e.timeLeft -= dt;
				if (e.timeLeft > 0.0f)
					continue;

				CallVoid(e.callback, "Timer");
				if (!e.alive)
					continue;

				if (e.looping && e.rate > 0.0f)
				{
					do { e.timeLeft += e.rate; } while (e.timeLeft <= 0.0f);
				}
				else
				{
					e.alive = false;
				}
			}
			else
			{
				e.elapsed += dt;
				float t = (e.duration <= 1e-6f) ? 1.0f : (e.elapsed / e.duration);
				const bool done = t >= 1.0f;
				t = ApplyEase(e.ease, std::min(t, 1.0f));

				if (e.kind == EntryKind::TweenFloat)
				{
					const float v = e.from + (e.to - e.from) * t;
					CallArgs(e.onUpdate, "Tween", v, t);
				}
				else
				{
					const float x = e.x0 + (e.x1 - e.x0) * t;
					const float y = e.y0 + (e.y1 - e.y0) * t;
					CallArgs(e.onUpdate, "Tween", x, y, t);
				}

				if (done)
				{
					CallVoid(e.onComplete, "TweenComplete");
					e.alive = false;
				}
			}
		}

		if (setOwner)
			setOwner(nullptr);
		s_Updating = false;

		for (int id : s_PendingClear)
		{
			if (TimerEntry* e = FindEntry(id))
				e->alive = false;
		}
		s_PendingClear.clear();

		s_Entries.erase(std::remove_if(s_Entries.begin(), s_Entries.end(),
			[](const TimerEntry& e) { return !e.alive; }), s_Entries.end());
	}
}
