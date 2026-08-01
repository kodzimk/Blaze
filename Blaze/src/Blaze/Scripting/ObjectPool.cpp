#include "ObjectPool.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Renderer/Renderer.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/PrefabAsset.h"
#include "Blaze/Physics/PhysicsWorld2D.h"
#include "Blaze/Audio/AudioEngine.h"
#include "Blaze/Scripting/ScriptEngine.h"
#include "Blaze/Scripting/ScriptTimers.h"
#include "Blaze/Logging/Log.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace Blaze
{
	Renderer* ObjectPool::s_Renderer = nullptr;
	PhysicsWorld2D* ObjectPool::s_Physics = nullptr;
	std::vector<float> ObjectPool::s_Vertices;
	std::vector<unsigned int> ObjectPool::s_Indices;
	std::unordered_map<std::string, ObjectPool::Pool> ObjectPool::s_Pools;
	std::unordered_map<GameObject*, std::string> ObjectPool::s_ObjectToPool;

	void ObjectPool::SetContext(Renderer* renderer,
		PhysicsWorld2D* physics,
		const std::vector<float>& vertices,
		const std::vector<unsigned int>& indices)
	{
		s_Renderer = renderer;
		s_Physics = physics;
		s_Vertices = vertices;
		s_Indices = indices;
	}

	std::string ObjectPool::ResolvePath(const std::string& path)
	{
		if (path.empty())
			return {};

		std::error_code ec;
		if (fs::exists(path, ec))
			return fs::path(path).generic_string();

		if (!Project::GetDirectory().empty())
		{
			fs::path full = fs::path(Project::GetDirectory()) / path;
			if (fs::exists(full, ec))
				return full.generic_string();
		}
		return fs::path(path).generic_string();
	}

	ObjectPool::Pool& ObjectPool::GetOrCreatePool(const std::string& resolvedPath)
	{
		auto it = s_Pools.find(resolvedPath);
		if (it != s_Pools.end())
			return it->second;
		Pool pool;
		pool.path = resolvedPath;
		auto inserted = s_Pools.emplace(resolvedPath, std::move(pool));
		return inserted.first->second;
	}

	void ObjectPool::DeactivateForPool(GameObject* root, bool callDespawn)
	{
		if (!root)
			return;

		std::vector<GameObject*> stack = { root };
		while (!stack.empty())
		{
			GameObject* object = stack.back();
			stack.pop_back();
			if (!object)
				continue;

			if (callDespawn)
				ScriptEngine::CallOnDespawn(object);
			ScriptTimers::ClearForObject(object);
			AudioEngine::Stop(object);
			if (s_Physics)
				s_Physics->SetVelocity(object, glm::vec2(0.0f));

			object->SetActive(false, false);
			for (GameObject* child : object->GetChildren())
			{
				if (child)
					stack.push_back(child);
			}
		}
	}

	void ObjectPool::ActivateForPool(GameObject* root, const glm::vec3& worldPos)
	{
		if (!root)
			return;

		std::vector<GameObject*> stack = { root };
		while (!stack.empty())
		{
			GameObject* object = stack.back();
			stack.pop_back();
			if (!object)
				continue;
			object->SetActive(true, false);
			for (GameObject* child : object->GetChildren())
			{
				if (child)
					stack.push_back(child);
			}
		}

		glm::vec3 pos = worldPos;
		root->SetPosition(pos);
		ScriptEngine::StorePlayTransformPublic(root);

		stack = { root };
		while (!stack.empty())
		{
			GameObject* object = stack.back();
			stack.pop_back();
			if (!object)
				continue;
			ScriptEngine::CallOnSpawn(object);
			for (GameObject* child : object->GetChildren())
			{
				if (child)
					stack.push_back(child);
			}
		}
	}

	GameObject* ObjectPool::CreateInactiveInstance(Pool& pool)
	{
		if (!s_Renderer || pool.path.empty())
			return nullptr;

		std::vector<GameObject*> created;
		GameObject* root = PrefabAsset::Instantiate(
			pool.path, *s_Renderer, s_Vertices, s_Indices, glm::vec3(0.0f), &created);
		if (!root)
			return nullptr;

		for (GameObject* object : created)
		{
			if (!object)
				continue;
			ScriptEngine::RegisterLiveObject(object);
			ScriptEngine::AttachScriptIfNeeded(object, true);
		}

		pool.members.push_back(root);
		s_ObjectToPool[root] = pool.path;
		DeactivateForPool(root, false);
		return root;
	}

	bool ObjectPool::Prewarm(const std::string& prefabPath, int count)
	{
		if (!s_Renderer || count <= 0)
			return false;

		const std::string resolved = ResolvePath(prefabPath);
		if (resolved.empty())
			return false;

		Pool& pool = GetOrCreatePool(resolved);
		int created = 0;
		for (int i = 0; i < count; ++i)
		{
			GameObject* root = CreateInactiveInstance(pool);
			if (!root)
				break;
			pool.free.push_back(root);
			++created;
		}
		BZ_INFO("Pool prewarm '{0}': {1}/{2}", resolved, created, count);
		return created > 0;
	}

	GameObject* ObjectPool::Acquire(const std::string& prefabPath, float x, float y, float z)
	{
		if (!s_Renderer)
			return nullptr;

		const std::string resolved = ResolvePath(prefabPath);
		if (resolved.empty())
			return nullptr;

		Pool& pool = GetOrCreatePool(resolved);
		GameObject* root = nullptr;
		if (!pool.free.empty())
		{
			root = pool.free.back();
			pool.free.pop_back();
		}
		else
		{
			root = CreateInactiveInstance(pool);
		}

		if (!root)
			return nullptr;

		ActivateForPool(root, glm::vec3(x, y, z));
		return root;
	}

	bool ObjectPool::Release(GameObject* object)
	{
		if (!object)
			return false;

		GameObject* root = object;
		while (root->GetParent())
			root = root->GetParent();

		auto it = s_ObjectToPool.find(root);
		if (it == s_ObjectToPool.end())
			return false;

		auto poolIt = s_Pools.find(it->second);
		if (poolIt == s_Pools.end())
			return false;

		Pool& pool = poolIt->second;
		for (GameObject* freeObj : pool.free)
		{
			if (freeObj == root)
				return true; // already free
		}

		DeactivateForPool(root, true);
		pool.free.push_back(root);
		return true;
	}

	bool ObjectPool::Owns(GameObject* object)
	{
		if (!object)
			return false;
		GameObject* root = object;
		while (root->GetParent())
			root = root->GetParent();
		return s_ObjectToPool.find(root) != s_ObjectToPool.end();
	}

	int ObjectPool::GetFreeCount(const std::string& prefabPath)
	{
		const std::string resolved = ResolvePath(prefabPath);
		auto it = s_Pools.find(resolved);
		return it == s_Pools.end() ? 0 : (int)it->second.free.size();
	}

	int ObjectPool::GetTotalCount(const std::string& prefabPath)
	{
		const std::string resolved = ResolvePath(prefabPath);
		auto it = s_Pools.find(resolved);
		return it == s_Pools.end() ? 0 : (int)it->second.members.size();
	}

	void ObjectPool::Clear()
	{
		if (s_Renderer)
		{
			for (auto& entry : s_Pools)
			{
				for (GameObject* root : entry.second.members)
				{
					if (!root)
						continue;

					// Destroy children first, then root.
					std::vector<GameObject*> doomed;
					std::vector<GameObject*> stack = { root };
					while (!stack.empty())
					{
						GameObject* object = stack.back();
						stack.pop_back();
						if (!object)
							continue;
						for (GameObject* child : object->GetChildren())
						{
							if (child)
								stack.push_back(child);
						}
						doomed.push_back(object);
					}
					for (auto it = doomed.rbegin(); it != doomed.rend(); ++it)
					{
						if (s_Physics)
							s_Physics->RemoveObject(*it);
						s_Renderer->DestroyObject(*it);
					}
				}
			}
		}

		s_Pools.clear();
		s_ObjectToPool.clear();
	}
}
