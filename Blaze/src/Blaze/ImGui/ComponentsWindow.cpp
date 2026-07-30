#include "ComponentsWindow.h"

#include "../vendor/imgui/src/imgui.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Texture.h"
#include "Blaze/Audio/AudioEngine.h"
#include "Blaze/ImGui/ScriptEditorWindow.h"
#include "Blaze/Logging/Log.h"
#include <filesystem>
#include <cctype>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace Blaze
{
	float ComponentsWindow::width = 300.f;
	float ComponentsWindow::height = 400.f;

	static const char* s_TexturePayload = "BLAZE_TEXTURE";
	static const char* s_AudioPayload = "BLAZE_AUDIO";

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

	static bool IsAudioExtension(std::string extension)
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".wav" || extension == ".mp3" ||
			   extension == ".ogg" || extension == ".flac";
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

	static void CollectContentAudio(const fs::path& root, std::vector<std::string>& out)
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
			if (IsAudioExtension(it->path().extension().string()))
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
		else if (m_Selected->IsSound())
			ImGui::TextDisabled("Type: Sound");
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

			int layer = m_Selected->GetLayer();
			if (ImGui::InputInt("Layer", &layer))
				m_Selected->SetLayer(layer);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Higher layers render in front");
		}

		if (m_Selected->IsCamera())
		{
			if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
			{
				bool isMain = m_Selected->DrivesView();
				if (ImGui::Checkbox("Main Camera", &isMain))
				{
					if (isMain)
						AssignMainCamera(m_Selected);
					else
						m_Selected->SetDrivesView(false);
				}
				if (isMain)
					ImGui::TextDisabled("This camera renders the Game view");

				float zoom = m_Selected->GetZoom();
				if (ImGui::InputFloat("Zoom", &zoom, 0.0f, 0.0f, "%.3f"))
					m_Selected->SetZoom(zoom);
			}
		}
		else if (!m_Selected->IsSound() && ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen))
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

		if (m_Selected->HasAudio())
		{
			if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const std::string& audioPath = m_Selected->GetAudioPath();
				if (audioPath.empty())
					ImGui::TextDisabled("No clip");
				else
					ImGui::TextWrapped("%s", audioPath.c_str());

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_AudioPayload))
					{
						std::string path((const char*)payload->Data);
						m_Selected->SetAudioPath(path);
						BZ_INFO("Assigned audio: {0}", path);
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::TextDisabled("Drag .wav/.mp3/.ogg from Content Browser");
				if (ImGui::Button("Select Clip..."))
					ImGui::OpenPopup("ContentBrowserAudioPicker");

				if (ImGui::BeginPopup("ContentBrowserAudioPicker"))
				{
					ImGui::TextUnformatted("Content Browser audio");
					ImGui::Separator();

					std::vector<std::string> clips;
					CollectContentAudio("content", clips);

					if (clips.empty())
						ImGui::TextDisabled("No audio found in content/");
					else
					{
						ImGui::BeginChild("AudioPickerList", ImVec2(280.0f, 240.0f), true);
						for (const std::string& clipPath : clips)
						{
							ImGui::PushID(clipPath.c_str());
							bool selected = (clipPath == audioPath);
							if (ImGui::Selectable(fs::path(clipPath).filename().string().c_str(), selected))
							{
								m_Selected->SetAudioPath(clipPath);
								ImGui::CloseCurrentPopup();
							}
							if (ImGui::IsItemHovered())
								ImGui::SetTooltip("%s", clipPath.c_str());
							ImGui::PopID();
						}
						ImGui::EndChild();
					}
					ImGui::EndPopup();
				}

				float volume = m_Selected->GetAudioVolume();
				if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f, "%.2f"))
					m_Selected->SetAudioVolume(volume);

				bool loop = m_Selected->GetAudioLoop();
				if (ImGui::Checkbox("Loop", &loop))
					m_Selected->SetAudioLoop(loop);

				bool playOnStart = m_Selected->GetAudioPlayOnStart();
				if (ImGui::Checkbox("Play On Start", &playOnStart))
					m_Selected->SetAudioPlayOnStart(playOnStart);

				if (ImGui::Button("Play", ImVec2(80.0f, 0.0f)))
					AudioEngine::Play(m_Selected);
				ImGui::SameLine();
				if (ImGui::Button("Stop", ImVec2(80.0f, 0.0f)))
					AudioEngine::Stop(m_Selected);

				if (!m_Selected->IsSound() && ImGui::Button("Remove Audio", ImVec2(-1.0f, 0.0f)))
				{
					AudioEngine::Stop(m_Selected);
					m_Selected->RemoveAudio();
				}
			}
		}

		if (m_Selected->HasScript())
		{
			if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const std::string& scriptPath = m_Selected->GetScriptPath();
				if (scriptPath.empty())
					ImGui::TextDisabled("No script");
				else
					ImGui::TextWrapped("%s", scriptPath.c_str());

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BLAZE_SCRIPT"))
					{
						std::string path((const char*)payload->Data);
						m_Selected->SetScriptPath(path);
						BZ_INFO("Assigned script: {0}", path);
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::TextDisabled("Drag .lua from Content Browser");
				if (ImGui::Button("Select Script..."))
					ImGui::OpenPopup("ContentBrowserScriptPicker");
				ImGui::SameLine();
				if (ImGui::Button("Create Script"))
				{
					fs::path scriptsDir = "content/scripts";
					std::error_code ec;
					fs::create_directories(scriptsDir, ec);

					std::string baseName = m_Selected->GetName();
					for (char& c : baseName)
					{
						if (c == ' ' || c == '\\' || c == '/' || c == ':' || c == '*' ||
							c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
							c = '_';
					}
					if (baseName.empty())
						baseName = "NewScript";

					fs::path path = scriptsDir / (baseName + ".lua");
					int suffix = 1;
					while (fs::exists(path, ec))
					{
						path = scriptsDir / (baseName + "_" + std::to_string(suffix) + ".lua");
						++suffix;
					}

					std::ofstream out(path);
					if (out.is_open())
					{
						out << "-- Blaze Lua script for " << m_Selected->GetName() << "\n\n";
						out << "function OnStart()\n";
						out << "\tprint(\"Script started on\", selfName)\n";
						out << "end\n\n";
						out << "function OnUpdate(dt)\n";
						out << "\tif Input.IsKeyDown(\"A\") then\n";
						out << "\t\tprint(\"A held\")\n";
						out << "\tend\n";
						out << "end\n";
						out.close();

						std::string created = path.generic_string();
						m_Selected->SetScriptPath(created);
						BZ_INFO("Created and assigned script: {0}", created);
						ScriptEditorWindow::OpenFile(created);
					}
					else
					{
						BZ_INFO("Failed to create script: {0}", path.string());
					}
				}

				if (!scriptPath.empty())
				{
					ImGui::SameLine();
					if (ImGui::Button("Edit Script"))
						ScriptEditorWindow::OpenFile(scriptPath);
				}

				if (ImGui::BeginPopup("ContentBrowserScriptPicker"))
				{
					ImGui::TextUnformatted("Content Browser scripts");
					ImGui::Separator();

					std::vector<std::string> scripts;
					{
						std::error_code ec;
						if (fs::exists("content", ec))
						{
							for (auto it = fs::recursive_directory_iterator("content", ec);
								it != fs::recursive_directory_iterator(); ++it)
							{
								if (ec) break;
								if (!it->is_regular_file()) continue;
								std::string ext = it->path().extension().string();
								for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
								if (ext == ".lua")
									scripts.push_back(it->path().string());
							}
						}
					}

					if (scripts.empty())
						ImGui::TextDisabled("No .lua files in content/");
					else
					{
						ImGui::BeginChild("ScriptPickerList", ImVec2(280.0f, 240.0f), true);
						for (const std::string& path : scripts)
						{
							ImGui::PushID(path.c_str());
							bool selected = (path == scriptPath);
							if (ImGui::Selectable(fs::path(path).filename().string().c_str(), selected))
							{
								m_Selected->SetScriptPath(path);
								ImGui::CloseCurrentPopup();
							}
							if (ImGui::IsItemHovered())
								ImGui::SetTooltip("%s", path.c_str());
							ImGui::PopID();
						}
						ImGui::EndChild();
					}
					ImGui::EndPopup();
				}

				ImGui::TextDisabled("Runs OnStart / OnUpdate while playing");
				if (ImGui::Button("Remove Script", ImVec2(-1.0f, 0.0f)))
					m_Selected->RemoveScript();
			}
		}

		if (m_Selected->HasCollision() && !m_Selected->IsSound() && !m_Selected->IsCamera())
		{
			if (ImGui::CollapsingHeader("Collision", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const char* typeNames[] = { "Circle", "Block", "PhysicsRigidbody2D" };
				int typeIndex = (int)m_Selected->GetCollisionType();
				if (ImGui::Combo("Type", &typeIndex, typeNames, 3))
					m_Selected->SetCollisionType((CollisionType)typeIndex);

				CollisionType collisionType = m_Selected->GetCollisionType();
				if (collisionType == CollisionType::Circle)
				{
					float radius = m_Selected->GetCollisionRadius();
					if (ImGui::InputFloat("Radius", &radius, 0.0f, 0.0f, "%.3f"))
						m_Selected->SetCollisionRadius(radius);
				}
				else if (collisionType == CollisionType::Block)
				{
					glm::vec2 size = m_Selected->GetCollisionSize();
					if (ImGui::InputFloat2("Size", &size.x, "%.3f"))
						m_Selected->SetCollisionSize(size);
				}
				else if (collisionType == CollisionType::PhysicsRigidbody2D)
				{
					float mass = m_Selected->GetRigidbodyMass();
					if (ImGui::InputFloat("Mass", &mass, 0.0f, 0.0f, "%.3f"))
						m_Selected->SetRigidbodyMass(mass);

					float gravity = m_Selected->GetRigidbodyGravityScale();
					if (ImGui::InputFloat("Gravity Scale", &gravity, 0.0f, 0.0f, "%.3f"))
						m_Selected->SetRigidbodyGravityScale(gravity);

					bool kinematic = m_Selected->IsRigidbodyKinematic();
					if (ImGui::Checkbox("Kinematic", &kinematic))
						m_Selected->SetRigidbodyKinematic(kinematic);

					ImGui::Separator();
					ImGui::TextUnformatted("Collider Shape");
					const char* shapeNames[] = { "Circle", "Block" };
					int shapeIndex = (int)m_Selected->GetColliderShape();
					if (ImGui::Combo("Shape", &shapeIndex, shapeNames, 2))
						m_Selected->SetColliderShape((ColliderShape)shapeIndex);

					if (m_Selected->GetColliderShape() == ColliderShape::Circle)
					{
						float radius = m_Selected->GetCollisionRadius();
						if (ImGui::InputFloat("Radius", &radius, 0.0f, 0.0f, "%.3f"))
							m_Selected->SetCollisionRadius(radius);
					}
					else
					{
						glm::vec2 size = m_Selected->GetCollisionSize();
						if (ImGui::InputFloat2("Size", &size.x, "%.3f"))
							m_Selected->SetCollisionSize(size);
					}
				}

				if (ImGui::Button("Remove Collision", ImVec2(-1.0f, 0.0f)))
					m_Selected->RemoveCollision();
			}
		}

		ImGui::Spacing();
		if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
			ImGui::OpenPopup("AddComponentPopup");

		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			const bool canAddCollision = !m_Selected->IsSound() && !m_Selected->IsCamera();
			if (canAddCollision && !m_Selected->HasCollision() && ImGui::MenuItem("Collision"))
			{
				m_Selected->AddCollision(CollisionType::Block);
				ImGui::CloseCurrentPopup();
			}
			if (canAddCollision && m_Selected->HasCollision())
				ImGui::TextDisabled("Collision already added");
			if (!canAddCollision)
				ImGui::TextDisabled("Collision not available");

			if (!m_Selected->HasAudio() && ImGui::MenuItem("Audio"))
			{
				m_Selected->AddAudio();
				ImGui::CloseCurrentPopup();
			}
			if (m_Selected->HasAudio())
				ImGui::TextDisabled("Audio already added");

			if (!m_Selected->HasScript() && ImGui::MenuItem("Script"))
			{
				m_Selected->AddScript();
				ImGui::CloseCurrentPopup();
			}
			if (m_Selected->HasScript())
				ImGui::TextDisabled("Script already added");

			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void ComponentsWindow::SetSize(float w, float h)
	{
		width = w;
		height = h;
	}

	void ComponentsWindow::AssignMainCamera(GameObject* camera)
	{
		if (!camera || !camera->IsCamera())
			return;

		for (GameObject* object : m_Objects)
		{
			if (object && object->IsCamera())
				object->SetDrivesView(object == camera);
		}
		camera->SetDrivesView(true);
	}
}
