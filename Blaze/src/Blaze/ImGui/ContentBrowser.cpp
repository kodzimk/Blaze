#include "ContentBrowser.h"

#include "../vendor/imgui/src/imgui.h"   
#include "Blaze/Renderer/Texture.h"
#include "Blaze/Renderer/Gizmo.h"
#include "Blaze/Logging/Log.h"
#include "Blaze/Core/Project.h"
#include "Blaze/ImGui/ScriptEditorWindow.h"
#include "Blaze/ImGui/WidgetEditorWindow.h"
#include "Blaze/UI/WidgetAsset.h"
#include "Blaze/Renderer/FlipbookAsset.h"
#include "Blaze/Renderer/AnimStateMachineAsset.h"
#include "Blaze/Input/InputActionsAsset.h"
#include "Blaze/Input/InputActions.h"
#include "Blaze/Localization/LocalizationAsset.h"
#include "Blaze/Localization/Localization.h"
#include "Blaze/ImGui/LocalizationEditorWindow.h"
#include "Blaze/Renderer/ParticleEmitterAsset.h"
#include "Blaze/ImGui/ParticleEditorWindow.h"
#include "Blaze/Renderer/PrefabAsset.h"
#include "Blaze/Renderer/ComponentPresetAsset.h"
#include "Blaze/Logging/Log.h"
#include "Blaze/Scripting/SaveObject.h"
#include "Blaze/Assets/TiledImporter.h"
#include "Blaze/ImGui/SceneWindow.h"
#include <filesystem>
#include <cctype>
#include <cstring>
#include <fstream>

#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

namespace fs = std::filesystem;

namespace Blaze
{
	float ContentBrowser::m_height = 250.f;
	float ContentBrowser::m_width = 1000.f;
	std::string ContentBrowser::s_DraggingPath;
	std::string ContentBrowser::s_PendingDropPath;
	std::string ContentBrowser::s_PendingOpenMap;
	std::string ContentBrowser::s_PendingApplyPreset;

	static const char* s_TexturePayload = "BLAZE_TEXTURE";
	static const char* s_AudioPayload = "BLAZE_AUDIO";
	static const char* s_FontPayload = "BLAZE_FONT";
	static const char* s_ScriptPayload = "BLAZE_SCRIPT";
	static const char* s_MapPayload = "BLAZE_MAP";
	static const char* s_FlipbookPayload = "BLAZE_FLIPBOOK";
	static const char* s_AnimPayload = "BLAZE_ANIM";
	static const char* s_InputActionsPayload = "BLAZE_INPUTACTIONS";
	static const char* s_PrefabPayload = "BLAZE_PREFAB";
	static const char* s_CompPresetPayload = "BLAZE_COMPRESET";

	ContentBrowser::ContentBrowser()
		: onFocus(false), m_RootDir("content"), m_CurrentDir("content")
	{
		
	}

	ContentBrowser::~ContentBrowser()
	{
		for (auto& entry : m_Thumbnails)
			delete entry.second;
	}

	void ContentBrowser::SetRootDirectory(const std::string& path)
	{
		m_RootDir = path;
		m_CurrentDir = path;
	}

	bool ContentBrowser::IsTextureFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".jpg" || extension == ".jpeg" ||
			   extension == ".png" || extension == ".bmp";
	}

	bool ContentBrowser::IsAudioFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".wav" || extension == ".mp3" ||
			   extension == ".ogg" || extension == ".flac";
	}

	bool ContentBrowser::IsFontFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".ttf" || extension == ".otf";
	}

	bool ContentBrowser::IsScriptFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".lua";
	}

	bool ContentBrowser::IsSaveScriptFile(const std::string& path) const
	{
		std::ifstream in(path);
		if (!in.is_open())
			return false;

		std::string line;
		int linesChecked = 0;
		while (linesChecked < 24 && std::getline(in, line))
		{
			++linesChecked;
			if (line.find("Blaze Save Script") != std::string::npos ||
				line.find("Save Script (SaveObject)") != std::string::npos)
				return true;
			if (line.find("SaveObject") != std::string::npos &&
				line.find("=") != std::string::npos &&
				line.find("function") == std::string::npos)
				return true;
		}
		return false;
	}

	bool ContentBrowser::IsMapFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".map";
	}

	bool ContentBrowser::IsWidgetFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".widget";
	}

	bool ContentBrowser::IsFlipbookFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".flipbook";
	}

	bool ContentBrowser::IsAnimAssetFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".animasset";
	}

	bool ContentBrowser::IsInputActionsFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".inputactions";
	}

	bool ContentBrowser::IsLocalizationFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".loc";
	}

	bool ContentBrowser::IsParticleFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".particle";
	}

	bool ContentBrowser::IsPrefabFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".prefab";
	}

	bool ContentBrowser::IsComponentPresetFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".compreset";
	}

	Texture* ContentBrowser::GetThumbnail(const std::string& path)
	{
		auto it = m_Thumbnails.find(path);
		if (it != m_Thumbnails.end())
			return it->second;

		Texture* texture = new Texture();
		texture->Generate(path);
		m_Thumbnails[path] = texture;
		return texture;
	}

	void ContentBrowser::ImportTexture()
	{
		char filePath[MAX_PATH] = {};

		OPENFILENAMEA ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = filePath;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter =
			"Texture Files (*.jpg;*.jpeg;*.png;*.bmp)\0*.jpg;*.jpeg;*.png;*.bmp\0"
			"JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0"
			"PNG (*.png)\0*.png\0"
			"BMP (*.bmp)\0*.bmp\0"
			"All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (!GetOpenFileNameA(&ofn))
			return;

		fs::path source(filePath);
		if (!IsTextureFile(source.extension().string()))
		{
			BZ_INFO("Import cancelled: unsupported format '{0}'", source.extension().string());
			return;
		}

		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path destination = fs::path(m_CurrentDir) / source.filename();
		if (fs::exists(destination))
		{
			fs::path stem = source.stem();
			fs::path ext = source.extension();
			int suffix = 1;
			do
			{
				destination = fs::path(m_CurrentDir) / (stem.string() + "_" + std::to_string(suffix) + ext.string());
				++suffix;
			} while (fs::exists(destination));
		}

		std::error_code ec;
		fs::copy_file(source, destination, fs::copy_options::none, ec);
		if (ec)
		{
			BZ_INFO("Failed to import texture: {0}", ec.message());
			return;
		}

		BZ_INFO("Imported texture: {0}", destination.string());
	}

	void ContentBrowser::ImportAudio()
	{
		char filePath[MAX_PATH] = {};

		OPENFILENAMEA ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = filePath;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter =
			"Audio Files (*.wav;*.mp3;*.ogg;*.flac)\0*.wav;*.mp3;*.ogg;*.flac\0"
			"WAV (*.wav)\0*.wav\0"
			"MP3 (*.mp3)\0*.mp3\0"
			"OGG (*.ogg)\0*.ogg\0"
			"FLAC (*.flac)\0*.flac\0"
			"All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (!GetOpenFileNameA(&ofn))
			return;

		fs::path source(filePath);
		if (!IsAudioFile(source.extension().string()))
		{
			BZ_INFO("Import cancelled: unsupported audio format '{0}'", source.extension().string());
			return;
		}

		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path destination = fs::path(m_CurrentDir) / source.filename();
		if (fs::exists(destination))
		{
			fs::path stem = source.stem();
			fs::path ext = source.extension();
			int suffix = 1;
			do
			{
				destination = fs::path(m_CurrentDir) / (stem.string() + "_" + std::to_string(suffix) + ext.string());
				++suffix;
			} while (fs::exists(destination));
		}

		std::error_code ec;
		fs::copy_file(source, destination, fs::copy_options::none, ec);
		if (ec)
		{
			BZ_INFO("Failed to import audio: {0}", ec.message());
			return;
		}

		BZ_INFO("Imported audio: {0}", destination.string());
	}

	void ContentBrowser::ImportFont()
	{
		char filePath[MAX_PATH] = {};

		OPENFILENAMEA ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = filePath;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter =
			"Font Files (*.ttf;*.otf)\0*.ttf;*.otf\0"
			"TrueType (*.ttf)\0*.ttf\0"
			"OpenType (*.otf)\0*.otf\0"
			"All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (!GetOpenFileNameA(&ofn))
			return;

		fs::path source(filePath);
		if (!IsFontFile(source.extension().string()))
		{
			BZ_INFO("Import cancelled: unsupported font format '{0}'", source.extension().string());
			return;
		}

		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path destination = fs::path(m_CurrentDir) / source.filename();
		if (fs::exists(destination))
		{
			fs::path stem = source.stem();
			fs::path ext = source.extension();
			int suffix = 1;
			do
			{
				destination = fs::path(m_CurrentDir) / (stem.string() + "_" + std::to_string(suffix) + ext.string());
				++suffix;
			} while (fs::exists(destination));
		}

		std::error_code ec;
		fs::copy_file(source, destination, fs::copy_options::none, ec);
		if (ec)
		{
			BZ_INFO("Failed to import font: {0}", ec.message());
			return;
		}

		BZ_INFO("Imported font: {0}", destination.string());
	}

	void ContentBrowser::ImportTiledMap()
	{
		char filePath[MAX_PATH] = {};

		OPENFILENAMEA ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = filePath;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter =
			"Tiled Maps (*.tmj;*.json;*.tmx)\0*.tmj;*.json;*.tmx\0"
			"Tiled JSON (*.tmj;*.json)\0*.tmj;*.json\0"
			"Tiled TMX (*.tmx)\0*.tmx\0"
			"All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (!GetOpenFileNameA(&ofn))
			return;

		fs::path source(filePath);
		if (!TiledImporter::IsTiledMapFile(source.string()))
		{
			BZ_INFO("Import cancelled: not a Tiled map '{0}'", source.extension().string());
			return;
		}

		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path mapPath = fs::path(m_CurrentDir) / (source.stem().string() + ".map");
		int suffix = 1;
		while (fs::exists(mapPath))
		{
			mapPath = fs::path(m_CurrentDir) / (source.stem().string() + "_" + std::to_string(suffix) + ".map");
			++suffix;
		}

		fs::path textureDir = fs::path(m_CurrentDir);
		if (Project::HasProject())
		{
			fs::path contentTex = fs::path(Project::GetContentDirectory()) / "texture";
			std::error_code ec;
			fs::create_directories(contentTex, ec);
			if (fs::exists(contentTex))
				textureDir = contentTex;
		}

		std::string error;
		if (!TiledImporter::ImportToMapFile(source.string(), mapPath.string(), textureDir.generic_string(), &error))
		{
			BZ_INFO("Tiled import failed: {0}", error);
			return;
		}

		const std::string scriptRel = Project::CreateMapScriptForMap(mapPath.string());
		if (!scriptRel.empty())
		{
			std::ifstream in(mapPath);
			std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			in.close();
			if (content.find("mapscript ") == std::string::npos)
			{
				std::ofstream out(mapPath, std::ios::trunc);
				if (out.is_open())
				{
					const std::string header = "BlazeMap 1\n";
					if (content.rfind(header, 0) == 0)
						out << header << "mapscript " << scriptRel << "\n" << content.substr(header.size());
					else
						out << "BlazeMap 1\nmapscript " << scriptRel << "\n" << content;
				}
			}
		}

		BZ_INFO("Imported Tiled map: {0}", mapPath.string());
		s_PendingOpenMap = mapPath.string();
	}

	void ContentBrowser::CreateScript()
	{
		m_OpenScriptTypePopup = true;
	}

	void ContentBrowser::CreateScriptOfKind(ScriptKind kind)
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		const char* base = (kind == ScriptKind::SaveObject) ? "NewSave" : "NewScript";
		fs::path scriptPath = fs::path(m_CurrentDir) / (std::string(base) + ".lua");
		int suffix = 1;
		while (fs::exists(scriptPath))
		{
			scriptPath = fs::path(m_CurrentDir) / (std::string(base) + " (" + std::to_string(suffix) + ").lua");
			++suffix;
		}

		if (!ScriptTemplates::WriteScriptFile(scriptPath.string(), kind))
		{
			BZ_INFO("Failed to create script: {0}", scriptPath.string());
			return;
		}

		BZ_INFO("Created {0}: {1}", ScriptTemplates::KindDisplayName(kind), scriptPath.string());
		StartRename(scriptPath.string());
	}

	void ContentBrowser::CreateWidget()
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path widgetPath = fs::path(m_CurrentDir) / "NewWidget.widget";
		int suffix = 1;
		while (fs::exists(widgetPath))
		{
			widgetPath = fs::path(m_CurrentDir) / ("NewWidget (" + std::to_string(suffix) + ").widget");
			++suffix;
		}

		WidgetAsset asset = WidgetAsset::CreateDefault(widgetPath.stem().string());
		asset.SetPath(widgetPath.string());
		if (!asset.SaveToFile(widgetPath.string()))
		{
			BZ_INFO("Failed to create widget: {0}", widgetPath.string());
			return;
		}

		BZ_INFO("Created widget: {0}", widgetPath.string());
		StartRename(widgetPath.string());
	}

	void ContentBrowser::CreateFlipbook()
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path flipPath = fs::path(m_CurrentDir) / "NewFlipbook.flipbook";
		int suffix = 1;
		while (fs::exists(flipPath))
		{
			flipPath = fs::path(m_CurrentDir) / ("NewFlipbook (" + std::to_string(suffix) + ").flipbook");
			++suffix;
		}

		FlipbookAsset asset = FlipbookAsset::CreateDefault(flipPath.stem().string());
		asset.SetPath(flipPath.string());
		if (!asset.SaveToFile(flipPath.string()))
		{
			BZ_INFO("Failed to create flipbook: {0}", flipPath.string());
			return;
		}

		BZ_INFO("Created flipbook asset: {0}", flipPath.string());
		StartRename(flipPath.string());
	}

	void ContentBrowser::CreateAnimAsset()
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path animPath = fs::path(m_CurrentDir) / "NewAnim.animasset";
		int suffix = 1;
		while (fs::exists(animPath))
		{
			animPath = fs::path(m_CurrentDir) / ("NewAnim (" + std::to_string(suffix) + ").animasset");
			++suffix;
		}

		AnimStateMachineAsset asset = AnimStateMachineAsset::CreateDefault(animPath.stem().string());
		asset.SetPath(animPath.string());
		if (!asset.SaveToFile(animPath.string()))
		{
			BZ_INFO("Failed to create anim asset: {0}", animPath.string());
			return;
		}

		BZ_INFO("Created anim asset: {0}", animPath.string());
		StartRename(animPath.string());
	}

	void ContentBrowser::CreateInputActions()
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path path = fs::path(m_CurrentDir) / "Default.inputactions";
		int suffix = 1;
		while (fs::exists(path))
		{
			path = fs::path(m_CurrentDir) / ("InputActions (" + std::to_string(suffix) + ").inputactions");
			++suffix;
		}

		InputActionsAsset asset = InputActionsAsset::CreateDefault(path.stem().string());
		asset.SetPath(path.string());
		if (!asset.SaveToFile(path.string()))
		{
			BZ_INFO("Failed to create input actions: {0}", path.string());
			return;
		}

		BZ_INFO("Created input actions: {0}", path.string());
		StartRename(path.string());
	}

	void ContentBrowser::CreateLocalization()
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path path = fs::path(m_CurrentDir) / "en.loc";
		int suffix = 1;
		while (fs::exists(path))
		{
			path = fs::path(m_CurrentDir) / ("lang (" + std::to_string(suffix) + ").loc");
			++suffix;
		}

		std::string lang = path.stem().string();
		if (lang.empty())
			lang = "en";
		LocalizationAsset asset = LocalizationAsset::CreateDefault(lang);
		asset.SetPath(path.string());
		if (!asset.SaveToFile(path.string()))
		{
			BZ_INFO("Failed to create localization: {0}", path.string());
			return;
		}

		BZ_INFO("Created localization: {0}", path.string());
		StartRename(path.string());
	}

	void ContentBrowser::CreateParticle()
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path path = fs::path(m_CurrentDir) / "NewParticle.particle";
		int suffix = 1;
		while (fs::exists(path))
		{
			path = fs::path(m_CurrentDir) / ("NewParticle (" + std::to_string(suffix) + ").particle");
			++suffix;
		}

		ParticleEmitterAsset asset = ParticleEmitterAsset::CreateDefault(path.stem().string());
		asset.SetPath(path.string());
		if (!asset.SaveToFile(path.string()))
		{
			BZ_INFO("Failed to create particle: {0}", path.string());
			return;
		}

		BZ_INFO("Created particle asset: {0}", path.string());
		StartRename(path.string());
	}

	void ContentBrowser::CreatePrefab()
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path path = fs::path(m_CurrentDir) / "NewPrefab.prefab";
		int suffix = 1;
		while (fs::exists(path))
		{
			path = fs::path(m_CurrentDir) / ("NewPrefab (" + std::to_string(suffix) + ").prefab");
			++suffix;
		}

		PrefabAsset asset = PrefabAsset::CreateDefault(path.stem().string());
		asset.SetPath(path.string());
		if (!asset.SaveToFile(path.string()))
		{
			BZ_INFO("Failed to create prefab: {0}", path.string());
			return;
		}

		BZ_INFO("Created prefab: {0}", path.string());
		StartRename(path.string());
	}

	void ContentBrowser::CreateComponentPreset()
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path path = fs::path(m_CurrentDir) / "NewPreset.compreset";
		int suffix = 1;
		while (fs::exists(path))
		{
			path = fs::path(m_CurrentDir) / ("NewPreset (" + std::to_string(suffix) + ").compreset");
			++suffix;
		}

		ComponentPresetAsset asset = ComponentPresetAsset::CreateDefault(path.stem().string());
		asset.SetPath(path.string());
		if (!asset.SaveToFile(path.string()))
		{
			BZ_INFO("Failed to create component preset: {0}", path.string());
			return;
		}

		BZ_INFO("Created component preset: {0}", path.string());
		StartRename(path.string());
	}

	void ContentBrowser::CreateMap()
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path mapPath = fs::path(m_CurrentDir) / "NewMap.map";
		int suffix = 1;
		while (fs::exists(mapPath))
		{
			mapPath = fs::path(m_CurrentDir) / ("NewMap (" + std::to_string(suffix) + ").map");
			++suffix;
		}

		const std::string scriptRel = Project::CreateMapScriptForMap(mapPath.string());

		std::ofstream out(mapPath);
		if (!out.is_open())
		{
			BZ_INFO("Failed to create map: {0}", mapPath.string());
			return;
		}

		out << "BlazeMap 1\n";
		if (!scriptRel.empty())
			out << "mapscript " << scriptRel << "\n";
		out.close();

		BZ_INFO("Created map: {0}", mapPath.string());
		StartRename(mapPath.string());
	}

	void ContentBrowser::CreateFolder()
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path folderPath = fs::path(m_CurrentDir) / "New Folder";
		int suffix = 1;
		while (fs::exists(folderPath))
		{
			folderPath = fs::path(m_CurrentDir) / ("New Folder (" + std::to_string(suffix) + ")");
			++suffix;
		}

		std::error_code ec;
		if (!fs::create_directory(folderPath, ec))
		{
			BZ_INFO("Failed to create folder: {0}", ec.message());
			return;
		}

		BZ_INFO("Created folder: {0}", folderPath.string());
		StartRename(folderPath.string());
	}

	void ContentBrowser::StartRename(const std::string& path)
	{
		m_RenamingPath = path;
		std::string name = fs::path(path).filename().string();
		std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
		std::strncpy(m_RenameBuffer, name.c_str(), sizeof(m_RenameBuffer) - 1);
		m_FocusRename = true;
	}

	void ContentBrowser::CommitRename()
	{
		if (m_RenamingPath.empty())
			return;

		std::string newName = m_RenameBuffer;
		while (!newName.empty() && (newName.front() == ' ' || newName.front() == '\t'))
			newName.erase(newName.begin());
		while (!newName.empty() && (newName.back() == ' ' || newName.back() == '\t'))
			newName.pop_back();

		const std::string oldPath = m_RenamingPath;
		m_RenamingPath.clear();
		m_FocusRename = false;

		if (newName.empty() || newName.find_first_of("\\/:*?\"<>|") != std::string::npos)
		{
			BZ_INFO("Rename cancelled: invalid name");
			return;
		}

		fs::path old = oldPath;
		// Keep extension for files if the user only typed a stem
		if (old.has_extension() && fs::is_regular_file(old) && fs::path(newName).extension().empty())
			newName += old.extension().string();

		fs::path destination = old.parent_path() / newName;
		if (old.filename().string() == newName)
			return;

		if (fs::exists(destination))
		{
			BZ_INFO("Rename cancelled: '{0}' already exists", newName);
			return;
		}

		std::error_code ec;
		fs::rename(old, destination, ec);
		if (ec)
		{
			BZ_INFO("Failed to rename: {0}", ec.message());
			return;
		}

		InvalidateThumbnail(oldPath);
		BZ_INFO("Renamed to: {0}", destination.string());
	}

	void ContentBrowser::DrawAssetRenameLabel(const std::string& fullPath, const std::string& name, float width)
	{
		if (m_RenamingPath == fullPath)
		{
			if (m_FocusRename)
			{
				ImGui::SetKeyboardFocusHere();
				m_FocusRename = false;
			}

			ImGui::SetNextItemWidth(width);
			bool renameDone = ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			if (renameDone || ImGui::IsItemDeactivatedAfterEdit())
				CommitRename();
		}
		else
		{
			ImGui::TextWrapped("%s", name.c_str());
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				StartRename(fullPath);
		}
	}

	void ContentBrowser::DrawContextMenu()
	{
		if (ImGui::BeginPopupContextWindow("ContentBrowserContext",
			ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Import Texture"))
				ImportTexture();
			if (ImGui::MenuItem("Import Audio"))
				ImportAudio();
			if (ImGui::MenuItem("Import Font"))
				ImportFont();
			if (ImGui::MenuItem("Import Tiled Map"))
				ImportTiledMap();
			if (ImGui::MenuItem("Create Script"))
				CreateScript();
			if (ImGui::MenuItem("Create Flipbook"))
				CreateFlipbook();
			if (ImGui::MenuItem("Create Anim Graph"))
				CreateAnimAsset();
			if (ImGui::MenuItem("Create Input Actions"))
				CreateInputActions();
			if (ImGui::MenuItem("Create Localization"))
				CreateLocalization();
			if (ImGui::MenuItem("Create Particle"))
				CreateParticle();
			if (ImGui::MenuItem("Create Prefab"))
				CreatePrefab();
			if (ImGui::MenuItem("Create Component Preset"))
				CreateComponentPreset();
			if (ImGui::MenuItem("Create Widget"))
				CreateWidget();
			if (ImGui::MenuItem("Create Map"))
				CreateMap();
			if (ImGui::MenuItem("New Folder"))
				CreateFolder();
			ImGui::EndPopup();
		}
	}

	void ContentBrowser::DrawCreateScriptPopup()
	{
		if (m_OpenScriptTypePopup)
		{
			ImGui::OpenPopup("Create Script Type");
			m_OpenScriptTypePopup = false;
		}

		if (ImGui::BeginPopupModal("Create Script Type", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Choose script type");
			ImGui::Separator();
			ImGui::TextWrapped("Component Script — attach to a GameObject (OnStart / OnUpdate).");
			ImGui::TextWrapped("Save Script — SaveObject data class (not a GameObject).");
			ImGui::Spacing();

			if (ImGui::Button("Component Script", ImVec2(180.0f, 0.0f)))
			{
				CreateScriptOfKind(ScriptKind::Component);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Save Script", ImVec2(180.0f, 0.0f)))
			{
				CreateScriptOfKind(ScriptKind::SaveObject);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::Button("Cancel", ImVec2(370.0f, 0.0f)))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}

	void ContentBrowser::InvalidateThumbnail(const std::string& path)
	{
		auto it = m_Thumbnails.find(path);
		if (it == m_Thumbnails.end())
			return;

		delete it->second;
		m_Thumbnails.erase(it);
	}

	bool ContentBrowser::MoveAssetToFolder(const std::string& assetPath, const std::string& folderPath)
	{
		fs::path source(assetPath);
		if (!fs::exists(source) || !fs::is_directory(folderPath))
			return false;

		std::error_code ec;
		if (fs::equivalent(source.parent_path(), folderPath, ec))
			return false;

		fs::path destination = fs::path(folderPath) / source.filename();
		if (fs::exists(destination))
		{
			fs::path stem = source.stem();
			fs::path ext = source.extension();
			int suffix = 1;
			do
			{
				destination = fs::path(folderPath) / (stem.string() + "_" + std::to_string(suffix) + ext.string());
				++suffix;
			} while (fs::exists(destination));
		}

		fs::rename(source, destination, ec);
		if (ec)
		{
			BZ_INFO("Failed to move asset: {0}", ec.message());
			return false;
		}

		InvalidateThumbnail(assetPath);
		BZ_INFO("Moved asset to: {0}", destination.string());
		return true;
	}

	bool ContentBrowser::AcceptAssetDrop(const std::string& folderPath)
	{
		bool moved = false;
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_TexturePayload))
		{
			std::string sourcePath((const char*)payload->Data);
			if (MoveAssetToFolder(sourcePath, folderPath))
				moved = true;
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_AudioPayload))
		{
			std::string sourcePath((const char*)payload->Data);
			if (MoveAssetToFolder(sourcePath, folderPath))
				moved = true;
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_FontPayload))
		{
			std::string sourcePath((const char*)payload->Data);
			if (MoveAssetToFolder(sourcePath, folderPath))
				moved = true;
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_ScriptPayload))
		{
			std::string sourcePath((const char*)payload->Data);
			if (MoveAssetToFolder(sourcePath, folderPath))
				moved = true;
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_MapPayload))
		{
			std::string sourcePath((const char*)payload->Data);
			if (MoveAssetToFolder(sourcePath, folderPath))
				moved = true;
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_FlipbookPayload))
		{
			std::string sourcePath((const char*)payload->Data);
			if (MoveAssetToFolder(sourcePath, folderPath))
				moved = true;
		}
		return moved;
	}

	void ContentBrowser::OnUpdate()
	{
		if (!m_Open)
			return;

		if (!ImGui::Begin("Content Browser", &m_Open))
		{
			ImGui::End();
			return;
		}
		onFocus = ImGui::IsWindowFocused();

		if (!fs::exists(m_RootDir))
		{
			ImGui::TextWrapped("Asset folder '%s' not found. Run the app from the folder that contains it.", m_RootDir.c_str());
			DrawContextMenu();
			DrawCreateScriptPopup();
			ImGui::End();
			return;
		}

		if (m_CurrentDir != m_RootDir)
		{
			if (ImGui::Button("<-"))
			{
				if (!m_RenamingPath.empty())
					CommitRename();
				m_CurrentDir = fs::path(m_CurrentDir).parent_path().string();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (AcceptAssetDrop(fs::path(m_CurrentDir).parent_path().string()))
					s_DraggingPath.clear();
				ImGui::EndDragDropTarget();
			}

			ImGui::SameLine();
		}

		ImGui::TextUnformatted(m_CurrentDir.c_str());
		ImGui::Separator();

		const float thumbSize = 84.0f;
		const float cellSize = thumbSize + 24.0f;
		int columns = (int)(ImGui::GetContentRegionAvail().x / cellSize);
		if (columns < 1) columns = 1;
		ImGui::Columns(columns, nullptr, false);

		bool changedDir = false;
		bool movedAsset = false;
		for (auto& entry : fs::directory_iterator(m_CurrentDir))
		{
			const fs::path& path = entry.path();
			std::string name = path.filename().string();
			std::string fullPath = path.generic_string();

			ImGui::PushID(fullPath.c_str());

			if (entry.is_directory())
			{
				unsigned int folderTex = Gizmo2D::GetFolderIconTextureID();
				bool clicked = false;
				if (folderTex != 0)
				{
					clicked = ImGui::ImageButton("##folder", (ImTextureID)(intptr_t)folderTex,
						ImVec2(thumbSize, thumbSize), ImVec2(0, 0), ImVec2(1, 1),
						ImVec4(0.14f, 0.14f, 0.14f, 1.0f), ImVec4(1, 1, 1, 1));
				}
				else
				{
					clicked = ImGui::Button("[DIR]", ImVec2(thumbSize, thumbSize));
				}

				if (clicked)
				{
					if (!m_RenamingPath.empty())
						CommitRename();
					m_CurrentDir = fullPath;
					changedDir = true;
				}

				if (ImGui::BeginDragDropTarget())
				{
					if (AcceptAssetDrop(fullPath))
					{
						s_DraggingPath.clear();
						movedAsset = true;
					}
					ImGui::EndDragDropTarget();
				}

				if (ImGui::BeginPopupContextItem("FolderItemContext"))
				{
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsTextureFile(path.extension().string()))
			{
				Texture* thumbnail = GetThumbnail(fullPath);
				ImGui::ImageButton("##thumb", (ImTextureID)(intptr_t)thumbnail->GetTextID(), ImVec2(thumbSize, thumbSize));

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(s_TexturePayload, fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					ImGui::Image((ImTextureID)(intptr_t)thumbnail->GetTextID(), ImVec2(48.0f, 48.0f));
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginPopupContextItem("TextureItemContext"))
				{
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsAudioFile(path.extension().string()))
			{
				unsigned int soundTex = Gizmo2D::GetSoundIconTextureID();
				if (soundTex != 0)
				{
					ImGui::ImageButton("##audio", (ImTextureID)(intptr_t)soundTex,
						ImVec2(thumbSize, thumbSize), ImVec2(0, 0), ImVec2(1, 1),
						ImVec4(0.14f, 0.14f, 0.14f, 1.0f), ImVec4(1, 1, 1, 1));
				}
				else
				{
					ImGui::Button("[AUD]", ImVec2(thumbSize, thumbSize));
				}

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(s_AudioPayload, fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					if (soundTex != 0)
						ImGui::Image((ImTextureID)(intptr_t)soundTex, ImVec2(48.0f, 48.0f));
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginPopupContextItem("AudioItemContext"))
				{
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsFontFile(path.extension().string()))
			{
				ImGui::Button("[FONT]", ImVec2(thumbSize, thumbSize));

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(s_FontPayload, fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginPopupContextItem("FontItemContext"))
				{
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsScriptFile(path.extension().string()))
			{
				const bool isSaveScript = IsSaveScriptFile(fullPath);
				unsigned int scriptTex = isSaveScript
					? Gizmo2D::GetSaveScriptIconTextureID()
					: Gizmo2D::GetScriptIconTextureID();
				if (scriptTex != 0)
				{
					ImGui::PushID(isSaveScript ? "save_script" : "comp_script");
					ImGui::ImageButton("##script", (ImTextureID)(intptr_t)scriptTex,
						ImVec2(thumbSize, thumbSize), ImVec2(0, 0), ImVec2(1, 1),
						ImVec4(0.14f, 0.14f, 0.14f, 1.0f), ImVec4(1, 1, 1, 1));
					ImGui::PopID();
				}
				else
				{
					ImGui::Button(isSaveScript ? "[SAV]" : "[LUA]", ImVec2(thumbSize, thumbSize));
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", isSaveScript ? "Save Script (SaveObject)" : "Component Script");
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						ScriptEditorWindow::OpenFile(fullPath);
				}

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(s_ScriptPayload, fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					if (scriptTex != 0)
						ImGui::Image((ImTextureID)(intptr_t)scriptTex, ImVec2(48.0f, 48.0f));
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginPopupContextItem("ScriptItemContext"))
				{
					if (ImGui::MenuItem("Edit Script"))
						ScriptEditorWindow::OpenFile(fullPath);
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsWidgetFile(path.extension().string()))
			{
				if (ImGui::Button("[UI]", ImVec2(thumbSize, thumbSize)))
				{
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					WidgetEditorWindow::OpenFile(fullPath);

				if (ImGui::BeginPopupContextItem("WidgetItemContext"))
				{
					if (ImGui::MenuItem("Edit Widget"))
						WidgetEditorWindow::OpenFile(fullPath);
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsFlipbookFile(path.extension().string()))
			{
				ImGui::Button("[FB]", ImVec2(thumbSize, thumbSize));

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(s_FlipbookPayload, fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginPopupContextItem("FlipbookItemContext"))
				{
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsAnimAssetFile(path.extension().string()))
			{
				ImGui::Button("[AN]", ImVec2(thumbSize, thumbSize));

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(s_AnimPayload, fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginPopupContextItem("AnimItemContext"))
				{
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsInputActionsFile(path.extension().string()))
			{
				ImGui::Button("[IA]", ImVec2(thumbSize, thumbSize));

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(s_InputActionsPayload, fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (InputActions::Load(fullPath))
						BZ_INFO("Loaded input actions: {0}", fullPath);
				}

				if (ImGui::BeginPopupContextItem("InputActionsItemContext"))
				{
					if (ImGui::MenuItem("Load"))
						InputActions::Load(fullPath);
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsLocalizationFile(path.extension().string()))
			{
				ImGui::Button("[LC]", ImVec2(thumbSize, thumbSize));

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload("BLAZE_LOC_ASSET", fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					LocalizationEditorWindow::OpenFile(fullPath);

				if (ImGui::BeginPopupContextItem("LocalizationItemContext"))
				{
					if (ImGui::MenuItem("Edit"))
						LocalizationEditorWindow::OpenFile(fullPath);
					if (ImGui::MenuItem("Load Language"))
						Localization::Load(fullPath);
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsParticleFile(path.extension().string()))
			{
				ImGui::Button("[FX]", ImVec2(thumbSize, thumbSize));

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload("BLAZE_PARTICLE", fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					ParticleEditorWindow::OpenFile(fullPath);

				if (ImGui::BeginPopupContextItem("ParticleItemContext"))
				{
					if (ImGui::MenuItem("Edit"))
						ParticleEditorWindow::OpenFile(fullPath);
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsPrefabFile(path.extension().string()))
			{
				ImGui::Button("[PF]", ImVec2(thumbSize, thumbSize));

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(s_PrefabPayload, fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginPopupContextItem("PrefabItemContext"))
				{
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsComponentPresetFile(path.extension().string()))
			{
				ImGui::Button("[CP]", ImVec2(thumbSize, thumbSize));

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(s_CompPresetPayload, fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginPopupContextItem("CompPresetItemContext"))
				{
					if (ImGui::MenuItem("Apply to Selection"))
						s_PendingApplyPreset = fullPath;
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}
			else if (IsMapFile(path.extension().string()))
			{
				unsigned int mapTex = Gizmo2D::GetMapIconTextureID();
				if (mapTex != 0)
				{
					ImGui::ImageButton("##map", (ImTextureID)(intptr_t)mapTex,
						ImVec2(thumbSize, thumbSize), ImVec2(0, 0), ImVec2(1, 1),
						ImVec4(0.0f, 0.0f, 0.0f, 1.0f), ImVec4(1, 1, 1, 1));
				}
				else
				{
					ImGui::Button("[MAP]", ImVec2(thumbSize, thumbSize));
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					s_PendingOpenMap = fullPath;
					BZ_INFO("Opening map: {0}", fullPath);
				}

				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(s_MapPayload, fullPath.c_str(), fullPath.size() + 1);
					s_DraggingPath = fullPath;
					if (mapTex != 0)
						ImGui::Image((ImTextureID)(intptr_t)mapTex, ImVec2(48.0f, 48.0f));
					ImGui::TextUnformatted(name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginPopupContextItem("MapItemContext"))
				{
					if (ImGui::MenuItem("Open Map"))
					{
						s_PendingOpenMap = fullPath;
						BZ_INFO("Opening map: {0}", fullPath);
					}
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				DrawAssetRenameLabel(fullPath, name, thumbSize);
			}

			ImGui::PopID();
			ImGui::NextColumn();

			if (changedDir || movedAsset)
				break;
		}

		ImGui::Columns(1);
		DrawContextMenu();
		DrawCreateScriptPopup();
		ImGui::End();

		if (!s_DraggingPath.empty() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			const bool overEmpty = !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
			const bool overScene = SceneWindow::IsMouseOverSceneView();
			if (overEmpty || overScene)
			{
				std::string ext = fs::path(s_DraggingPath).extension().string();
				for (auto& c : ext)
					c = (char)std::tolower((unsigned char)c);
				if (ext != ".compreset" && ext != ".inputactions" && ext != ".animasset" && ext != ".flipbook" && ext != ".loc")
				{
					s_PendingDropPath = s_DraggingPath;
					BZ_INFO("Asset dropped on scene: {0}", s_PendingDropPath);
				}
			}
			s_DraggingPath.clear();
		}
	}

	bool ContentBrowser::ConsumePendingDrop(std::string& outPath)
	{
		if (s_PendingDropPath.empty())
			return false;

		outPath = s_PendingDropPath;
		s_PendingDropPath.clear();
		return true;
	}

	bool ContentBrowser::ConsumePendingOpenMap(std::string& outPath)
	{
		if (s_PendingOpenMap.empty())
			return false;

		outPath = s_PendingOpenMap;
		s_PendingOpenMap.clear();
		return true;
	}

	bool ContentBrowser::ConsumePendingApplyPreset(std::string& outPath)
	{
		if (s_PendingApplyPreset.empty())
			return false;

		outPath = s_PendingApplyPreset;
		s_PendingApplyPreset.clear();
		return true;
	}

	void ContentBrowser::SetSize(float width, float height)
	{
		m_width = width;
		m_height = height;
	}
}
