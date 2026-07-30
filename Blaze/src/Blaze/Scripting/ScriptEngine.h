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

		static void SetRenderer(Renderer* renderer);
		static void SetPhysicsWorld(PhysicsWorld2D* physics);

		// Returns false if any script fails to compile — play should be cancelled.
		static bool OnPlayModeStart(const std::vector<GameObject*>& objects);
		static void OnUpdate(float dt, const std::vector<GameObject*>& objects);
		static void OnCollisionEvents(const std::vector<CollisionEvent>& events);
		static void OnPlayModeStop();

		static void RemoveObject(GameObject* object);
		static void QueueDestroy(GameObject* object);
		static std::vector<GameObject*> ConsumePendingDestroys();

		static GameObject* SpawnObject(const std::string& typeName,
			const std::string& name,
			const std::string& tag,
			float x, float y, float z);

		static GameObject* FindByTag(const std::string& tag);
		static std::vector<GameObject*> FindAllByTag(const std::string& tag);

		static std::string GetCurrentMapName();
		static bool RequestOpenMap(const std::string& mapNameOrPath);
		static bool ConsumePendingOpenMap(std::string& outPath);

		// Lua error popup (also printed to the console when Start is pressed)
		static void DrawErrorWindow();
		static bool HasScriptErrors();
		static void ClearScriptErrors();
	};
}
