#pragma once
#include "bzpch.h"
#include "Blaze/Core/Core.h"

namespace Blaze
{
	// Unreal-style on-screen debug prints (visible on the Game view while playing).
	class BLAZE_API OnScreenDebug
	{
	public:
		static void Print(const std::string& message, float durationSeconds = 4.0f,
			float r = 1.0f, float g = 0.9f, float b = 0.1f, float a = 1.0f);
		static void Clear();
		static void Update(float dt);
		static void DrawOverItem(); // call right after the Game view Image()
	};
}
