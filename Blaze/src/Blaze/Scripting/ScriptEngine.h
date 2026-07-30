#pragma once
#include "bzpch.h"
#include "Blaze/Core/Core.h"
#include "Blaze/Physics/PhysicsWorld2D.h"

namespace Blaze
{
	class GameObject;
	class Renderer;

	class BLAZE_API ScriptEngine
	{
	public:
		static bool Init();
		static void Shutdown();
		static bool IsReady();

		// Returns false if any script fails to compile — play should be cancelled.
		static bool OnPlayModeStart(const std::vector<GameObject*>& objects);
		static void OnUpdate(float dt, const std::vector<GameObject*>& objects);
		static void OnCollisionEvents(const std::vector<CollisionEvent>& events);
		static void OnPlayModeStop();

		static void RemoveObject(GameObject* object);
		static void QueueDestroy(GameObject* object);
		static std::vector<GameObject*> ConsumePendingDestroys();

		// Lua error popup (also printed to the console when Start is pressed)
		static void DrawErrorWindow();
		static bool HasScriptErrors();
		static void ClearScriptErrors();
	};
}
