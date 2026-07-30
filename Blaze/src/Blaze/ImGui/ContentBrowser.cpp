#include "ContentBrowser.h"

#include "../vendor/imgui/src/imgui.h"   
#include "Blaze/Renderer/Texture.h"
#include "Blaze/Renderer/Gizmo.h"
#include "Blaze/Logging/Log.h"
#include "Blaze/Core/Project.h"
#include "Blaze/ImGui/ScriptEditorWindow.h"
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

	static const char* s_TexturePayload = "BLAZE_TEXTURE";
	static const char* s_AudioPayload = "BLAZE_AUDIO";
	static const char* s_ScriptPayload = "BLAZE_SCRIPT";
	static const char* s_MapPayload = "BLAZE_MAP";

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

	bool ContentBrowser::IsScriptFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".lua";
	}

	bool ContentBrowser::IsMapFile(std::string extension) const
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".map";
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

	void ContentBrowser::CreateScript()
	{
		if (!fs::exists(m_CurrentDir))
			fs::create_directories(m_CurrentDir);

		fs::path scriptPath = fs::path(m_CurrentDir) / "NewScript.lua";
		int suffix = 1;
		while (fs::exists(scriptPath))
		{
			scriptPath = fs::path(m_CurrentDir) / ("NewScript (" + std::to_string(suffix) + ").lua");
			++suffix;
		}

		std::ofstream out(scriptPath);
		if (!out.is_open())
		{
			BZ_INFO("Failed to create script: {0}", scriptPath.string());
			return;
		}

		out << "-- Blaze Lua script\n\n";
		out << "function OnStart()\n";
		out << "\tprint(\"Script started on\", selfName)\n";
		out << "end\n\n";
		out << "function OnUpdate(dt)\n";
		out << "\tif Input.IsKeyDown(\"A\") then\n";
		out << "\t\tprint(\"A held\")\n";
		out << "\tend\n";
		out << "end\n";
		out.close();

		BZ_INFO("Created script: {0}", scriptPath.string());
		StartRename(scriptPath.string());
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
			if (ImGui::MenuItem("Create Script"))
				CreateScript();
			if (ImGui::MenuItem("Create Map"))
				CreateMap();
			if (ImGui::MenuItem("New Folder"))
				CreateFolder();
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
			else if (IsScriptFile(path.extension().string()))
			{
				unsigned int scriptTex = Gizmo2D::GetScriptIconTextureID();
				if (scriptTex != 0)
				{
					ImGui::ImageButton("##script", (ImTextureID)(intptr_t)scriptTex,
						ImVec2(thumbSize, thumbSize), ImVec2(0, 0), ImVec2(1, 1),
						ImVec4(0.14f, 0.14f, 0.14f, 1.0f), ImVec4(1, 1, 1, 1));
				}
				else
				{
					ImGui::Button("[LUA]", ImVec2(thumbSize, thumbSize));
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					ScriptEditorWindow::OpenFile(fullPath);

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
		ImGui::End();

		if (!s_DraggingPath.empty() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
			{
				s_PendingDropPath = s_DraggingPath;
				BZ_INFO("Asset dropped on scene: {0}", s_PendingDropPath);
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

	void ContentBrowser::SetSize(float width, float height)
	{
		m_width = width;
		m_height = height;
	}
}
