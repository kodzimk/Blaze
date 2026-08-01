#include "Localization.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Logging/Log.h"
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

namespace Blaze
{
	LocalizationAsset Localization::s_Asset;
	bool Localization::s_Loaded = false;

	std::string Localization::ResolveLanguagePath(const std::string& languageCode)
	{
		std::string code = languageCode.empty() ? "en" : languageCode;
		for (char& c : code)
			c = (char)std::tolower((unsigned char)c);

		fs::path path;
		if (Project::HasProject() && !Project::GetDirectory().empty())
			path = fs::path(Project::GetDirectory()) / "content" / "localization" / (code + ".loc");
		else
			path = fs::path("content") / "localization" / (code + ".loc");
		return path.string();
	}

	std::string Localization::ApplyFormat(const std::string& text, const std::vector<std::string>& args)
	{
		std::string out = text;
		for (size_t i = 0; i < args.size(); ++i)
		{
			const std::string token = "{" + std::to_string(i) + "}";
			size_t pos = 0;
			while ((pos = out.find(token, pos)) != std::string::npos)
			{
				out.replace(pos, token.size(), args[i]);
				pos += args[i].size();
			}
		}
		return out;
	}

	bool Localization::Load(const std::string& path)
	{
		LocalizationAsset asset;
		if (!asset.LoadFromFile(path))
			return false;
		s_Asset = std::move(asset);
		s_Loaded = true;
		BZ_INFO("Loaded localization ({0}): {1}", s_Asset.GetLanguage(), path);
		return true;
	}

	bool Localization::LoadLanguage(const std::string& languageCode)
	{
		const std::string path = ResolveLanguagePath(languageCode);
		std::error_code ec;
		if (!fs::exists(path, ec))
		{
			BZ_CORE_ERROR("Localization file missing: {0}", path);
			return false;
		}
		return Load(path);
	}

	bool Localization::LoadDefault()
	{
		const std::string path = ResolveLanguagePath("en");
		std::error_code ec;
		if (!fs::exists(path, ec))
		{
			fs::create_directories(fs::path(path).parent_path(), ec);
			LocalizationAsset asset = LocalizationAsset::CreateDefault("en");
			asset.SetPath(path);
			if (!asset.SaveToFile(path))
			{
				s_Asset = std::move(asset);
				s_Loaded = true;
				BZ_INFO("Using in-memory default localization (save failed)");
				return true;
			}
			BZ_INFO("Created default localization: {0}", path);
		}
		return Load(path);
	}

	bool Localization::EnsureLoaded()
	{
		if (s_Loaded)
			return true;
		return LoadDefault();
	}

	void Localization::Unload()
	{
		s_Asset = LocalizationAsset{};
		s_Loaded = false;
	}

	bool Localization::SetLanguage(const std::string& languageCode)
	{
		return LoadLanguage(languageCode);
	}

	const std::string& Localization::GetLanguage()
	{
		EnsureLoaded();
		return s_Asset.GetLanguage();
	}

	const std::string& Localization::GetLoadedPath()
	{
		return s_Asset.GetPath();
	}

	bool Localization::IsLoaded()
	{
		return s_Loaded;
	}

	bool Localization::Has(const std::string& key)
	{
		EnsureLoaded();
		return s_Asset.HasEntry(key);
	}

	std::string Localization::Get(const std::string& key)
	{
		EnsureLoaded();
		return s_Asset.GetEntry(key);
	}

	std::string Localization::Get(const std::string& key, const std::string& fallback)
	{
		EnsureLoaded();
		return s_Asset.GetEntry(key, fallback);
	}

	std::string Localization::Format(const std::string& key, const std::vector<std::string>& args)
	{
		return ApplyFormat(Get(key), args);
	}

	std::string Localization::Format(const std::string& key, const std::string& fallback,
		const std::vector<std::string>& args)
	{
		return ApplyFormat(Get(key, fallback), args);
	}

	const LocalizationAsset& Localization::GetAsset()
	{
		return s_Asset;
	}
}
