#pragma once
#include "Blaze/Core/Core.h"
#include <functional>
#include <string>

#define SOL_SAFE_FUNCTIONS 1
#define SOL_SAFE_USERTYPE 1
#define SOL_EXCEPTIONS_SAFE_PROPAGATION 1
#include "sol.hpp"

namespace Blaze
{
	class GameObject;

	// UE-style Delay / SetTimer / Tween for Lua play-mode scripts.
	class BLAZE_API ScriptTimers
	{
	public:
		static void Update(float dt, const std::function<void(GameObject*)>& setOwner = {});
		static void ClearAll();
		static void ClearForObject(GameObject* object);

		static int Delay(float seconds, const sol::protected_function& callback, GameObject* owner);
		static int SetTimer(float rate, const sol::protected_function& callback, bool looping, GameObject* owner);
		static bool ClearTimer(int handle);
		static bool IsTimerActive(int handle);

		static int TweenFloat(float from, float to, float duration,
			const sol::protected_function& onUpdate,
			const sol::protected_function& onComplete,
			const std::string& ease,
			GameObject* owner);
		static int TweenVec2(float x0, float y0, float x1, float y1, float duration,
			const sol::protected_function& onUpdate,
			const sol::protected_function& onComplete,
			const std::string& ease,
			GameObject* owner);

		static float EvaluateEase(const std::string& ease, float t);
	};
}
