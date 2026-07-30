#pragma once
#include "bzpch.h"
#include "Blaze/Core/Core.h"

namespace Blaze
{
	class GameObject;

	class BLAZE_API AudioEngine
	{
	public:
		static bool Init();
		static void Shutdown();
		static bool IsReady();

		static void Play(GameObject* object);
		static void Stop(GameObject* object);
		static void StopAll();
		static bool IsPlaying(GameObject* object);
		static void SyncVolume(GameObject* object);
		static void SyncLoop(GameObject* object);

		static void OnPlayModeStart(const std::vector<GameObject*>& objects);
		static void OnPlayModeStop();

		static void RemoveObject(GameObject* object);
	};
}
