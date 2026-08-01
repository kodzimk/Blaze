#pragma once
#include "Blaze/Core/Core.h"
#include "Blaze/Localization/LocalizationAsset.h"
#include <string>
#include <vector>

namespace Blaze
{
	// Runtime string table loaded from .loc files (one language per file).
	class BLAZE_API Localization
	{
	public:
		static bool Load(const std::string& path);
		static bool LoadLanguage(const std::string& languageCode); // content/localization/<code>.loc
		static bool LoadDefault(); // en.loc (creates if missing)
		static bool EnsureLoaded();
		static void Unload();

		static bool SetLanguage(const std::string& languageCode); // alias LoadLanguage
		static const std::string& GetLanguage();
		static const std::string& GetLoadedPath();
		static bool IsLoaded();

		static bool Has(const std::string& key);
		static std::string Get(const std::string& key);
		static std::string Get(const std::string& key, const std::string& fallback);
		static std::string Format(const std::string& key, const std::vector<std::string>& args);
		static std::string Format(const std::string& key, const std::string& fallback,
			const std::vector<std::string>& args);

		static const LocalizationAsset& GetAsset();

	private:
		static std::string ApplyFormat(const std::string& text, const std::vector<std::string>& args);
		static std::string ResolveLanguagePath(const std::string& languageCode);

		static LocalizationAsset s_Asset;
		static bool s_Loaded;
	};
}
