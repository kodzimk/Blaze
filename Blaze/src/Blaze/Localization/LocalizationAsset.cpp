#include "LocalizationAsset.h"
#include "Blaze/Logging/Log.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Blaze
{
	int LocalizationAsset::FindEntry(const std::string& key) const
	{
		for (int i = 0; i < (int)m_Entries.size(); ++i)
		{
			if (m_Entries[i].first == key)
				return i;
		}
		return -1;
	}

	void LocalizationAsset::SetEntry(const std::string& key, const std::string& value)
	{
		if (key.empty())
			return;
		const int index = FindEntry(key);
		if (index >= 0)
			m_Entries[index].second = value;
		else
			m_Entries.emplace_back(key, value);
	}

	bool LocalizationAsset::RemoveEntry(const std::string& key)
	{
		const int index = FindEntry(key);
		if (index < 0)
			return false;
		m_Entries.erase(m_Entries.begin() + index);
		return true;
	}

	bool LocalizationAsset::HasEntry(const std::string& key) const
	{
		return FindEntry(key) >= 0;
	}

	std::string LocalizationAsset::GetEntry(const std::string& key, const std::string& fallback) const
	{
		const int index = FindEntry(key);
		if (index < 0)
			return fallback.empty() ? key : fallback;
		return m_Entries[index].second;
	}

	bool LocalizationAsset::LoadFromFile(const std::string& path)
	{
		std::ifstream in(path);
		if (!in.is_open())
		{
			BZ_CORE_ERROR("Failed to open localization: {0}", path);
			return false;
		}

		m_Path = path;
		m_Language = "en";
		m_Entries.clear();

		std::string line;
		bool headerOk = false;
		while (std::getline(in, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty() || line[0] == '#')
				continue;

			std::istringstream stream(line);
			std::string key;
			stream >> key;
			if (key == "BlazeLoc")
			{
				headerOk = true;
				continue;
			}
			if (!headerOk)
				continue;

			if (key == "lang" || key == "language")
			{
				std::string lang;
				stream >> lang;
				if (!lang.empty())
					m_Language = lang;
			}
			else if (key == "entry")
			{
				std::string entryKey;
				stream >> entryKey;
				if (entryKey.empty())
					continue;
				std::string value;
				std::getline(stream, value);
				if (!value.empty() && value[0] == ' ')
					value.erase(value.begin());
				SetEntry(entryKey, value);
			}
		}

		if (!headerOk)
		{
			BZ_CORE_ERROR("Invalid localization header: {0}", path);
			return false;
		}
		return true;
	}

	bool LocalizationAsset::SaveToFile(const std::string& path) const
	{
		std::ofstream out(path);
		if (!out.is_open())
		{
			BZ_CORE_ERROR("Failed to save localization: {0}", path);
			return false;
		}

		out << "BlazeLoc 1\n";
		out << "lang " << (m_Language.empty() ? "en" : m_Language) << "\n";
		for (const auto& entry : m_Entries)
		{
			if (entry.first.empty())
				continue;
			out << "entry " << entry.first << " " << entry.second << "\n";
		}
		return true;
	}

	LocalizationAsset LocalizationAsset::CreateDefault(const std::string& language)
	{
		LocalizationAsset asset;
		asset.SetLanguage(language.empty() ? "en" : language);
		asset.SetEntry("ui.play", "Play");
		asset.SetEntry("ui.quit", "Quit");
		asset.SetEntry("ui.score", "Score: {0}");
		asset.SetEntry("ui.paused", "Paused");
		asset.SetEntry("world.welcome", "Welcome!");
		return asset;
	}
}
