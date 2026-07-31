#include "ComponentsWindow.h"

#include "../vendor/imgui/src/imgui.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Texture.h"
#include "Blaze/Audio/AudioEngine.h"
#include "Blaze/ImGui/ScriptEditorWindow.h"
#include "Blaze/ImGui/PropertiesWindow.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Editor/UndoStack.h"
#include "Blaze/Logging/Log.h"
#include "Blaze/Renderer/FlipbookAsset.h"
#include "Blaze/Scripting/SaveObject.h"
#include <filesystem>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

namespace fs = std::filesystem;

namespace Blaze
{
	float ComponentsWindow::width = 300.f;
	float ComponentsWindow::height = 400.f;
	int ComponentsWindow::s_TilePaintBrush = 0;
	bool ComponentsWindow::s_TilePaintMode = false;

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

	static bool IsFontExtension(std::string extension)
	{
		for (auto& c : extension)
			c = (char)std::tolower((unsigned char)c);
		return extension == ".ttf" || extension == ".otf";
	}

	static void CollectContentFonts(const fs::path& root, std::vector<std::string>& out)
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
			if (IsFontExtension(it->path().extension().string()))
				out.push_back(it->path().generic_string());
		}
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

		if (m_MapSelected)
		{
			const std::string mapName = Project::GetActiveMap().empty()
				? "Untitled Map"
				: fs::path(Project::GetActiveMap()).stem().string();

			ImGui::Text("Map: %s", mapName.c_str());
			ImGui::TextDisabled("Level Blueprint / Map Script");
			ImGui::Separator();

			if (ImGui::CollapsingHeader("Map Script", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const std::string scriptPath = Project::EnsureMapScript();
				if (scriptPath.empty())
				{
					ImGui::TextDisabled("No map script");
				}
				else
				{
					ImGui::TextWrapped("%s", scriptPath.c_str());
					if (ImGui::Button("Open Script", ImVec2(-1.0f, 0.0f)))
						ScriptEditorWindow::OpenFile(scriptPath);
				}
			}

			ImGui::End();
			return;
		}

		if (!m_Selected)
		{
			ImGui::TextDisabled("No object selected");
			ImGui::TextDisabled("Select the map root for Map Script");
			ImGui::End();
			return;
		}

		ImGui::Text("Name: %s", m_Selected->GetName().c_str());
		if (m_Selected->IsCamera())
			ImGui::TextDisabled("Type: Camera");
		else if (m_Selected->IsSound())
			ImGui::TextDisabled("Type: Sound");
		else if (m_Selected->IsText())
			ImGui::TextDisabled("Type: Text");
		else if (m_Selected->IsTilemap())
			ImGui::TextDisabled("Type: Tilemap");

		bool visible = m_Selected->IsVisible();
		if (ImGui::Checkbox("Visible", &visible))
			m_Selected->SetVisible(visible);
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const bool playLocked = StartWindow::IsPlaying();
			if (playLocked)
				ImGui::TextDisabled("Locked while playing — edit in Game view only");

			if (GameObject* parent = m_Selected->GetParent())
				ImGui::TextDisabled("Parent: %s", parent->GetName().c_str());
			else
				ImGui::TextDisabled("Parent: (none)");

			if (playLocked)
				ImGui::BeginDisabled();

			auto pushTransformIfChanged = [this]()
			{
				if (!m_Selected || !m_TransformEditActive)
					return;
				LocalTransformSnap after = LocalTransformSnap::Capture(*m_Selected);
				if (!m_TransformEditBefore.Equals(after))
				{
					UndoStack::Get().Push(std::make_unique<TransformCommand>(
						m_Selected, m_TransformEditBefore, after));
				}
				m_TransformEditActive = false;
			};

			LocalTransformSnap beforePos = LocalTransformSnap::Capture(*m_Selected);
			glm::vec3 localPos = beforePos.position;
			if (ImGui::InputFloat3("Local Position", &localPos.x, "%.3f"))
				m_Selected->SetLocalPosition(localPos);
			if (ImGui::IsItemActivated())
			{
				m_TransformEditBefore = beforePos;
				m_TransformEditActive = true;
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
				pushTransformIfChanged();

			LocalTransformSnap beforeRot = LocalTransformSnap::Capture(*m_Selected);
			float localRot = beforeRot.rotation;
			if (ImGui::InputFloat("Local Rotation", &localRot, 0.0f, 0.0f, "%.3f"))
				m_Selected->SetLocalRotation(localRot);
			if (ImGui::IsItemActivated())
			{
				m_TransformEditBefore = beforeRot;
				m_TransformEditActive = true;
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
				pushTransformIfChanged();

			LocalTransformSnap beforeScale = LocalTransformSnap::Capture(*m_Selected);
			glm::vec2 localScale = beforeScale.scale;
			if (ImGui::InputFloat2("Local Scale", &localScale.x, "%.3f"))
			{
				localScale.x = glm::max(localScale.x, 0.05f);
				localScale.y = glm::max(localScale.y, 0.05f);
				m_Selected->SetLocalScale(localScale);
			}
			if (ImGui::IsItemActivated())
			{
				m_TransformEditBefore = beforeScale;
				m_TransformEditActive = true;
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
				pushTransformIfChanged();

			glm::vec3 worldPos = m_Selected->GetWorldPosition();
			ImGui::TextDisabled("World: %.1f, %.1f  rot %.1f", worldPos.x, worldPos.y, m_Selected->GetWorldRotation());

			int layer = m_Selected->GetLayer();
			if (ImGui::InputInt("Layer", &layer))
				m_Selected->SetLayer(layer);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Higher layers render in front");

			if (playLocked)
				ImGui::EndDisabled();
		}

		if (ImGui::CollapsingHeader("Tags", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const std::vector<std::string>& tags = m_Selected->GetTags();
			int removeIndex = -1;

			for (size_t i = 0; i < tags.size(); ++i)
			{
				ImGui::PushID((int)i);

				char buf[128] = {};
				std::snprintf(buf, sizeof(buf), "%s", tags[i].c_str());

				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 28.0f);
				if (ImGui::InputText("##tag", buf, sizeof(buf)))
					m_Selected->SetTagAt(i, buf);

				ImGui::SameLine();
				if (ImGui::Button("x", ImVec2(22.0f, 0.0f)))
					removeIndex = (int)i;

				ImGui::PopID();
			}

			if (removeIndex >= 0)
				m_Selected->RemoveTagAt((size_t)removeIndex);

			if (ImGui::Button("+", ImVec2(28.0f, 0.0f)))
			{
				std::string name = "Tag";
				int suffix = 1;
				while (m_Selected->HasTag(name))
				{
					name = "Tag" + std::to_string(suffix);
					++suffix;
				}
				m_Selected->AddTag(name);
			}
			ImGui::SameLine();
			ImGui::TextDisabled("Add tag");
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
					ImGui::TextDisabled("Renders Game view. Parent under an object to follow it.");

				float zoom = m_Selected->GetZoom();
				if (ImGui::InputFloat("Zoom", &zoom, 0.0f, 0.0f, "%.3f"))
					m_Selected->SetZoom(zoom);
			}
		}
		else if (m_Selected->IsTilemap() && ImGui::CollapsingHeader("Tilemap", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Tileset Texture");

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
					BZ_INFO("Assigned tileset: {0}", path);
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::SameLine();
			ImGui::BeginGroup();
			const std::string& path = m_Selected->GetTexturePath();
			if (path.empty())
				ImGui::TextDisabled("No tileset");
			else
				ImGui::TextWrapped("%s", path.c_str());
			ImGui::TextDisabled("Drag a sprite sheet from Content Browser");
			ImGui::EndGroup();

			ImGui::Separator();
			int mapCols = m_Selected->GetTilemapColumns();
			int mapRows = m_Selected->GetTilemapRows();
			float cellSize = m_Selected->GetTileCellSize();
			int setCols = m_Selected->GetTilesetColumns();
			int setRows = m_Selected->GetTilesetRows();

			if (ImGui::InputInt("Map Width", &mapCols))
				m_Selected->SetTilemapSize(mapCols, m_Selected->GetTilemapRows());
			if (ImGui::InputInt("Map Height", &mapRows))
				m_Selected->SetTilemapSize(m_Selected->GetTilemapColumns(), mapRows);
			if (ImGui::InputFloat("Cell Size", &cellSize, 1.0f, 8.0f, "%.0f"))
				m_Selected->SetTileCellSize(cellSize);
			if (ImGui::InputInt("Tileset Columns", &setCols))
				m_Selected->SetTilesetGrid(setCols, m_Selected->GetTilesetRows());
			if (ImGui::InputInt("Tileset Rows", &setRows))
				m_Selected->SetTilesetGrid(m_Selected->GetTilesetColumns(), setRows);

			ImGui::Separator();
			ImGui::TextUnformatted("Layers");
			if (ImGui::Button("Add Layer"))
			{
				const int n = m_Selected->GetTileLayerCount();
				m_Selected->AddTileLayer("Layer " + std::to_string(n));
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove Layer") && m_Selected->GetTileLayerCount() > 1)
				m_Selected->RemoveTileLayer(m_Selected->GetActiveTileLayer());

			for (int li = 0; li < m_Selected->GetTileLayerCount(); ++li)
			{
				const TilemapLayer* layer = m_Selected->GetTileLayer(li);
				if (!layer)
					continue;
				ImGui::PushID(li);
				bool selected = (li == m_Selected->GetActiveTileLayer());
				if (ImGui::Selectable(("##sel" + std::to_string(li)).c_str(), selected, 0, ImVec2(18.0f, 0.0f)))
					m_Selected->SetActiveTileLayer(li);
				ImGui::SameLine();
				char nameBuf[64] = {};
				strncpy_s(nameBuf, layer->name.c_str(), _TRUNCATE);
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
					m_Selected->SetTileLayerName(li, nameBuf);
				ImGui::SameLine();
				bool visible = layer->visible;
				if (ImGui::Checkbox("Vis", &visible))
					m_Selected->SetTileLayerVisible(li, visible);
				ImGui::SameLine();
				bool collision = layer->collision;
				if (ImGui::Checkbox("Col", &collision))
					m_Selected->SetTileLayerCollision(li, collision);
				ImGui::SameLine();
				float opacity = layer->opacity;
				ImGui::SetNextItemWidth(60.0f);
				if (ImGui::SliderFloat("##op", &opacity, 0.0f, 1.0f, "%.2f"))
					m_Selected->SetTileLayerOpacity(li, opacity);
				ImGui::PopID();
			}
			ImGui::TextDisabled("Active layer is used by the paint brush.");

			ImGui::Separator();
			bool tileCol = m_Selected->IsTileCollisionEnabled();
			if (ImGui::Checkbox("Tile Collision", &tileCol))
				m_Selected->SetTileCollisionEnabled(tileCol);
			ImGui::TextDisabled("Solid painted tiles become static boxes in play.");
			if (ImGui::Button("Mark All Solid"))
				m_Selected->SetAllTilesetSolid(true);
			ImGui::SameLine();
			if (ImGui::Button("Clear Solids"))
				m_Selected->SetAllTilesetSolid(false);
			ImGui::TextDisabled("Tileset: LMB paint brush, RMB toggle solid.");

			ImGui::Separator();
			ImGui::TextUnformatted("Paint brush");
			bool paintMode = s_TilePaintMode;
			if (ImGui::Checkbox("Lock paint (B)", &paintMode))
				s_TilePaintMode = paintMode;
			ImGui::TextDisabled("Click/drag map to paint. RMB erases.");
			ImGui::TextDisabled("Grab red/green arrows to move (unless locked).");

			int& s_PaintTile = s_TilePaintBrush;
			const int tileCount = m_Selected->GetTilesetTileCount();
			if (ImGui::SliderInt("Tile ID", &s_PaintTile, -1, glm::max(tileCount - 1, 0)))
			{
			}
			ImGui::TextDisabled("-1 = erase");
			if (ImGui::Button("Fill"))
			{
				std::vector<TileChange> changes;
				const int cols = m_Selected->GetTilemapColumns();
				const int rows = m_Selected->GetTilemapRows();
				for (int y = 0; y < rows; ++y)
				{
					for (int x = 0; x < cols; ++x)
					{
						const int oldId = m_Selected->GetTile(x, y);
						if (oldId == s_PaintTile)
							continue;
						changes.push_back({ x, y, oldId, s_PaintTile });
					}
				}
				m_Selected->FillTiles(s_PaintTile);
				if (!changes.empty())
					UndoStack::Get().Push(std::make_unique<TileStrokeCommand>(m_Selected, std::move(changes)));
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear"))
			{
				std::vector<TileChange> changes;
				const int cols = m_Selected->GetTilemapColumns();
				const int rows = m_Selected->GetTilemapRows();
				for (int y = 0; y < rows; ++y)
				{
					for (int x = 0; x < cols; ++x)
					{
						const int oldId = m_Selected->GetTile(x, y);
						if (oldId < 0)
							continue;
						changes.push_back({ x, y, oldId, -1 });
					}
				}
				m_Selected->ClearTiles();
				if (!changes.empty())
					UndoStack::Get().Push(std::make_unique<TileStrokeCommand>(m_Selected, std::move(changes)));
			}

			if (m_Selected->HasTexture() && tileCount > 0)
			{
				ImGui::TextUnformatted("Tileset picker");
				const float brushThumb = 28.0f;
				int brushCols = (int)(ImGui::GetContentRegionAvail().x / (brushThumb + 4.0f));
				if (brushCols < 1) brushCols = 1;
				ImGui::BeginChild("TilesetPicker", ImVec2(0.0f, brushThumb * 3.5f), true);
				ImGui::Columns(brushCols, nullptr, false);
				for (int id = 0; id < tileCount; ++id)
				{
					ImGui::PushID(id);
					const int cols = glm::max(m_Selected->GetTilesetColumns(), 1);
					const int rows = glm::max(m_Selected->GetTilesetRows(), 1);
					const int col = id % cols;
					const int rowFromTop = id / cols;
					ImVec2 uv0((float)col / (float)cols, (float)rowFromTop / (float)rows);
					ImVec2 uv1(((float)col + 1.0f) / (float)cols, ((float)rowFromTop + 1.0f) / (float)rows);
					const bool isSolid = m_Selected->IsTilesetSolid(id);
					ImVec4 tint = (s_PaintTile == id) ? ImVec4(0.6f, 0.85f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);
					if (isSolid)
						tint = ImVec4(tint.x * 0.85f, 1.0f, tint.z * 0.7f, 1.0f);
					if (ImGui::ImageButton("##tile", (ImTextureID)(intptr_t)m_Selected->GetTextureID(),
						ImVec2(brushThumb, brushThumb), uv0, uv1, ImVec4(0, 0, 0, 0), tint))
					{
						s_PaintTile = id;
					}
					if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
						m_Selected->SetTilesetSolid(id, !m_Selected->IsTilesetSolid(id));
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Tile %d%s", id, isSolid ? " (solid)" : "");
					if (isSolid)
					{
						ImVec2 mn = ImGui::GetItemRectMin();
						ImVec2 mx = ImGui::GetItemRectMax();
						ImGui::GetWindowDrawList()->AddRect(mn, mx, IM_COL32(80, 220, 120, 255), 0.0f, 0, 2.0f);
					}
					ImGui::PopID();
					ImGui::NextColumn();
				}
				ImGui::Columns(1);
				ImGui::EndChild();
			}

			ImGui::TextUnformatted("Paint grid (hold & drag)");
			ImGui::TextDisabled("LMB paint  |  RMB erase");
			ImGui::BeginChild("TilePaintGrid", ImVec2(0.0f, 220.0f), true,
				ImGuiWindowFlags_NoMove);
			const float cellBtn = 18.0f;
			const bool paintHeld = ImGui::IsMouseDown(ImGuiMouseButton_Left);
			const bool eraseHeld = ImGui::IsMouseDown(ImGuiMouseButton_Right);
			const bool anyStroke = paintHeld || eraseHeld;
			if (!anyStroke && m_CompTileStrokeActive)
			{
				if (!m_CompTileStrokeChanges.empty())
				{
					UndoStack::Get().Push(std::make_unique<TileStrokeCommand>(
						m_Selected, std::move(m_CompTileStrokeChanges)));
				}
				m_CompTileStrokeChanges.clear();
				m_CompTileStrokeActive = false;
			}
			for (int y = m_Selected->GetTilemapRows() - 1; y >= 0; --y)
			{
				for (int x = 0; x < m_Selected->GetTilemapColumns(); ++x)
				{
					ImGui::PushID(y * 10000 + x);
					const int tile = m_Selected->GetTile(x, y);
					char label[16];
					if (tile < 0)
						std::snprintf(label, sizeof(label), " ");
					else
						std::snprintf(label, sizeof(label), "%d", tile);

					ImGui::Button(label, ImVec2(cellBtn, cellBtn));
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("(%d, %d) = %d", x, y, tile);
						if (paintHeld || eraseHeld)
						{
							const int newId = paintHeld ? s_PaintTile : -1;
							if (tile != newId)
							{
								m_CompTileStrokeActive = true;
								bool found = false;
								for (TileChange& change : m_CompTileStrokeChanges)
								{
									if (change.x == x && change.y == y)
									{
										change.newId = newId;
										found = true;
										break;
									}
								}
								if (!found)
									m_CompTileStrokeChanges.push_back({ x, y, tile, newId });
								m_Selected->SetTile(x, y, newId);
							}
						}
					}
					ImGui::PopID();
					if (x + 1 < m_Selected->GetTilemapColumns())
						ImGui::SameLine(0.0f, 1.0f);
				}
			}
			ImGui::EndChild();
		}
		if (m_Selected->IsText())
		{
			if (ImGui::CollapsingHeader("Text Renderer", ImGuiTreeNodeFlags_DefaultOpen))
			{
				char buf[512];
				std::snprintf(buf, sizeof(buf), "%s", m_Selected->GetText().c_str());
				if (ImGui::InputTextMultiline("Content", buf, sizeof(buf), ImVec2(-1.0f, 80.0f)))
					m_Selected->SetText(buf);

				float fontSize = m_Selected->GetFontSize();
				if (ImGui::DragFloat("Font Size", &fontSize, 0.5f, 1.0f, 512.0f, "%.1f"))
					m_Selected->SetFontSize(fontSize);

				ImGui::Text("Font");
				const std::string& fontPath = m_Selected->GetFontPath();
				if (fontPath.empty())
					ImGui::TextDisabled("Default (Arial)");
				else
				{
					const std::string fileName = fs::path(fontPath).filename().string();
					ImGui::TextUnformatted(fileName.c_str());
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s", fontPath.c_str());
				}

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BLAZE_FONT"))
					{
						m_Selected->SetFontPath(std::string((const char*)payload->Data));
						BZ_INFO("Assigned font: {0}", m_Selected->GetFontPath());
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::TextDisabled("Drag from Content Browser or Select...");
				if (ImGui::Button("Select Font...", ImVec2(-1.0f, 0.0f)))
					ImGui::OpenPopup("ContentBrowserFontPicker");

				if (ImGui::BeginPopup("ContentBrowserFontPicker"))
				{
					ImGui::TextUnformatted("Project fonts (.ttf / .otf)");
					ImGui::Separator();

					std::vector<std::string> fonts;
					const std::string contentRoot = Project::HasProject()
						? Project::GetContentDirectory()
						: "content";
					CollectContentFonts(contentRoot, fonts);
					if (contentRoot != "content")
						CollectContentFonts("content", fonts);

					if (fonts.empty())
					{
						ImGui::TextDisabled("No fonts found. Import via Content Browser.");
					}
					else
					{
						ImGui::BeginChild("FontPickerList", ImVec2(320.0f, 240.0f), true);
						for (const std::string& path : fonts)
						{
							ImGui::PushID(path.c_str());
							const std::string label = fs::path(path).filename().string();
							bool selected = (path == fontPath);
							if (!selected && !fontPath.empty())
							{
								std::error_code ec;
								selected = fs::equivalent(path, fontPath, ec);
							}
							if (ImGui::Selectable(label.c_str(), selected))
							{
								m_Selected->SetFontPath(path);
								ImGui::CloseCurrentPopup();
							}
							if (ImGui::IsItemHovered())
								ImGui::SetTooltip("%s", path.c_str());
							ImGui::PopID();
						}
						ImGui::EndChild();
					}

					if (ImGui::Button("Use Default", ImVec2(-1.0f, 0.0f)))
					{
						m_Selected->SetFontPath("");
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}

				if (!fontPath.empty() && ImGui::Button("Clear Font", ImVec2(-1.0f, 0.0f)))
					m_Selected->SetFontPath("");

				glm::vec4 color = m_Selected->GetColor();
				if (ImGui::ColorEdit4("Color", &color.x))
					m_Selected->SetColor(color);
			}
		}
		else if (!m_Selected->IsSound() && !m_Selected->IsCamera() && !m_Selected->IsTilemap()
			&& ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen))
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

			ImGui::Separator();
			ImGui::Text("Flipbook / Sprite Sheet");
			ImGui::TextDisabled("Grid on the texture (left-to-right, top-to-bottom)");

			int cols = m_Selected->GetFlipbookColumns();
			int rows = m_Selected->GetFlipbookRows();
			int frameCount = m_Selected->GetFlipbookFrameCount();
			float fps = m_Selected->GetFlipbookFps();
			int frame = m_Selected->GetFlipbookFrame();
			bool playing = m_Selected->IsFlipbookPlaying();
			bool loop = m_Selected->GetFlipbookLoop();

			if (ImGui::InputInt("Columns", &cols))
				m_Selected->SetFlipbookColumns(cols);
			if (ImGui::InputInt("Rows", &rows))
				m_Selected->SetFlipbookRows(rows);
			if (ImGui::InputInt("Frame Count", &frameCount))
				m_Selected->SetFlipbookFrameCount(frameCount);
			ImGui::TextDisabled("0 = use Columns x Rows");
			if (ImGui::InputFloat("Default FPS", &fps, 1.0f, 5.0f, "%.1f"))
				m_Selected->SetFlipbookFps(fps);
			ImGui::SameLine();
			ImGui::TextDisabled("(clips can override)");
			if (ImGui::Checkbox("Default Loop", &loop))
				m_Selected->SetFlipbookLoop(loop);

			int playStart = 0, playEnd = 0;
			m_Selected->GetFlipbookPlaybackRange(playStart, playEnd);
			if (ImGui::SliderInt("Frame", &frame, playStart, playEnd))
				m_Selected->SetFlipbookFrame(frame);
			if (ImGui::Checkbox("Playing", &playing))
				m_Selected->SetFlipbookPlaying(playing);

			if (ImGui::Button("Play"))
				m_Selected->PlayFlipbook();
			ImGui::SameLine();
			if (ImGui::Button("Pause"))
				m_Selected->PauseFlipbook();
			ImGui::SameLine();
			if (ImGui::Button("Stop"))
				m_Selected->StopFlipbook();

			ImGui::Separator();
			ImGui::Text("Flipbook Asset");
			ImGui::TextDisabled("Saveable animation set (.flipbook) — not a GameObject");
			{
				const std::string& assetPath = m_Selected->GetFlipbookAssetPath();
				if (assetPath.empty())
					ImGui::TextDisabled("No flipbook asset assigned");
				else
					ImGui::TextWrapped("%s", fs::path(assetPath).filename().string().c_str());

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BLAZE_FLIPBOOK"))
					{
						std::string path((const char*)payload->Data);
						m_Selected->SetFlipbookAssetPath(path);
						BZ_INFO("Assigned flipbook asset: {0}", path);
					}
					ImGui::EndDragDropTarget();
				}

				if (ImGui::Button("Select Flipbook..."))
					ImGui::OpenPopup("FlipbookAssetPicker");
				ImGui::SameLine();
				if (ImGui::Button("Create Flipbook Asset"))
				{
					fs::path dir = "content/animations";
					std::error_code ec;
					fs::create_directories(dir, ec);
					std::string baseName = m_Selected->GetName();
					for (char& c : baseName)
					{
						if (c == ' ' || c == '\\' || c == '/' || c == ':' || c == '*' ||
							c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
							c = '_';
					}
					if (baseName.empty())
						baseName = "NewFlipbook";
					fs::path path = dir / (baseName + ".flipbook");
					int suffix = 1;
					while (fs::exists(path, ec))
					{
						path = dir / (baseName + "_" + std::to_string(suffix) + ".flipbook");
						++suffix;
					}

					FlipbookAsset asset = FlipbookAsset::CreateDefault(baseName);
					asset.SetColumns(m_Selected->GetFlipbookColumns());
					asset.SetRows(m_Selected->GetFlipbookRows());
					asset.SetFrameCount(m_Selected->GetFlipbookFrameCount());
					asset.SetFps(m_Selected->GetFlipbookFps());
					if (!m_Selected->GetFlipbookClips().empty())
						asset.SetClips(m_Selected->GetFlipbookClips());
					asset.SetPath(path.string());
					if (asset.SaveToFile(path.string()))
					{
						m_Selected->SetFlipbookAssetPath(path.generic_string());
						BZ_INFO("Created flipbook asset: {0}", path.string());
					}
				}
				if (m_Selected->HasFlipbookAsset())
				{
					ImGui::SameLine();
					if (ImGui::Button("Save Asset"))
						m_Selected->SaveFlipbookAsset();
					ImGui::SameLine();
					if (ImGui::Button("Clear Asset"))
						m_Selected->SetFlipbookAssetPath("", false);
				}

				if (ImGui::BeginPopup("FlipbookAssetPicker"))
				{
					ImGui::TextUnformatted("Flipbook assets");
					ImGui::Separator();
					std::vector<std::string> assets;
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
								if (ext == ".flipbook")
									assets.push_back(it->path().string());
							}
						}
					}
					if (assets.empty())
						ImGui::TextDisabled("No .flipbook files in content/");
					else
					{
						ImGui::BeginChild("FlipbookPickerList", ImVec2(280.0f, 200.0f), true);
						for (const std::string& path : assets)
						{
							ImGui::PushID(path.c_str());
							if (ImGui::Selectable(fs::path(path).filename().string().c_str(), path == assetPath))
							{
								m_Selected->SetFlipbookAssetPath(path);
								ImGui::CloseCurrentPopup();
							}
							ImGui::PopID();
						}
						ImGui::EndChild();
					}
					ImGui::EndPopup();
				}
			}

			ImGui::Separator();
			ImGui::Text("Animations (Clips)");
			ImGui::TextDisabled("Named ranges — saved on .flipbook asset when assigned");

			auto PersistFlipbookIfNeeded = [&]()
			{
				if (m_Selected->HasFlipbookAsset())
					m_Selected->SaveFlipbookAsset();
			};

			const std::vector<FlipbookClip>& clips = m_Selected->GetFlipbookClips();
			static int s_SelectedClip = -1;
			static char s_NewClipName[64] = "Idle";
			static uintptr_t s_LastFlipObject = 0;
			const uintptr_t selectedId = (uintptr_t)m_Selected;
			if (selectedId != s_LastFlipObject)
			{
				s_LastFlipObject = selectedId;
				s_SelectedClip = m_Selected->GetActiveFlipbookClipIndex();
				if (s_SelectedClip < 0 && !clips.empty())
					s_SelectedClip = 0;
			}
			if (s_SelectedClip >= (int)clips.size())
				s_SelectedClip = clips.empty() ? -1 : (int)clips.size() - 1;

			ImGui::SetNextItemWidth(160.0f);
			ImGui::InputText("##NewClipName", s_NewClipName, sizeof(s_NewClipName));
			ImGui::SameLine();
			if (ImGui::Button("Add Animation"))
			{
				std::string name = s_NewClipName;
				if (name.empty())
					name = "Clip";
				const int total = m_Selected->GetFlipbookTotalFrames();
				const int end = glm::max(total - 1, 0);
				s_SelectedClip = m_Selected->AddFlipbookClip(name, 0, end, -1.0f, true);
				m_Selected->SetActiveFlipbookClip(m_Selected->GetFlipbookClips()[s_SelectedClip].name);
				PersistFlipbookIfNeeded();
			}

			if (clips.empty())
			{
				ImGui::TextDisabled("No clips — plays full sheet / Frame Count");
			}
			else
			{
				if (ImGui::BeginListBox("##FlipClips", ImVec2(-1.0f, 100.0f)))
				{
					for (int i = 0; i < (int)clips.size(); ++i)
					{
						const bool isActive = (i == m_Selected->GetActiveFlipbookClipIndex());
						char label[128];
						std::snprintf(label, sizeof(label), "%s%s  [%d-%d]",
							clips[i].name.c_str(),
							isActive ? " *" : "",
							clips[i].startFrame,
							clips[i].endFrame);
						if (ImGui::Selectable(label, s_SelectedClip == i))
						{
							s_SelectedClip = i;
							m_Selected->SetActiveFlipbookClip(clips[i].name);
						}
						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
							m_Selected->PlayFlipbook(clips[i].name);
					}
					ImGui::EndListBox();
				}

				if (s_SelectedClip >= 0 && s_SelectedClip < (int)clips.size())
				{
					FlipbookClip edit = clips[s_SelectedClip];
					char renameBuf[64];
					std::snprintf(renameBuf, sizeof(renameBuf), "%s", edit.name.c_str());
					bool dirty = false;
					if (ImGui::InputText("Clip Name", renameBuf, sizeof(renameBuf),
						ImGuiInputTextFlags_EnterReturnsTrue))
					{
						std::string newName = renameBuf;
						if (!newName.empty() && newName != edit.name && m_Selected->FindFlipbookClip(newName) < 0)
						{
							edit.name = newName;
							dirty = true;
						}
					}
					const int lastFrame = glm::max(m_Selected->GetFlipbookTotalFrames() - 1, 0);
					if (ImGui::InputInt("Start Frame", &edit.startFrame))
					{
						edit.startFrame = glm::clamp(edit.startFrame, 0, lastFrame);
						if (edit.endFrame < edit.startFrame)
							edit.endFrame = edit.startFrame;
						dirty = true;
					}
					if (ImGui::InputInt("End Frame", &edit.endFrame))
					{
						edit.endFrame = glm::clamp(edit.endFrame, edit.startFrame, lastFrame);
						dirty = true;
					}
					float clipFps = edit.fps;
					bool useDefaultFps = clipFps <= 0.0f;
					if (ImGui::Checkbox("Use Default FPS", &useDefaultFps))
					{
						edit.fps = useDefaultFps ? -1.0f : m_Selected->GetFlipbookFps();
						dirty = true;
					}
					if (!useDefaultFps)
					{
						clipFps = edit.fps > 0.0f ? edit.fps : m_Selected->GetFlipbookFps();
						if (ImGui::InputFloat("Clip FPS", &clipFps, 1.0f, 5.0f, "%.1f"))
						{
							edit.fps = glm::max(clipFps, 0.01f);
							dirty = true;
						}
					}
					if (ImGui::Checkbox("Clip Loop", &edit.loop))
						dirty = true;

					if (dirty)
					{
						std::vector<FlipbookClip> updated = clips;
						updated[s_SelectedClip] = edit;
						const std::string activeBefore = m_Selected->GetActiveFlipbookClipName();
						m_Selected->SetFlipbookClips(updated);
						if (activeBefore == clips[s_SelectedClip].name || activeBefore.empty())
							m_Selected->SetActiveFlipbookClip(edit.name);
						else
							m_Selected->SetActiveFlipbookClip(activeBefore);
						PersistFlipbookIfNeeded();
					}

					if (ImGui::Button("Play Clip"))
						m_Selected->PlayFlipbook(edit.name);
					ImGui::SameLine();
					if (ImGui::Button("Delete Clip"))
					{
						m_Selected->RemoveFlipbookClipAt(s_SelectedClip);
						s_SelectedClip = -1;
						PersistFlipbookIfNeeded();
					}
					ImGui::SameLine();
					if (ImGui::Button("Clear Active"))
						m_Selected->ClearActiveFlipbookClip();
				}
			}
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
					ImGui::OpenPopup("CreateComponentScriptType");

				if (!scriptPath.empty())
				{
					ImGui::SameLine();
					if (ImGui::Button("Edit Script"))
						ScriptEditorWindow::OpenFile(scriptPath);
				}

				if (ImGui::BeginPopupModal("CreateComponentScriptType", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::TextUnformatted("Choose script type");
					ImGui::Separator();
					ImGui::TextWrapped("Component Script — attaches to this GameObject.");
					ImGui::TextWrapped("Save Script — SaveObject data class (not attached).");
					ImGui::Spacing();

					auto MakeScriptPath = [&](const char* baseName) -> fs::path
					{
						fs::path scriptsDir = "content/scripts";
						std::error_code ec;
						fs::create_directories(scriptsDir, ec);
						std::string name = baseName;
						for (char& c : name)
						{
							if (c == ' ' || c == '\\' || c == '/' || c == ':' || c == '*' ||
								c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
								c = '_';
						}
						if (name.empty())
							name = "NewScript";
						fs::path path = scriptsDir / (name + ".lua");
						int suffix = 1;
						while (fs::exists(path, ec))
						{
							path = scriptsDir / (name + "_" + std::to_string(suffix) + ".lua");
							++suffix;
						}
						return path;
					};

					if (ImGui::Button("Component Script", ImVec2(180.0f, 0.0f)))
					{
						fs::path path = MakeScriptPath(m_Selected->GetName().c_str());
						if (ScriptTemplates::WriteScriptFile(path.string(), ScriptKind::Component, m_Selected->GetName()))
						{
							std::string created = path.generic_string();
							m_Selected->SetScriptPath(created);
							BZ_INFO("Created and assigned Component Script: {0}", created);
							ScriptEditorWindow::OpenFile(created);
						}
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if (ImGui::Button("Save Script", ImVec2(180.0f, 0.0f)))
					{
						fs::path path = MakeScriptPath((m_Selected->GetName() + "Save").c_str());
						if (ScriptTemplates::WriteScriptFile(path.string(), ScriptKind::SaveObject, m_Selected->GetName()))
						{
							BZ_INFO("Created Save Script: {0}", path.string());
							ScriptEditorWindow::OpenFile(path.generic_string());
						}
						ImGui::CloseCurrentPopup();
					}
					if (ImGui::Button("Cancel", ImVec2(370.0f, 0.0f)))
						ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
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

				ImGui::Separator();
				bool isTrigger = m_Selected->IsTrigger();
				if (ImGui::Checkbox("Is Trigger", &isTrigger))
					m_Selected->SetIsTrigger(isTrigger);
				ImGui::TextDisabled("Triggers overlap only (no bounce). Use OnTriggerBegin");

				const char* layerNames[] = {
					"0 Default", "1 Player", "2 Enemy", "3 Projectile",
					"4 Trigger", "5 Ground", "6 Pickup", "7 Custom"
				};
				int layer = m_Selected->GetCollisionLayer();
				if (layer < 0) layer = 0;
				if (layer > 7) layer = 7;
				if (ImGui::Combo("Layer", &layer, layerNames, 8))
					m_Selected->SetCollisionLayer(layer);

				ImGui::TextUnformatted("Collides With");
				for (int i = 0; i < 8; ++i)
				{
					bool enabled = m_Selected->CollidesWithLayer(i);
					ImGui::PushID(i);
					if (ImGui::Checkbox(layerNames[i], &enabled))
						m_Selected->SetCollidesWithLayer(i, enabled);
					ImGui::PopID();
					if (i % 2 == 0)
						ImGui::SameLine();
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
