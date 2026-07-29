#include "ContentBrowser.h"

#include "../vendor/imgui/src/imgui.h"   
#include "Blaze/Renderer/Texture.h"
#include "Blaze/Logging/Log.h"
#include <filesystem>
#include <cctype>
#include <cstring>

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

	static const char* s_TexturePayload = "BLAZE_TEXTURE";

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
			BZ_INFO("Rename cancelled: invalid folder name");
			return;
		}

		fs::path destination = fs::path(oldPath).parent_path() / newName;
		if (fs::path(oldPath).filename().string() == newName)
			return;

		if (fs::exists(destination))
		{
			BZ_INFO("Rename cancelled: '{0}' already exists", newName);
			return;
		}

		std::error_code ec;
		fs::rename(oldPath, destination, ec);
		if (ec)
		{
			BZ_INFO("Failed to rename folder: {0}", ec.message());
			return;
		}

		BZ_INFO("Renamed folder to: {0}", destination.string());
	}

	void ContentBrowser::DrawContextMenu()
	{
		if (ImGui::BeginPopupContextWindow("ContentBrowserContext",
			ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Import"))
				ImportTexture();
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

	bool ContentBrowser::MoveTextureToFolder(const std::string& texturePath, const std::string& folderPath)
	{
		fs::path source(texturePath);
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
			BZ_INFO("Failed to move texture: {0}", ec.message());
			return false;
		}

		InvalidateThumbnail(texturePath);
		BZ_INFO("Moved texture to: {0}", destination.string());
		return true;
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
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_TexturePayload))
				{
					std::string sourcePath((const char*)payload->Data);
					std::string parentPath = fs::path(m_CurrentDir).parent_path().string();
					if (MoveTextureToFolder(sourcePath, parentPath))
						s_DraggingPath.clear();
				}
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
				if (ImGui::Button("[DIR]", ImVec2(thumbSize, thumbSize)))
				{
					if (!m_RenamingPath.empty())
						CommitRename();
					m_CurrentDir = fullPath;
					changedDir = true;
				}

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_TexturePayload))
					{
						std::string sourcePath((const char*)payload->Data);
						if (MoveTextureToFolder(sourcePath, fullPath))
						{
							s_DraggingPath.clear();
							movedAsset = true;
						}
					}
					ImGui::EndDragDropTarget();
				}

				if (ImGui::BeginPopupContextItem("FolderItemContext"))
				{
					if (ImGui::MenuItem("Rename"))
						StartRename(fullPath);
					ImGui::EndPopup();
				}

				if (m_RenamingPath == fullPath)
				{
					if (m_FocusRename)
					{
						ImGui::SetKeyboardFocusHere();
						m_FocusRename = false;
					}

					ImGui::SetNextItemWidth(thumbSize);
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
				ImGui::TextWrapped("%s", name.c_str());
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
				BZ_INFO("Texture dropped on scene: {0}", s_PendingDropPath);
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

	void ContentBrowser::SetSize(float width, float height)
	{
		m_width = width;
		m_height = height;
	}
}
