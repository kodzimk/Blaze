#include "ComponentsWindow.h"

#include "../vendor/imgui/src/imgui.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Texture.h"
#include "Blaze/Logging/Log.h"
#include <filesystem>
#include <cctype>
#include <vector>

namespace fs = std::filesystem;

namespace Blaze
{
	float ComponentsWindow::width = 300.f;
	float ComponentsWindow::height = 400.f;

	static const char* s_TexturePayload = "BLAZE_TEXTURE";

	ComponentsWindow::ComponentsWindow()
		: onFocus(false), m_Selected(nullptr)
	{
	}

	ComponentsWindow::~ComponentsWindow()
	{
		for (auto& entry : m_Thumbnails)
			delete entry.second;
	}

	static bool IsTextureExtension(std::string extension)
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".jpg" || extension == ".jpeg" ||
			   extension == ".png" || extension == ".bmp";
	}

	static void CollectContentTextures(const fs::path& root, std::vector<std::string>& out)
	{
		if (!fs::exists(root))
			return;

		std::error_code ec;
		for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); ++it)
		{
			if (ec)
				break;
			if (!it->is_regular_file())
				continue;
			if (IsTextureExtension(it->path().extension().string()))
				out.push_back(it->path().string());
		}
	}

	Texture* ComponentsWindow::GetThumbnail(const std::string& path)
	{
		auto it = m_Thumbnails.find(path);
		if (it != m_Thumbnails.end())
			return it->second;

		Texture* texture = new Texture();
		texture->Generate(path);
		m_Thumbnails[path] = texture;
		return texture;
	}

	void ComponentsWindow::OnUpdate()
	{
		if (!m_Open)
			return;

		if (!ImGui::Begin("Components", &m_Open))
		{
			ImGui::End();
			return;
		}
		onFocus = ImGui::IsWindowFocused();

		if (!m_Selected)
		{
			ImGui::TextDisabled("No object selected");
			ImGui::End();
			return;
		}

		ImGui::Text("Name: %s", m_Selected->GetName().c_str());
		if (m_Selected->IsCamera())
			ImGui::TextDisabled("Type: Camera");
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			glm::vec3 position = m_Selected->GetPosition();
			if (ImGui::InputFloat3("Position", &position.x, "%.3f"))
				m_Selected->SetPosition(position);

			float rotation = m_Selected->GetRotation();
			if (ImGui::InputFloat("Rotation", &rotation, 0.0f, 0.0f, "%.3f"))
				m_Selected->SetRotation(rotation);

			glm::vec2 scale = m_Selected->GetScale();
			if (ImGui::InputFloat2("Scale", &scale.x, "%.3f"))
			{
				scale.x = glm::max(scale.x, 0.05f);
				scale.y = glm::max(scale.y, 0.05f);
				m_Selected->SetScale(scale);
			}
		}

		if (m_Selected->IsCamera())
		{
			if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
			{
				float zoom = m_Selected->GetZoom();
				if (ImGui::InputFloat("Zoom", &zoom, 0.0f, 0.0f, "%.3f"))
					m_Selected->SetZoom(zoom);
			}
		}
		else if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Texture");

			const float previewSize = 64.0f;
			ImVec2 cursor = ImGui::GetCursorScreenPos();

			if (m_Selected->HasTexture())
			{
				ImGui::Image((ImTextureID)(intptr_t)m_Selected->GetTextureID(), ImVec2(previewSize, previewSize));
			}
			else
			{
				ImGui::Dummy(ImVec2(previewSize, previewSize));
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				drawList->AddRectFilled(cursor, ImVec2(cursor.x + previewSize, cursor.y + previewSize), IM_COL32(40, 40, 40, 255));
				drawList->AddRect(cursor, ImVec2(cursor.x + previewSize, cursor.y + previewSize), IM_COL32(90, 90, 90, 255));
				drawList->AddText(ImVec2(cursor.x + 10.0f, cursor.y + previewSize * 0.5f - 7.0f), IM_COL32(160, 160, 160, 255), "None");
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_TexturePayload))
				{
					std::string path((const char*)payload->Data);
					m_Selected->SetTexture(path);
					BZ_INFO("Assigned texture: {0}", path);
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::SameLine();
			ImGui::BeginGroup();
			const std::string& path = m_Selected->GetTexturePath();
			if (path.empty())
				ImGui::TextDisabled("No texture");
			else
				ImGui::TextWrapped("%s", path.c_str());

			ImGui::TextDisabled("Drag from Content Browser");
			if (ImGui::Button("Select..."))
				ImGui::OpenPopup("ContentBrowserTexturePicker");

			if (ImGui::BeginPopup("ContentBrowserTexturePicker"))
			{
				ImGui::TextUnformatted("Content Browser textures");
				ImGui::Separator();

				std::vector<std::string> textures;
				CollectContentTextures("content", textures);

				if (textures.empty())
				{
					ImGui::TextDisabled("No textures found in content/");
				}
				else
				{
					const float thumbSize = 64.0f;
					const float cellSize = thumbSize + 16.0f;
					ImGui::BeginChild("TexturePickerList", ImVec2(280.0f, 240.0f), true);

					int columns = (int)(ImGui::GetContentRegionAvail().x / cellSize);
					if (columns < 1) columns = 1;
					ImGui::Columns(columns, nullptr, false);

					for (const std::string& texturePath : textures)
					{
						ImGui::PushID(texturePath.c_str());

						Texture* thumbnail = GetThumbnail(texturePath);
						bool selected = (texturePath == path);
						if (!selected && !path.empty())
						{
							std::error_code ec;
							selected = fs::equivalent(texturePath, path, ec);
						}

						ImVec4 tint = selected ? ImVec4(0.6f, 0.85f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);
						if (ImGui::ImageButton("##pick", (ImTextureID)(intptr_t)thumbnail->GetTextID(),
							ImVec2(thumbSize, thumbSize), ImVec2(0, 0), ImVec2(1, 1),
							ImVec4(0, 0, 0, 0), tint))
						{
							m_Selected->SetTexture(texturePath);
							ImGui::CloseCurrentPopup();
						}

						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("%s", texturePath.c_str());

						ImGui::TextWrapped("%s", fs::path(texturePath).filename().string().c_str());
						ImGui::PopID();
						ImGui::NextColumn();
					}

					ImGui::Columns(1);
					ImGui::EndChild();
				}

				ImGui::EndPopup();
			}
			ImGui::EndGroup();
		}

		ImGui::Spacing();
		if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
			ImGui::OpenPopup("AddComponentPopup");

		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			ImGui::TextDisabled("No additional components yet");
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void ComponentsWindow::SetSize(float w, float h)
	{
		width = w;
		height = h;
	}
}
