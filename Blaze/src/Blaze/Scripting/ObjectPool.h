#pragma once
#include "Blaze/Core/Core.h"
#include "glm.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace Blaze
{
	class GameObject;
	class Renderer;
	class PhysicsWorld2D;

	// Prefab-path object pools for play mode. Acquire/Release recycle instances;
	// scripts get OnSpawn / OnDespawn (OnStart runs once at first create).
	class BLAZE_API ObjectPool
	{
	public:
		static void SetContext(Renderer* renderer,
			PhysicsWorld2D* physics,
			const std::vector<float>& vertices,
			const std::vector<unsigned int>& indices);
		static void Clear(); // destroy all pooled objects (call on play stop)

		static bool Prewarm(const std::string& prefabPath, int count);
		static GameObject* Acquire(const std::string& prefabPath,
			float x = 0.0f, float y = 0.0f, float z = 0.0f);
		static bool Release(GameObject* object);

		static bool Owns(GameObject* object);
		static int GetFreeCount(const std::string& prefabPath);
		static int GetTotalCount(const std::string& prefabPath);

	private:
		struct Pool
		{
			std::string path;
			std::vector<GameObject*> free;
			std::vector<GameObject*> members; // roots owned by this pool
		};

		static std::string ResolvePath(const std::string& path);
		static Pool& GetOrCreatePool(const std::string& resolvedPath);
		static GameObject* CreateInactiveInstance(Pool& pool);
		static void DeactivateForPool(GameObject* root, bool callDespawn);
		static void ActivateForPool(GameObject* root, const glm::vec3& worldPos);

		static Renderer* s_Renderer;
		static PhysicsWorld2D* s_Physics;
		static std::vector<float> s_Vertices;
		static std::vector<unsigned int> s_Indices;
		static std::unordered_map<std::string, Pool> s_Pools;
		static std::unordered_map<GameObject*, std::string> s_ObjectToPool;
	};
}
