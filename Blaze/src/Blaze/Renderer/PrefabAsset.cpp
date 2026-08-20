#include "PrefabAsset.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Renderer/Renderer.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Logging/Log.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace Blaze
{
	void PrefabAsset::CollectSubtree(GameObject* root, std::vector<GameObject*>& out)
	{
		if (!root)
			return;
		out.push_back(root);
		for (GameObject* child : root->GetChildren())
			CollectSubtree(child, out);
	}

	void PrefabAsset::WriteSubtree(std::ostream& out, GameObject* root)
	{
		if (!root)
			return;

		std::vector<GameObject*> objects;
		CollectSubtree(root, objects);

		std::unordered_map<GameObject*, int> indexOf;
		indexOf.reserve(objects.size());
		for (int i = 0; i < (int)objects.size(); ++i)
			indexOf[objects[i]] = i;

		out << "BlazePrefab 1\n";
		out << "rootname " << root->GetName() << "\n";
		for (GameObject* object : objects)
		{
			int parentIndex = -1;
			if (GameObject* parent = object->GetParent())
			{
				auto it = indexOf.find(parent);
				if (it != indexOf.end())
					parentIndex = it->second;
			}
			Project::WriteGameObject(out, object, parentIndex);
		}
	}

	GameObject* PrefabAsset::InstantiateFromStream(std::istream& in,
		Renderer& renderer,
		const std::vector<float>& vertices,
		const std::vector<unsigned int>& indices,
		const glm::vec3& worldPos,
		std::vector<GameObject*>* outCreated)
	{
		std::string header;
		std::getline(in, header);
		while (!header.empty() && (header.back() == '\r' || header.back() == '\n'))
			header.pop_back();
		if (header.rfind("BlazePrefab", 0) != 0)
		{
			BZ_INFO("Not a Blaze prefab stream");
			return nullptr;
		}

		const size_t baseIndex = renderer.GetObjects().size();
		GameObject* current = nullptr;
		GameObject* root = nullptr;
		std::vector<GameObject*> created;
		std::vector<std::pair<GameObject*, int>> pendingParents;
		std::string line;

		while (std::getline(in, line))
		{
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
				line.pop_back();
			if (line.empty())
				continue;

			if (line.rfind("rootname ", 0) == 0)
				continue;

			if (line == "object")
			{
				renderer.CreateObject(vertices, indices);
				current = renderer.GetObjects().back();
				created.push_back(current);
				if (!root)
					root = current;
				continue;
			}
			if (!current)
				continue;
			if (line == "end")
			{
				current = nullptr;
				continue;
			}

			int parentIndex = -1;
			if (Project::ApplyGameObjectProperty(current, line, &parentIndex))
			{
				if (parentIndex >= 0)
					pendingParents.push_back({ current, parentIndex });
			}
		}

		for (const auto& entry : pendingParents)
		{
			if (!entry.first || entry.first == root)
				continue;
			if (entry.second < 0 || entry.second >= (int)created.size())
				continue;
			GameObject* parent = created[entry.second];
			if (!parent || parent == entry.first)
				continue;
			entry.first->SetParent(parent, false);
		}

		if (root)
		{
			glm::vec3 pos = worldPos;
			root->SetPosition(pos);
		}

		for (GameObject* object : created)
		{
			if (!object)
				continue;
			// Force matrix refresh after parenting + root placement.
			object->SetLocalPosition(object->GetLocalPosition());
		}

		if (outCreated)
			*outCreated = created;

		(void)baseIndex;
		BZ_INFO("Prefab instantiated ({0} objects)", created.size());
		return root;
	}

	bool PrefabAsset::LoadFromFile(const std::string& path)
	{
		std::ifstream in(path);
		if (!in.is_open())
		{
			BZ_INFO("Could not open prefab: {0}", path);
			return false;
		}

		std::string header;
		std::getline(in, header);
		while (!header.empty() && (header.back() == '\r' || header.back() == '\n'))
			header.pop_back();
		if (header.rfind("BlazePrefab", 0) != 0)
		{
			BZ_INFO("Not a Blaze prefab: {0}", path);
			return false;
		}

		m_Path = path;
		m_RootName = fs::path(path).stem().string();

		std::string line;
		while (std::getline(in, line))
		{
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
				line.pop_back();
			if (line.rfind("rootname ", 0) == 0)
			{
				m_RootName = line.substr(9);
				while (!m_RootName.empty() && (m_RootName.front() == ' ' || m_RootName.front() == '\t'))
					m_RootName.erase(m_RootName.begin());
				break;
			}
			if (line == "object")
				break;
		}
		return true;
	}

	bool PrefabAsset::SaveToFile(const std::string& path) const
	{
		std::error_code ec;
		fs::create_directories(fs::path(path).parent_path(), ec);

		std::ofstream out(path, std::ios::trunc);
		if (!out.is_open())
		{
			BZ_INFO("Could not write prefab: {0}", path);
			return false;
		}

		const std::string name = m_RootName.empty() ? fs::path(path).stem().string() : m_RootName;
		out << "BlazePrefab 1\n";
		out << "rootname " << name << "\n";
		out << "object\n";
		out << "name " << name << "\n";
		out << "type " << (int)ObjectType::Empty << "\n";
		out << "pos 0 0 0\n";
		out << "rot 0\n";
		out << "scale 1 1\n";
		out << "layer 0\n";
		out << "visible 1\n";
		out << "color 1 1 1 1\n";
		out << "end\n";
		return true;
	}

	bool PrefabAsset::SaveFromRoot(GameObject* root, const std::string& path)
	{
		if (!root || path.empty())
			return false;

		std::error_code ec;
		fs::create_directories(fs::path(path).parent_path(), ec);

		std::ofstream out(path, std::ios::trunc);
		if (!out.is_open())
		{
			BZ_INFO("Could not write prefab: {0}", path);
			return false;
		}

		WriteSubtree(out, root);
		m_Path = path;
		m_RootName = root->GetName();
		BZ_INFO("Prefab saved: {0}", path);
		return true;
	}

	GameObject* PrefabAsset::Instantiate(const std::string& path,
		Renderer& renderer,
		const std::vector<float>& vertices,
		const std::vector<unsigned int>& indices,
		const glm::vec3& worldPos,
		std::vector<GameObject*>* outCreated)
	{
		std::ifstream in(path);
		if (!in.is_open())
		{
			BZ_INFO("Could not open prefab: {0}", path);
			return nullptr;
		}
		return InstantiateFromStream(in, renderer, vertices, indices, worldPos, outCreated);
	}

	GameObject* PrefabAsset::Duplicate(GameObject* root,
		Renderer& renderer,
		const std::vector<float>& vertices,
		const std::vector<unsigned int>& indices,
		const glm::vec3& worldOffset,
		std::vector<GameObject*>* outCreated)
	{
		if (!root)
			return nullptr;

		std::ostringstream out;
		WriteSubtree(out, root);
		std::istringstream in(out.str());

		GameObject* originalParent = root->GetParent();
		const glm::vec3 originalWorld = root->GetPosition();

		// Spawn on top of the original first, then nudge — keeps parenting correct.
		GameObject* copy = InstantiateFromStream(in, renderer, vertices, indices, originalWorld, outCreated);
		if (!copy)
			return nullptr;

		if (originalParent)
			copy->SetParent(originalParent, true);

		glm::vec3 nudged = copy->GetPosition() + worldOffset;
		copy->SetPosition(nudged);

		if (outCreated)
		{
			for (GameObject* object : *outCreated)
			{
				if (object)
					object->SetLocalPosition(object->GetLocalPosition());
			}
		}
		else
			copy->SetLocalPosition(copy->GetLocalPosition());

		return copy;
	}

	PrefabAsset PrefabAsset::CreateDefault(const std::string& name)
	{
		PrefabAsset asset;
		asset.m_RootName = name.empty() ? "Prefab" : name;
		return asset;
	}
}
