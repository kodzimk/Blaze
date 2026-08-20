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

		// Fire-and-forget clip (no Sound GameObject required).
		static bool PlayOneShot(const std::string& path, float volume = 1.0f);
		static void StopOneShots();
		static void Update(); // frees finished one-shots + applies falloff

		static void SetListenerPosition(float x, float y);
		static float ComputeFalloffVolume(GameObject* object);

		static void OnPlayModeStart(const std::vector<GameObject*>& objects);
		static void OnPlayModeStop();

		static void RemoveObject(GameObject* object);
	};
}
