#pragma once
#include "Blaze/Core/Core.h"
#include <string>
#include <vector>
#include <utility>

namespace Blaze
{
	class BLAZE_API LocalizationAsset
	{
	public:
		const std::string& GetPath() const { return m_Path; }
		void SetPath(const std::string& path) { m_Path = path; }

		const std::string& GetLanguage() const { return m_Language; }
		void SetLanguage(const std::string& lang) { m_Language = lang.empty() ? "en" : lang; }

		std::vector<std::pair<std::string, std::string>>& GetEntries() { return m_Entries; }
		const std::vector<std::pair<std::string, std::string>>& GetEntries() const { return m_Entries; }

		int FindEntry(const std::string& key) const;
		void SetEntry(const std::string& key, const std::string& value);
		bool RemoveEntry(const std::string& key);
		bool HasEntry(const std::string& key) const;
		std::string GetEntry(const std::string& key, const std::string& fallback = {}) const;

		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;

		static LocalizationAsset CreateDefault(const std::string& language = "en");

	private:
		std::string m_Path;
		std::string m_Language = "en";
		std::vector<std::pair<std::string, std::string>> m_Entries;
	};
}
