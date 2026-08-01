#include "ComponentPresetAsset.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Logging/Log.h"

#include <fstream>
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

namespace Blaze
{
	static bool ShouldSkipPresetKey(const std::string& key)
	{
		return key == "name" || key == "type" || key == "pos" || key == "rot" ||
			key == "scale" || key == "parent" || key == "drives" || key == "tiles";
	}

	bool ComponentPresetAsset::LoadFromFile(const std::string& path)
	{
		std::ifstream in(path);
		if (!in.is_open())
		{
			BZ_INFO("Could not open component preset: {0}", path);
			return false;
		}

		std::string header;
		std::getline(in, header);
		while (!header.empty() && (header.back() == '\r' || header.back() == '\n'))
			header.pop_back();
		if (header.rfind("BlazeComponentPreset", 0) != 0)
		{
			BZ_INFO("Not a Blaze component preset: {0}", path);
			return false;
		}

		m_Path = path;
		m_PresetName = fs::path(path).stem().string();

		std::string line;
		while (std::getline(in, line))
		{
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
				line.pop_back();
			if (line.rfind("presetname ", 0) == 0)
			{
				m_PresetName = line.substr(11);
				while (!m_PresetName.empty() && (m_PresetName.front() == ' ' || m_PresetName.front() == '\t'))
					m_PresetName.erase(m_PresetName.begin());
				break;
			}
			if (line == "object")
				break;
		}
		return true;
	}

	bool ComponentPresetAsset::SaveToFile(const std::string& path) const
	{
		std::error_code ec;
		fs::create_directories(fs::path(path).parent_path(), ec);

		std::ofstream out(path, std::ios::trunc);
		if (!out.is_open())
		{
			BZ_INFO("Could not write component preset: {0}", path);
			return false;
		}

		const std::string name = m_PresetName.empty() ? fs::path(path).stem().string() : m_PresetName;
		out << "BlazeComponentPreset 1\n";
		out << "presetname " << name << "\n";
		out << "object\n";
		out << "layer 0\n";
		out << "visible 1\n";
		out << "color 1 1 1 1\n";
		out << "end\n";
		return true;
	}

	bool ComponentPresetAsset::SaveFromObject(GameObject* object, const std::string& path)
	{
		if (!object || path.empty())
			return false;

		std::error_code ec;
		fs::create_directories(fs::path(path).parent_path(), ec);

		std::ofstream out(path, std::ios::trunc);
		if (!out.is_open())
		{
			BZ_INFO("Could not write component preset: {0}", path);
			return false;
		}

		const std::string name = object->GetName().empty()
			? fs::path(path).stem().string()
			: object->GetName();

		out << "BlazeComponentPreset 1\n";
		out << "presetname " << name << "\n";
		Project::WriteGameObject(out, object, -1, Project::GameObjectWriteFlags::ComponentPreset());

		m_Path = path;
		m_PresetName = name;
		BZ_INFO("Component preset saved: {0}", path);
		return true;
	}

	bool ComponentPresetAsset::ApplyFromStream(std::istream& in, GameObject* object)
	{
		if (!object)
			return false;

		std::string header;
		std::getline(in, header);
		while (!header.empty() && (header.back() == '\r' || header.back() == '\n'))
			header.pop_back();
		if (header.rfind("BlazeComponentPreset", 0) != 0)
		{
			BZ_INFO("Not a Blaze component preset stream");
			return false;
		}

		std::string line;
		int applied = 0;
		while (std::getline(in, line))
		{
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
				line.pop_back();
			if (line.empty() || line == "object" || line == "end")
				continue;
			if (line.rfind("presetname ", 0) == 0)
				continue;

			std::string key;
			{
				size_t i = 0;
				while (i < line.size() && !std::isspace((unsigned char)line[i]))
					++i;
				key = line.substr(0, i);
			}
			if (ShouldSkipPresetKey(key))
				continue;

			if (Project::ApplyGameObjectProperty(object, line, nullptr))
				++applied;
		}

		BZ_INFO("Applied component preset ({0} properties) to '{1}'", applied, object->GetName());
		return true;
	}

	bool ComponentPresetAsset::ApplyToObject(const std::string& path, GameObject* object)
	{
		if (!object || path.empty())
			return false;

		std::ifstream in(path);
		if (!in.is_open())
		{
			BZ_INFO("Could not open component preset: {0}", path);
			return false;
		}
		return ApplyFromStream(in, object);
	}

	ComponentPresetAsset ComponentPresetAsset::CreateDefault(const std::string& name)
	{
		ComponentPresetAsset asset;
		asset.m_PresetName = name.empty() ? "Preset" : name;
		return asset;
	}
}
