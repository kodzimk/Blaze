#pragma once
#include "Blaze/Core/Core.h"
#include "glm.hpp"
#include <string>
#include <vector>
#include <iosfwd>

namespace Blaze
{
	class GameObject;
	class Renderer;

	// Reusable GameObject subtree (.prefab). Same object-block language as maps.
	class BLAZE_API PrefabAsset
	{
	public:
		const std::string& GetPath() const { return m_Path; }
		void SetPath(const std::string& path) { m_Path = path; }
		const std::string& GetRootName() const { return m_RootName; }
		void SetRootName(const std::string& name) { m_RootName = name; }

		bool LoadFromFile(const std::string& path); // validates + stores path/rootname
		bool SaveToFile(const std::string& path) const; // empty default template
		bool SaveFromRoot(GameObject* root, const std::string& path);

		// Appends objects to the renderer. Returns the instantiated root (or nullptr).
		static GameObject* Instantiate(const std::string& path,
			Renderer& renderer,
			const std::vector<float>& vertices,
			const std::vector<unsigned int>& indices,
			const glm::vec3& worldPos = glm::vec3(0.0f),
			std::vector<GameObject*>* outCreated = nullptr);

		// In-scene clone of a root (+ children), offset in world space.
		static GameObject* Duplicate(GameObject* root,
			Renderer& renderer,
			const std::vector<float>& vertices,
			const std::vector<unsigned int>& indices,
			const glm::vec3& worldOffset = glm::vec3(50.0f, 50.0f, 0.0f),
			std::vector<GameObject*>* outCreated = nullptr);

		static PrefabAsset CreateDefault(const std::string& name = "Prefab");

		static void WriteSubtree(std::ostream& out, GameObject* root);
		static GameObject* InstantiateFromStream(std::istream& in,
			Renderer& renderer,
			const std::vector<float>& vertices,
			const std::vector<unsigned int>& indices,
			const glm::vec3& worldPos,
			std::vector<GameObject*>* outCreated = nullptr);

	private:
		static void CollectSubtree(GameObject* root, std::vector<GameObject*>& out);

		std::string m_Path;
		std::string m_RootName = "Prefab";
	};
}
