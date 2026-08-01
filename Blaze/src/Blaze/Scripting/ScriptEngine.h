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
		// Snapshot / restore play-space locals (used so the editor scene can stay frozen).
		static void CapturePlayTransforms(const std::vector<GameObject*>& objects);
		static void ApplyStoredPlayTransforms();
		static void NotifyWidgetClicked(const std::string& widgetPath, const std::string& buttonName);

		static void RemoveObject(GameObject* object);
		static void QueueDestroy(GameObject* object);
		static std::vector<GameObject*> ConsumePendingDestroys();

		static GameObject* SpawnObject(const std::string& typeName,
			const std::string& name,
			const std::string& tag,
			float x, float y, float z);

		// Instantiates a .prefab into the live scene (play mode). Returns root or nullptr.
		static GameObject* InstantiatePrefab(const std::string& path,
			float x = 0.0f, float y = 0.0f, float z = 0.0f,
			const std::string& nameOverride = "",
			const std::string& tagOverride = "");

		// Used by ObjectPool.
		static void RegisterLiveObject(GameObject* object);
		static bool AttachScriptIfNeeded(GameObject* object, bool callStart);
		static void StorePlayTransformPublic(GameObject* object);
		static void CallOnSpawn(GameObject* object);
		static void CallOnDespawn(GameObject* object);

		static GameObject* FindByTag(const std::string& tag);
		static std::vector<GameObject*> FindAllByTag(const std::string& tag);
		static GameObject* FindByName(const std::string& name);
		static std::vector<GameObject*> FindAllByName(const std::string& name);

		static void EnableMouse(bool enabled);
		static bool IsMouseEnabled();
		static void SetCursorVisible(bool visible);
		static bool IsCursorVisible();
		static bool WantsCursorVisible();

		static void UpdateCameraFollow(float dt);

		static std::string GetCurrentMapName();
		static bool RequestOpenMap(const std::string& mapNameOrPath);
		static bool ConsumePendingOpenMap(std::string& outPath);

		static void DrawErrorWindow();
		static bool HasScriptErrors();
		static void ClearScriptErrors();
	};
}
