#include "WidgetEditorWindow.h"
#include "Blaze/UI/UISystem.h"
#include "Blaze/Renderer/Texture.h"
#include "Blaze/ImGui/PropertiesWindow.h"
#include "Blaze/Core/Project.h"
#include "../vendor/imgui/src/imgui.h"
#include "Blaze/Logging/Log.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <vector>

namespace fs = std::filesystem;

namespace Blaze
{
	WidgetEditorWindow* WidgetEditorWindow::s_Instance = nullptr;

	static bool IsTextureExtension(std::string extension)
	{
		for (char& c : extension)
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

	WidgetEditorWindow::WidgetEditorWindow()
	{
		s_Instance = this;
	}

	WidgetEditorWindow::~WidgetEditorWindow()
	{
		for (auto& entry : m_Thumbnails)
			delete entry.second;
		m_Thumbnails.clear();
		if (s_Instance == this)
			s_Instance = nullptr;
	}

	Texture* WidgetEditorWindow::GetThumbnail(const std::string& path)
	{
		auto it = m_Thumbnails.find(path);
		if (it != m_Thumbnails.end())
			return it->second;

		Texture* texture = new Texture();
		texture->Generate(path);
		m_Thumbnails[path] = texture;
		return texture;
	}

	void WidgetEditorWindow::OpenFile(const std::string& path)
	{
		if (s_Instance)
			s_Instance->Open(path);
	}

	bool WidgetEditorWindow::IsBlockingMainEditorStatic()
	{
		return s_Instance && s_Instance->IsBlockingMainEditor();
	}

	void WidgetEditorWindow::Open(const std::string& path)
	{
		if (path.empty())
			return;
		CancelRename();
		if (!m_Asset.LoadFromFile(path))
		{
			m_Asset = WidgetAsset::CreateDefault();
			m_Asset.SetPath(path);
			m_Asset.SaveToFile(path);
		}
		m_SelectedName = m_Asset.GetRoot().name;
		m_NameEditTarget.clear();
		m_DragMode = WidgetDragMode::None;
		m_DragNodeName.clear();
		m_ResizingDesign = false;
		m_PanningPreview = false;
		m_PreviewZoom = 1.0f;
		m_PreviewPan = glm::vec2(0.0f);
		m_Dirty = false;
		m_Open = true;
		BZ_INFO("Opened widget: {0}", path);
	}

	bool WidgetEditorWindow::Save()
	{
		if (m_Asset.GetPath().empty())
			return false;
		if (!m_Asset.SaveToFile(m_Asset.GetPath()))
			return false;
		m_Dirty = false;
		BZ_INFO("Saved widget: {0}", m_Asset.GetPath());
		return true;
	}

	WidgetNode* WidgetEditorWindow::FindSelected()
	{
		if (m_SelectedName.empty())
			return nullptr;
		return WidgetAsset::FindByName(m_Asset.GetRoot(), m_SelectedName);
	}

	WidgetNode* WidgetEditorWindow::GetSelectedNode()
	{
		return FindSelected();
	}

	bool WidgetEditorWindow::IsNameTaken(const std::string& name, const WidgetNode* except) const
	{
		const WidgetNode* found = WidgetAsset::FindByName(m_Asset.GetRoot(), name);
		return found != nullptr && found != except;
	}

	bool WidgetEditorWindow::TryRenameNode(WidgetNode* node, const std::string& newName)
	{
		if (!node || newName.empty())
			return false;
		if (newName == node->name)
			return true;
		if (IsNameTaken(newName, node))
			return false;

		if (m_SelectedName == node->name)
			m_SelectedName = newName;
		if (m_RenamingName == node->name)
			m_RenamingName = newName;
		if (m_NameEditTarget == node->name)
			m_NameEditTarget = newName;

		node->name = newName;
		m_Dirty = true;
		return true;
	}

	void WidgetEditorWindow::StartRename(WidgetNode& node)
	{
		m_RenamingName = node.name;
		m_SelectedName = node.name;
		std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
		std::snprintf(m_RenameBuffer, sizeof(m_RenameBuffer), "%s", node.name.c_str());
		m_FocusRename = true;
	}

	void WidgetEditorWindow::CancelRename()
	{
		m_RenamingName.clear();
		m_FocusRename = false;
	}

	void WidgetEditorWindow::CommitRename()
	{
		WidgetNode* node = WidgetAsset::FindByName(m_Asset.GetRoot(), m_RenamingName);
		const std::string newName = m_RenameBuffer;
		CancelRename();
		if (!node)
			return;
		if (newName.empty())
		{
			BZ_INFO("Rename cancelled: empty name");
			return;
		}
		if (!TryRenameNode(node, newName))
			BZ_INFO("Rename cancelled: name already used ({0})", newName);
	}

	void WidgetEditorWindow::AddChild(WidgetType type)
	{
		WidgetNode* parent = FindSelected();
		if (!parent)
			parent = &m_Asset.GetRoot();
		if (parent->type != WidgetType::Canvas && parent->type != WidgetType::Button)
			parent = &m_Asset.GetRoot();

		WidgetNode child;
		child.type = type;
		child.InitDefaults();
		int n = 1;
		std::string base = WidgetNode::TypeName(type);
		child.name = base;
		while (WidgetAsset::FindByName(m_Asset.GetRoot(), child.name))
			child.name = base + std::to_string(n++);

		if (type == WidgetType::Text)
			child.anchorMin = child.anchorMax = glm::vec2(0.5f, 0.35f);
		else if (type == WidgetType::Button)
			child.anchorMin = child.anchorMax = glm::vec2(0.5f, 0.55f);
		else if (type == WidgetType::Image)
			child.anchorMin = child.anchorMax = glm::vec2(0.5f, 0.45f);

		parent->children.push_back(child);
		m_SelectedName = child.name;
		m_Dirty = true;
	}

	void WidgetEditorWindow::DeleteSelected()
	{
		if (m_SelectedName == m_Asset.GetRoot().name)
			return;

		CancelRename();

		std::function<bool(WidgetNode&)> removeFrom = [&](WidgetNode& node) -> bool
		{
			for (auto it = node.children.begin(); it != node.children.end(); ++it)
			{
				if (it->name == m_SelectedName)
				{
					node.children.erase(it);
					return true;
				}
				if (removeFrom(*it))
					return true;
			}
			return false;
		};

		if (removeFrom(m_Asset.GetRoot()))
		{
			m_SelectedName = m_Asset.GetRoot().name;
			m_Dirty = true;
		}
	}

	void WidgetEditorWindow::DrawNodeTree(WidgetNode& node, int depth)
	{
		ImGui::PushID(&node);

		const bool renaming = (m_RenamingName == node.name);
		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_DefaultOpen;
		if (node.name == m_SelectedName)
			flags |= ImGuiTreeNodeFlags_Selected;
		if (node.children.empty())
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

		bool open = false;
		if (renaming)
		{
			open = ImGui::TreeNodeEx("##rename", flags | ImGuiTreeNodeFlags_AllowItemOverlap, "");
			ImGui::SameLine();
			if (m_FocusRename)
			{
				ImGui::SetKeyboardFocusHere();
				m_FocusRename = false;
			}
			const bool renameDone = ImGui::InputText("##widgetrename", m_RenameBuffer, sizeof(m_RenameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				CancelRename();
			}
			else if (renameDone || ImGui::IsItemDeactivated())
			{
				// Enter or click away — leave rename mode
				CommitRename();
			}
		}
		else
		{
			open = ImGui::TreeNodeEx(
				(void*)(intptr_t)(&node), flags, "%s (%s)", node.name.c_str(), WidgetNode::TypeName(node.type));
			if (ImGui::IsItemClicked())
				m_SelectedName = node.name;
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				StartRename(node);

			if (ImGui::BeginPopupContextItem("##widgetctx"))
			{
				m_SelectedName = node.name;
				if (ImGui::MenuItem("Rename"))
					StartRename(node);
				if (node.name != m_Asset.GetRoot().name && ImGui::MenuItem("Delete"))
					m_PendingDelete = true;
				ImGui::EndPopup();
			}
		}

		if (open && !node.children.empty())
		{
			for (WidgetNode& child : node.children)
				DrawNodeTree(child, depth + 1);
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	void WidgetEditorWindow::DrawHierarchy()
	{
		ImGui::BeginChild("##WidgetHierarchy", ImVec2(220.0f, 0.0f), true);
		ImGui::TextUnformatted("Hierarchy");
		ImGui::TextDisabled("Right-click: rename / delete");
		ImGui::Separator();
		DrawNodeTree(m_Asset.GetRoot(), 0);
		ImGui::EndChild();
	}

	void WidgetEditorWindow::DrawInspector()
	{
		WidgetNode* node = FindSelected();
		if (!node)
		{
			ImGui::TextDisabled("No widget element selected");
			return;
		}

		ImGui::TextUnformatted(node->name.c_str());
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Widget", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (m_NameEditTarget != node->name)
			{
				m_NameEditTarget = node->name;
				std::snprintf(m_NameEditBuffer, sizeof(m_NameEditBuffer), "%s", node->name.c_str());
			}

			ImGui::InputText("Name", m_NameEditBuffer, sizeof(m_NameEditBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			if (ImGui::IsItemDeactivatedAfterEdit() || (ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Enter)))
			{
				const std::string newName = m_NameEditBuffer;
				if (!TryRenameNode(node, newName))
				{
					BZ_INFO("Rename cancelled: name already used or empty");
					std::snprintf(m_NameEditBuffer, sizeof(m_NameEditBuffer), "%s", node->name.c_str());
				}
				else
				{
					m_NameEditTarget = node->name;
				}
			}
			else if (ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				std::snprintf(m_NameEditBuffer, sizeof(m_NameEditBuffer), "%s", node->name.c_str());
			}

			bool visible = node->visible;
			if (ImGui::Checkbox("Visible", &visible))
			{
				node->visible = visible;
				m_Dirty = true;
			}
		}

		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			m_AnchorPadDrag = (node->type == WidgetType::Canvas) ? 0 : m_AnchorPadDrag;

			if (ImGui::InputFloat2("Pivot", &node->pivot.x, "%.2f"))
			{
				node->pivot.x = glm::clamp(node->pivot.x, 0.0f, 1.0f);
				node->pivot.y = glm::clamp(node->pivot.y, 0.0f, 1.0f);
				m_Dirty = true;
			}
			if (ImGui::InputFloat2("Position", &node->position.x, "%.1f"))
				m_Dirty = true;
			if (node->type != WidgetType::Canvas)
			{
				if (ImGui::InputFloat2("Size", &node->size.x, "%.0f"))
					m_Dirty = true;
				ImGui::TextDisabled("Anchors: edit in Preview (presets + drag handles)");
			}
			else
			{
				glm::vec2 design = m_Asset.GetDesignSize();
				if (ImGui::InputFloat2("Design Size", &design.x, "%.0f"))
				{
					m_Asset.SetDesignSize(design.x, design.y);
					m_Dirty = true;
				}
				ImGui::TextDisabled("Game View canvas size");
			}
			if (ImGui::ColorEdit4("Color", &node->color.x))
				m_Dirty = true;
		}

		if ((node->type == WidgetType::Text || node->type == WidgetType::Button)
			&& ImGui::CollapsingHeader(node->type == WidgetType::Text ? "Text" : "Button", ImGuiTreeNodeFlags_DefaultOpen))
		{
			char textBuf[256] = {};
			std::snprintf(textBuf, sizeof(textBuf), "%s", node->text.c_str());
			if (ImGui::InputText("Label", textBuf, sizeof(textBuf)))
			{
				node->text = textBuf;
				m_Dirty = true;
			}
			if (ImGui::InputFloat("Font Size", &node->fontSize, 1.0f, 4.0f, "%.0f"))
			{
				node->fontSize = glm::max(node->fontSize, 1.0f);
				m_Dirty = true;
			}

			ImGui::Text("Font");
			if (node->fontPath.empty())
				ImGui::TextDisabled("Default (Arial)");
			else
			{
				const std::string fileName = fs::path(node->fontPath).filename().string();
				ImGui::TextUnformatted(fileName.c_str());
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", node->fontPath.c_str());
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BLAZE_FONT"))
				{
					node->fontPath = std::string((const char*)payload->Data);
					m_Dirty = true;
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::TextDisabled("Drag from Content Browser or Select...");
			if (ImGui::Button("Select Font...", ImVec2(-1.0f, 0.0f)))
				ImGui::OpenPopup("WidgetFontPicker");

			if (ImGui::BeginPopup("WidgetFontPicker"))
			{
				ImGui::TextUnformatted("Project fonts (.ttf / .otf)");
				ImGui::Separator();

				std::vector<std::string> fonts;
				auto collectFonts = [&](const fs::path& root)
				{
					if (!fs::exists(root))
						return;
					std::error_code ec;
					for (auto it = fs::recursive_directory_iterator(root, ec);
						it != fs::recursive_directory_iterator(); ++it)
					{
						if (ec || !it->is_regular_file())
							continue;
						std::string ext = it->path().extension().string();
						for (char& c : ext)
							c = (char)std::tolower((unsigned char)c);
						if (ext == ".ttf" || ext == ".otf")
							fonts.push_back(it->path().generic_string());
					}
				};

				const std::string contentRoot = Project::HasProject()
					? Project::GetContentDirectory()
					: "content";
				collectFonts(contentRoot);
				if (contentRoot != "content")
					collectFonts("content");

				if (fonts.empty())
				{
					ImGui::TextDisabled("No fonts found. Import via Content Browser.");
				}
				else
				{
					ImGui::BeginChild("WidgetFontPickerList", ImVec2(320.0f, 240.0f), true);
					for (const std::string& path : fonts)
					{
						ImGui::PushID(path.c_str());
						const std::string label = fs::path(path).filename().string();
						bool selected = (path == node->fontPath);
						if (!selected && !node->fontPath.empty())
						{
							std::error_code ec;
							selected = fs::equivalent(path, node->fontPath, ec);
						}
						if (ImGui::Selectable(label.c_str(), selected))
						{
							node->fontPath = path;
							m_Dirty = true;
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
					node->fontPath.clear();
					m_Dirty = true;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			if (!node->fontPath.empty() && ImGui::Button("Clear Font", ImVec2(-1.0f, 0.0f)))
			{
				node->fontPath.clear();
				m_Dirty = true;
			}
		}

		if (node->type == WidgetType::Image
			&& ImGui::CollapsingHeader("Image", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawImageTextureField(*node);
		}

		if (node->name != m_Asset.GetRoot().name)
		{
			ImGui::Separator();
			if (ImGui::Button("Remove Element", ImVec2(-1.0f, 0.0f)))
				DeleteSelected();
		}
	}

	void WidgetEditorWindow::DrawImageTextureField(WidgetNode& node)
	{
		ImGui::Text("Texture");

		const float previewSize = 64.0f;
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		Texture* thumb = node.texturePath.empty() ? nullptr : GetThumbnail(node.texturePath);

		if (thumb && thumb->IsValid())
		{
			ImGui::Image((ImTextureID)(intptr_t)thumb->GetTextID(), ImVec2(previewSize, previewSize));
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
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BLAZE_TEXTURE"))
			{
				node.texturePath = std::string((const char*)payload->Data);
				m_Dirty = true;
				BZ_INFO("Assigned widget texture: {0}", node.texturePath);
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();
		ImGui::BeginGroup();
		if (node.texturePath.empty())
			ImGui::TextDisabled("No texture");
		else
			ImGui::TextWrapped("%s", node.texturePath.c_str());

		ImGui::TextDisabled("Drag from Content Browser");
		if (ImGui::Button("Select..."))
			ImGui::OpenPopup("WidgetTexturePicker");

		if (ImGui::BeginPopup("WidgetTexturePicker"))
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
				ImGui::BeginChild("WidgetTexturePickerList", ImVec2(280.0f, 240.0f), true);

				int columns = (int)(ImGui::GetContentRegionAvail().x / cellSize);
				if (columns < 1)
					columns = 1;
				ImGui::Columns(columns, nullptr, false);

				for (const std::string& texturePath : textures)
				{
					ImGui::PushID(texturePath.c_str());
					Texture* thumbnail = GetThumbnail(texturePath);
					bool selected = (texturePath == node.texturePath);
					if (!selected && !node.texturePath.empty())
					{
						std::error_code ec;
						selected = fs::equivalent(texturePath, node.texturePath, ec);
					}

					ImVec4 tint = selected ? ImVec4(0.6f, 0.85f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);
					if (ImGui::ImageButton("##pick", (ImTextureID)(intptr_t)thumbnail->GetTextID(),
						ImVec2(thumbSize, thumbSize), ImVec2(0, 0), ImVec2(1, 1),
						ImVec4(0, 0, 0, 0), tint))
					{
						node.texturePath = texturePath;
						m_Dirty = true;
						ImGui::CloseCurrentPopup();
					}
					ImGui::NextColumn();
					ImGui::PopID();
				}
				ImGui::Columns(1);
				ImGui::EndChild();
			}
			ImGui::EndPopup();
		}

		if (!node.texturePath.empty() && ImGui::Button("Clear"))
		{
			node.texturePath.clear();
			m_Dirty = true;
		}
		ImGui::EndGroup();
	}

	void WidgetEditorWindow::DrawComponentsPanel()
	{
		ImGui::BeginChild("##WidgetComponents", ImVec2(320.0f, 0.0f), true,
			ImGuiWindowFlags_AlwaysUseWindowPadding);
		ImGui::TextUnformatted("Components");
		ImGui::Separator();
		DrawInspector();
		ImGui::EndChild();
	}

	void WidgetEditorWindow::DrawToolbar()
	{
		if (ImGui::Button("Save"))
			Save();
		ImGui::SameLine();
		if (ImGui::Button("+ Text"))
			AddChild(WidgetType::Text);
		ImGui::SameLine();
		if (ImGui::Button("+ Button"))
			AddChild(WidgetType::Button);
		ImGui::SameLine();
		if (ImGui::Button("+ Image"))
			AddChild(WidgetType::Image);
		ImGui::SameLine();
		{
			const bool canDelete = m_SelectedName != m_Asset.GetRoot().name;
			if (!canDelete)
				ImGui::BeginDisabled();
			if (ImGui::Button("Delete") && canDelete)
				DeleteSelected();
			if (!canDelete)
				ImGui::EndDisabled();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%s%s", m_Asset.GetPath().c_str(), m_Dirty ? " *" : "");
	}

	void WidgetEditorWindow::CollectPreviewHits(const WidgetNode& node, const UIRect& parent, std::vector<PreviewHit>& hits) const
	{
		if (!node.visible)
			return;
		const UIRect rect = UISystem::ResolveRect(node, parent);
		if (node.type != WidgetType::Canvas)
			hits.push_back({ node.name, rect, parent });
		for (const WidgetNode& child : node.children)
			CollectPreviewHits(child, rect, hits);
	}

	WidgetDragMode WidgetEditorWindow::HitTestHandle(const UIRect& rect, float mx, float my, float handlePad) const
	{
		const float x0 = rect.x;
		const float y0 = rect.y;
		const float x1 = rect.x + rect.w;
		const float y1 = rect.y + rect.h;
		const float cx = (x0 + x1) * 0.5f;
		const float cy = (y0 + y1) * 0.5f;
		const float p = handlePad;

		auto nearPt = [&](float x, float y) {
			return std::abs(mx - x) <= p && std::abs(my - y) <= p;
		};

		if (nearPt(x0, y0)) return WidgetDragMode::ResizeNW;
		if (nearPt(x1, y0)) return WidgetDragMode::ResizeNE;
		if (nearPt(x0, y1)) return WidgetDragMode::ResizeSW;
		if (nearPt(x1, y1)) return WidgetDragMode::ResizeSE;
		if (nearPt(cx, y0)) return WidgetDragMode::ResizeN;
		if (nearPt(cx, y1)) return WidgetDragMode::ResizeS;
		if (nearPt(x0, cy)) return WidgetDragMode::ResizeW;
		if (nearPt(x1, cy)) return WidgetDragMode::ResizeE;
		return WidgetDragMode::None;
	}

	bool WidgetEditorWindow::GetNodePreviewRects(const std::string& name, UIRect& outRect, UIRect& outParent) const
	{
		float srcW = m_Asset.GetDesignSize().x;
		float srcH = m_Asset.GetDesignSize().y;
		if (srcW < 64.0f) srcW = 1280.0f;
		if (srcH < 64.0f) srcH = 720.0f;
		const UIRect screen{ 0.0f, 0.0f, srcW, srcH };
		std::vector<PreviewHit> hits;
		CollectPreviewHits(m_Asset.GetRoot(), screen, hits);
		for (const PreviewHit& hit : hits)
		{
			if (hit.name == name)
			{
				outRect = hit.rect;
				outParent = hit.parentRect;
				return true;
			}
		}
		return false;
	}

	void WidgetEditorWindow::ApplyAnchorPreset(WidgetNode& node, const glm::vec2& amin, const glm::vec2& amax, const glm::vec2& pivot)
	{
		UIRect rect{};
		UIRect parent{};
		if (!GetNodePreviewRects(node.name, rect, parent) || parent.w < 1.0f || parent.h < 1.0f)
		{
			node.anchorMin = amin;
			node.anchorMax = amax;
			node.pivot = pivot;
			node.position = glm::vec2(0.0f);
			m_Dirty = true;
			return;
		}

		node.anchorMin = amin;
		node.anchorMax = amax;
		node.pivot = pivot;

		const float ax0 = parent.x + amin.x * parent.w;
		const float ay0 = parent.y + amin.y * parent.h;
		const float ax1 = parent.x + amax.x * parent.w;
		const float ay1 = parent.y + amax.y * parent.h;
		const bool stretchX = std::abs(amax.x - amin.x) > 1e-4f;
		const bool stretchY = std::abs(amax.y - amin.y) > 1e-4f;

		if (!stretchX)
			node.size.x = glm::max(rect.w, 8.0f);
		if (!stretchY)
			node.size.y = glm::max(rect.h, 8.0f);

		if (stretchX)
			node.position.x = rect.x - ax0;
		else
			node.position.x = rect.x - ax0 + pivot.x * rect.w;

		if (stretchY)
			node.position.y = rect.y - ay0;
		else
			node.position.y = rect.y - ay0 + pivot.y * rect.h;

		(void)ax1;
		(void)ay1;
		m_Dirty = true;
	}

	void WidgetEditorWindow::DrawPreviewAnchorTools(WidgetNode& node, const UIRect& parentRect,
		float originX, float originY, float scale, bool canvasHovered, bool& consumedClick)
	{
		if (parentRect.w < 1.0f || parentRect.h < 1.0f || scale < 1e-6f)
			return;

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImGuiIO& io = ImGui::GetIO();

		auto toScreen = [&](float x, float y) {
			return ImVec2(originX + x * scale, originY + y * scale);
		};
		auto toLogical = [&](float sx, float sy) {
			return glm::vec2((sx - originX) / scale, (sy - originY) / scale);
		};

		const ImVec2 p0 = toScreen(parentRect.x, parentRect.y);
		const ImVec2 p1 = toScreen(parentRect.x + parentRect.w, parentRect.y + parentRect.h);
		dl->AddRect(p0, p1, IM_COL32(90, 170, 255, 100), 0.0f, 0, 1.5f);

		auto anchorToScreen = [&](const glm::vec2& a)
		{
			return toScreen(
				parentRect.x + a.x * parentRect.w,
				parentRect.y + a.y * parentRect.h);
		};
		auto screenToAnchor = [&](ImVec2 sp)
		{
			const glm::vec2 logical = toLogical(sp.x, sp.y);
			return glm::vec2(
				glm::clamp((logical.x - parentRect.x) / parentRect.w, 0.0f, 1.0f),
				glm::clamp((logical.y - parentRect.y) / parentRect.h, 0.0f, 1.0f));
		};

		struct PointPreset { float x, y; const char* tip; };
		const PointPreset points[3][3] = {
			{ { 0.0f, 0.0f, "Top Left" },    { 0.5f, 0.0f, "Top Center" },    { 1.0f, 0.0f, "Top Right" } },
			{ { 0.0f, 0.5f, "Middle Left" }, { 0.5f, 0.5f, "Center" },        { 1.0f, 0.5f, "Middle Right" } },
			{ { 0.0f, 1.0f, "Bottom Left" }, { 0.5f, 1.0f, "Bottom Center" }, { 1.0f, 1.0f, "Bottom Right" } },
		};

		auto nearAnchor = [](const glm::vec2& a, float x, float y)
		{
			return std::abs(a.x - x) < 0.02f && std::abs(a.y - y) < 0.02f;
		};

		const float presetR = 5.0f;
		const float presetHit = 10.0f;
		for (int row = 0; row < 3; ++row)
		{
			for (int col = 0; col < 3; ++col)
			{
				const PointPreset& pr = points[row][col];
				const ImVec2 c = anchorToScreen(glm::vec2(pr.x, pr.y));
				const bool selected = nearAnchor(node.anchorMin, pr.x, pr.y) && nearAnchor(node.anchorMax, pr.x, pr.y);
				dl->AddCircleFilled(c, presetR, selected ? IM_COL32(90, 170, 255, 255) : IM_COL32(70, 70, 80, 220));
				dl->AddCircle(c, presetR, IM_COL32(200, 200, 210, 255));

				const float dx = io.MousePos.x - c.x;
				const float dy = io.MousePos.y - c.y;
				if (canvasHovered && (dx * dx + dy * dy) <= presetHit * presetHit)
				{
					ImGui::SetTooltip("%s", pr.tip);
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						ApplyAnchorPreset(node, glm::vec2(pr.x, pr.y), glm::vec2(pr.x, pr.y), glm::vec2(pr.x, pr.y));
						consumedClick = true;
						m_AnchorPadDrag = 0;
					}
				}
			}
		}

		struct StretchPreset
		{
			float minX, minY, maxX, maxY;
			float pivotX, pivotY;
			const char* label;
		};
		const StretchPreset stretches[] = {
			{ 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, "H Top" },
			{ 0.0f, 0.5f, 1.0f, 0.5f, 0.5f, 0.5f, "H Mid" },
			{ 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 1.0f, "H Bot" },
			{ 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, "V L" },
			{ 0.5f, 0.0f, 0.5f, 1.0f, 0.5f, 0.5f, "V M" },
			{ 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, "V R" },
			{ 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, "Fill" },
		};

		float btnX = p0.x;
		const float btnY = p0.y - 22.0f;
		const float btnH = 18.0f;
		for (const StretchPreset& s : stretches)
		{
			const ImVec2 textSize = ImGui::CalcTextSize(s.label);
			const float btnW = textSize.x + 10.0f;
			const ImVec2 b0(btnX, btnY);
			const ImVec2 b1(btnX + btnW, btnY + btnH);
			const bool hover = canvasHovered
				&& io.MousePos.x >= b0.x && io.MousePos.x <= b1.x
				&& io.MousePos.y >= b0.y && io.MousePos.y <= b1.y;
			dl->AddRectFilled(b0, b1, hover ? IM_COL32(70, 110, 180, 230) : IM_COL32(40, 44, 54, 220), 3.0f);
			dl->AddText(ImVec2(b0.x + 5.0f, b0.y + 2.0f), IM_COL32(230, 230, 235, 255), s.label);
			if (hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				ApplyAnchorPreset(node,
					glm::vec2(s.minX, s.minY),
					glm::vec2(s.maxX, s.maxY),
					glm::vec2(s.pivotX, s.pivotY));
				consumedClick = true;
				m_AnchorPadDrag = 0;
			}
			btnX += btnW + 4.0f;
		}

		const ImVec2 minPt = anchorToScreen(node.anchorMin);
		const ImVec2 maxPt = anchorToScreen(node.anchorMax);
		const bool stretched = std::abs(node.anchorMax.x - node.anchorMin.x) > 1e-3f
			|| std::abs(node.anchorMax.y - node.anchorMin.y) > 1e-3f;

		if (stretched)
		{
			dl->AddRectFilled(
				ImVec2(std::min(minPt.x, maxPt.x), std::min(minPt.y, maxPt.y)),
				ImVec2(std::max(minPt.x, maxPt.x), std::max(minPt.y, maxPt.y)),
				IM_COL32(90, 170, 255, 35));
			dl->AddLine(minPt, maxPt, IM_COL32(200, 200, 210, 180), 1.5f);
		}

		const float handleR = 7.0f;
		dl->AddCircleFilled(minPt, handleR, IM_COL32(80, 220, 120, 255));
		dl->AddCircle(minPt, handleR, IM_COL32(20, 20, 20, 255));
		if (stretched)
		{
			dl->AddCircleFilled(maxPt, handleR, IM_COL32(255, 170, 70, 255));
			dl->AddCircle(maxPt, handleR, IM_COL32(20, 20, 20, 255));
		}
		else
		{
			dl->AddCircleFilled(minPt, 2.5f, IM_COL32(255, 255, 255, 255));
		}

		auto dist2 = [](ImVec2 a, ImVec2 b)
		{
			const float dx = a.x - b.x;
			const float dy = a.y - b.y;
			return dx * dx + dy * dy;
		};
		const float hitR2 = (handleR + 5.0f) * (handleR + 5.0f);

		if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !consumedClick)
		{
			if (dist2(io.MousePos, minPt) <= hitR2)
			{
				m_AnchorPadDrag = 1;
				consumedClick = true;
			}
			else if (stretched && dist2(io.MousePos, maxPt) <= hitR2)
			{
				m_AnchorPadDrag = 2;
				consumedClick = true;
			}
		}

		if (m_AnchorPadDrag != 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			consumedClick = true;
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			glm::vec2 p = screenToAnchor(io.MousePos);
			if (m_AnchorPadDrag == 1)
			{
				if (!stretched)
				{
					ApplyAnchorPreset(node, p, p, p);
				}
				else
				{
					glm::vec2 amin = p;
					glm::vec2 amax = node.anchorMax;
					if (amin.x > amax.x) std::swap(amin.x, amax.x);
					if (amin.y > amax.y) std::swap(amin.y, amax.y);
					ApplyAnchorPreset(node, amin, amax, node.pivot);
				}
			}
			else if (m_AnchorPadDrag == 2)
			{
				glm::vec2 amin = node.anchorMin;
				glm::vec2 amax = p;
				if (amin.x > amax.x) std::swap(amin.x, amax.x);
				if (amin.y > amax.y) std::swap(amin.y, amax.y);
				ApplyAnchorPreset(node, amin, amax, node.pivot);
			}
		}
		else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			m_AnchorPadDrag = 0;
		}
	}

	void WidgetEditorWindow::EnsurePointAnchors(WidgetNode& node, const UIRect& parentRect, const UIRect& rect)
	{
		const bool stretchX = std::abs(node.anchorMax.x - node.anchorMin.x) > 1e-4f;
		const bool stretchY = std::abs(node.anchorMax.y - node.anchorMin.y) > 1e-4f;
		if (!stretchX && !stretchY)
			return;
		if (parentRect.w < 1.0f || parentRect.h < 1.0f)
			return;

		const float cx = (rect.x + rect.w * 0.5f - parentRect.x) / parentRect.w;
		const float cy = (rect.y + rect.h * 0.5f - parentRect.y) / parentRect.h;
		node.anchorMin = node.anchorMax = glm::vec2(glm::clamp(cx, 0.0f, 1.0f), glm::clamp(cy, 0.0f, 1.0f));
		node.pivot = glm::vec2(0.5f);
		node.size = glm::vec2(glm::max(rect.w, 8.0f), glm::max(rect.h, 8.0f));
		node.position = glm::vec2(0.0f);
	}

	void WidgetEditorWindow::ApplyDrag(WidgetNode& node, const UIRect& parentRect, float logicalX, float logicalY)
	{
		const float minSize = 8.0f;
		const float dx = logicalX - m_DragStartMouse.x;
		const float dy = logicalY - m_DragStartMouse.y;

		if (m_DragMode == WidgetDragMode::Move)
		{
			node.position = m_DragStartPos + glm::vec2(dx, dy);
			m_Dirty = true;
			return;
		}

		float left = m_DragStartRect.x;
		float top = m_DragStartRect.y;
		float right = m_DragStartRect.x + m_DragStartRect.w;
		float bottom = m_DragStartRect.y + m_DragStartRect.h;

		switch (m_DragMode)
		{
			case WidgetDragMode::ResizeNW:
				left = m_DragStartRect.x + dx;
				top = m_DragStartRect.y + dy;
				break;
			case WidgetDragMode::ResizeN:
				top = m_DragStartRect.y + dy;
				break;
			case WidgetDragMode::ResizeNE:
				right = m_DragStartRect.x + m_DragStartRect.w + dx;
				top = m_DragStartRect.y + dy;
				break;
			case WidgetDragMode::ResizeW:
				left = m_DragStartRect.x + dx;
				break;
			case WidgetDragMode::ResizeE:
				right = m_DragStartRect.x + m_DragStartRect.w + dx;
				break;
			case WidgetDragMode::ResizeSW:
				left = m_DragStartRect.x + dx;
				bottom = m_DragStartRect.y + m_DragStartRect.h + dy;
				break;
			case WidgetDragMode::ResizeS:
				bottom = m_DragStartRect.y + m_DragStartRect.h + dy;
				break;
			case WidgetDragMode::ResizeSE:
				right = m_DragStartRect.x + m_DragStartRect.w + dx;
				bottom = m_DragStartRect.y + m_DragStartRect.h + dy;
				break;
			default:
				return;
		}

		if (right - left < minSize)
		{
			if (m_DragMode == WidgetDragMode::ResizeNW || m_DragMode == WidgetDragMode::ResizeW || m_DragMode == WidgetDragMode::ResizeSW)
				left = right - minSize;
			else
				right = left + minSize;
		}
		if (bottom - top < minSize)
		{
			if (m_DragMode == WidgetDragMode::ResizeNW || m_DragMode == WidgetDragMode::ResizeN || m_DragMode == WidgetDragMode::ResizeNE)
				top = bottom - minSize;
			else
				bottom = top + minSize;
		}

		const float w = right - left;
		const float h = bottom - top;

		const float ax0 = parentRect.x + node.anchorMin.x * parentRect.w;
		const float ay0 = parentRect.y + node.anchorMin.y * parentRect.h;
		node.size = glm::vec2(w, h);
		node.position.x = left - ax0 + node.pivot.x * w;
		node.position.y = top - ay0 + node.pivot.y * h;
		m_Dirty = true;
	}

	void WidgetEditorWindow::DrawPreview()
	{
		const float componentsW = 320.0f;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		float previewW = ImGui::GetContentRegionAvail().x - componentsW - spacing;
		if (previewW < 200.0f)
			previewW = 200.0f;

		ImGui::BeginChild("##WidgetPreview", ImVec2(previewW, 0.0f), true);
		ImGui::TextUnformatted("Preview");
		ImGui::SameLine();
		{
			glm::vec2 design = m_Asset.GetDesignSize();
			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::DragFloat2("##DesignSize", &design.x, 1.0f, 64.0f, 7680.0f, "%.0f"))
			{
				m_Asset.SetDesignSize(design.x, design.y);
				m_Dirty = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Game View size (width x height)");
		}
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(8.0f, 0.0f));
		ImGui::SameLine();
		if (ImGui::SmallButton("-##ZoomOut"))
			m_PreviewZoom = glm::clamp(m_PreviewZoom / 1.15f, 0.1f, 8.0f);
		ImGui::SameLine();
		{
			char zoomLabel[32];
			std::snprintf(zoomLabel, sizeof(zoomLabel), "%.0f%%", m_PreviewZoom * 100.0f);
			ImGui::SetNextItemWidth(64.0f);
			float zoomPct = m_PreviewZoom * 100.0f;
			if (ImGui::DragFloat("##ZoomPct", &zoomPct, 1.0f, 10.0f, 800.0f, zoomLabel))
				m_PreviewZoom = glm::clamp(zoomPct / 100.0f, 0.1f, 8.0f);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Preview zoom");
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("+##ZoomIn"))
			m_PreviewZoom = glm::clamp(m_PreviewZoom * 1.15f, 0.1f, 8.0f);
		ImGui::SameLine();
		if (ImGui::SmallButton("Fit##ZoomFit"))
		{
			m_PreviewZoom = 1.0f;
			m_PreviewPan = glm::vec2(0.0f);
		}
		ImGui::TextDisabled("Wheel: zoom  |  RMB: pan  |  select widget to set anchors in preview");
		ImGui::Separator();

		const ImVec2 avail = ImGui::GetContentRegionAvail();
		ImGui::InvisibleButton("##WidgetPreviewCanvas", avail);
		const bool canvasHovered = ImGui::IsItemHovered();
		const bool canvasActive = ImGui::IsItemActive();
		const ImVec2 origin = ImGui::GetItemRectMin();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImGuiIO& io = ImGui::GetIO();

		float srcW = m_Asset.GetDesignSize().x;
		float srcH = m_Asset.GetDesignSize().y;
		if (srcW < 64.0f) srcW = 1280.0f;
		if (srcH < 64.0f) srcH = 720.0f;

		const float pad = 24.0f;
		const float fitW = glm::max(avail.x - pad * 2.0f, 32.0f);
		const float fitH = glm::max(avail.y - pad * 2.0f, 32.0f);
		const float fitScale = glm::min(fitW / srcW, fitH / srcH);

		if (canvasHovered && io.MouseWheel != 0.0f && !m_ResizingDesign && m_DragMode == WidgetDragMode::None)
		{
			const float oldZoom = m_PreviewZoom;
			const float factor = (io.MouseWheel > 0.0f) ? 1.12f : (1.0f / 1.12f);
			m_PreviewZoom = glm::clamp(oldZoom * factor, 0.1f, 8.0f);
			const float oldScale = fitScale * oldZoom;
			const float newScale = fitScale * m_PreviewZoom;
			if (oldScale > 1e-4f && newScale > 1e-4f)
			{
				const float oldOx = origin.x + (avail.x - srcW * oldScale) * 0.5f + m_PreviewPan.x;
				const float oldOy = origin.y + (avail.y - srcH * oldScale) * 0.5f + m_PreviewPan.y;
				const float lx = (io.MousePos.x - oldOx) / oldScale;
				const float ly = (io.MousePos.y - oldOy) / oldScale;
				const float newOx = io.MousePos.x - lx * newScale;
				const float newOy = io.MousePos.y - ly * newScale;
				m_PreviewPan.x = newOx - (origin.x + (avail.x - srcW * newScale) * 0.5f);
				m_PreviewPan.y = newOy - (origin.y + (avail.y - srcH * newScale) * 0.5f);
			}
		}

		const float scale = fitScale * m_PreviewZoom;
		const float ox = origin.x + (avail.x - srcW * scale) * 0.5f + m_PreviewPan.x;
		const float oy = origin.y + (avail.y - srcH * scale) * 0.5f + m_PreviewPan.y;
		const ImVec2 gameMin(ox, oy);
		const ImVec2 gameMax(ox + srcW * scale, oy + srcH * scale);

		ImGui::PushClipRect(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), true);

		// Outside the game view (letterbox / unused editor space)
		dl->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(18, 18, 20, 255));

		// Game view range — what players see in the Game window
		dl->AddRectFilled(gameMin, gameMax, IM_COL32(32, 32, 38, 255));
		{
			const float step = 100.0f * scale;
			if (step >= 8.0f)
			{
				for (float x = gameMin.x + step; x < gameMax.x; x += step)
					dl->AddLine(ImVec2(x, gameMin.y), ImVec2(x, gameMax.y), IM_COL32(45, 45, 52, 255));
				for (float y = gameMin.y + step; y < gameMax.y; y += step)
					dl->AddLine(ImVec2(gameMin.x, y), ImVec2(gameMax.x, y), IM_COL32(45, 45, 52, 255));
			}
		}
		dl->AddRect(gameMin, gameMax, IM_COL32(90, 170, 255, 220), 0.0f, 0, 2.0f);
		const float tick = 10.0f;
		const ImU32 tickCol = IM_COL32(90, 170, 255, 255);
		dl->AddLine(gameMin, ImVec2(gameMin.x + tick, gameMin.y), tickCol, 2.0f);
		dl->AddLine(gameMin, ImVec2(gameMin.x, gameMin.y + tick), tickCol, 2.0f);
		dl->AddLine(ImVec2(gameMax.x, gameMin.y), ImVec2(gameMax.x - tick, gameMin.y), tickCol, 2.0f);
		dl->AddLine(ImVec2(gameMax.x, gameMin.y), ImVec2(gameMax.x, gameMin.y + tick), tickCol, 2.0f);
		dl->AddLine(ImVec2(gameMin.x, gameMax.y), ImVec2(gameMin.x + tick, gameMax.y), tickCol, 2.0f);
		dl->AddLine(ImVec2(gameMin.x, gameMax.y), ImVec2(gameMin.x, gameMax.y - tick), tickCol, 2.0f);
		dl->AddLine(gameMax, ImVec2(gameMax.x - tick, gameMax.y), tickCol, 2.0f);
		dl->AddLine(gameMax, ImVec2(gameMax.x, gameMax.y - tick), tickCol, 2.0f);

		// Design-size resize handle (bottom-right of Game View)
		const float designHandle = 10.0f;
		const ImVec2 dh0(gameMax.x - designHandle * 0.5f, gameMax.y - designHandle * 0.5f);
		const ImVec2 dh1(gameMax.x + designHandle * 0.5f, gameMax.y + designHandle * 0.5f);
		dl->AddRectFilled(dh0, dh1, IM_COL32(90, 170, 255, 255));
		dl->AddRect(dh0, dh1, IM_COL32(20, 20, 20, 255));

		char rangeLabel[64];
		std::snprintf(rangeLabel, sizeof(rangeLabel), "Game View  %.0fx%.0f", srcW, srcH);
		dl->AddText(ImVec2(gameMin.x + 6.0f, gameMin.y - 18.0f), IM_COL32(160, 200, 255, 255), rangeLabel);

		auto toScreen = [&](float x, float y) {
			return ImVec2(ox + x * scale, oy + y * scale);
		};
		auto toLogical = [&](float sx, float sy) {
			return glm::vec2((sx - ox) / scale, (sy - oy) / scale);
		};

		auto drawFontText = [&](const std::string& text, float logicalX, float logicalY,
			float fontSize, ImU32 col, const std::string& fontPath)
		{
			if (text.empty())
				return;

			unsigned int tex = 0;
			float bakeSize = 32.0f;
			const UISystem::FontGlyph* glyphs = nullptr;
			if (!UISystem::ResolveFont(fontPath, tex, bakeSize, glyphs) || !tex || !glyphs)
			{
				dl->AddText(toScreen(logicalX, logicalY), col, text.c_str());
				return;
			}

			const float logicalScale = fontSize / bakeSize;
			float cursor = logicalX;
			for (unsigned char ch : text)
			{
				if (ch < 32 || ch > 127)
					ch = '?';
				const UISystem::FontGlyph& g = glyphs[ch - 32];
				const ImVec2 p0 = toScreen(cursor + g.x0 * logicalScale, logicalY + g.y0 * logicalScale);
				const ImVec2 p1 = toScreen(cursor + g.x1 * logicalScale, logicalY + g.y1 * logicalScale);
				if (p1.x > p0.x && p1.y > p0.y)
				{
					dl->AddImage((ImTextureID)(intptr_t)tex, p0, p1,
						ImVec2(g.u0, g.v0), ImVec2(g.u1, g.v1), col);
				}
				cursor += g.advance * logicalScale;
			}
		};

		const UIRect screen{ 0.0f, 0.0f, srcW, srcH };

		std::function<void(const WidgetNode&, UIRect)> drawPreview =
			[&](const WidgetNode& node, UIRect parent)
		{
			if (!node.visible)
				return;
			UIRect rect = UISystem::ResolveRect(node, parent);

			if (node.type == WidgetType::Button)
			{
				ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(node.color.r, node.color.g, node.color.b, node.color.a));
				dl->AddRectFilled(toScreen(rect.x, rect.y), toScreen(rect.x + rect.w, rect.y + rect.h), col, 4.0f);
				if (!node.text.empty())
				{
					const float tw = UISystem::MeasureTextWidth(node.text, node.fontSize, node.fontPath);
					const float tx = rect.x + (rect.w - tw) * 0.5f;
					const float ty = rect.y + rect.h * 0.5f + node.fontSize * 0.35f;
					drawFontText(node.text, tx, ty, node.fontSize, IM_COL32(255, 255, 255, 255), node.fontPath);
				}
			}
			else if (node.type == WidgetType::Text)
			{
				ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(node.color.r, node.color.g, node.color.b, node.color.a));
				drawFontText(node.text, rect.x, rect.y + node.fontSize, node.fontSize, col, node.fontPath);
			}
			else if (node.type == WidgetType::Image)
			{
				const ImVec2 p0 = toScreen(rect.x, rect.y);
				const ImVec2 p1 = toScreen(rect.x + rect.w, rect.y + rect.h);
				Texture* thumb = node.texturePath.empty() ? nullptr : GetThumbnail(node.texturePath);
				if (thumb && thumb->IsValid())
				{
					const ImU32 tint = ImGui::ColorConvertFloat4ToU32(
						ImVec4(node.color.r, node.color.g, node.color.b, node.color.a));
					dl->AddImage((ImTextureID)(intptr_t)thumb->GetTextID(), p0, p1,
						ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint);
				}
				else
				{
					dl->AddRectFilled(p0, p1, IM_COL32(60, 40, 60, 255));
					dl->AddRect(p0, p1, IM_COL32(200, 200, 200, 180));
					dl->AddText(ImVec2(p0.x + 4.0f, p0.y + 4.0f), IM_COL32(220, 220, 220, 255), "Image");
				}
			}

			for (const WidgetNode& child : node.children)
				drawPreview(child, rect);
		};

		ImGui::PushClipRect(gameMin, gameMax, true);
		drawPreview(m_Asset.GetRoot(), screen);
		ImGui::PopClipRect();

		std::vector<PreviewHit> hits;
		CollectPreviewHits(m_Asset.GetRoot(), screen, hits);

		UIRect selectedRect{};
		UIRect selectedParent{};
		bool hasSelected = false;
		for (const PreviewHit& hit : hits)
		{
			if (hit.name == m_SelectedName)
			{
				selectedRect = hit.rect;
				selectedParent = hit.parentRect;
				hasSelected = true;
			}
		}

		const float handleScreen = 7.0f;
		const float handleLogical = (scale > 1e-4f) ? (handleScreen / scale) : handleScreen;
		const glm::vec2 mouseLogical = toLogical(io.MousePos.x, io.MousePos.y);

		WidgetDragMode hoverHandle = WidgetDragMode::None;
		if (hasSelected)
			hoverHandle = HitTestHandle(selectedRect, mouseLogical.x, mouseLogical.y, handleLogical);

		const bool overDesignHandle =
			io.MousePos.x >= dh0.x && io.MousePos.x <= dh1.x &&
			io.MousePos.y >= dh0.y && io.MousePos.y <= dh1.y;

		const bool panHeld =
			ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
			ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
			(ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.KeyAlt);
		const bool panClicked =
			ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
			ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
			(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyAlt);

		if (canvasHovered &&
			m_DragMode == WidgetDragMode::None &&
			!m_ResizingDesign &&
			panClicked)
		{
			m_PanningPreview = true;
			m_PanStartMouse = glm::vec2(io.MousePos.x, io.MousePos.y);
			m_PanStartOffset = m_PreviewPan;
		}

		if (m_PanningPreview)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
			if (panHeld)
			{
				m_PreviewPan = m_PanStartOffset +
					glm::vec2(io.MousePos.x - m_PanStartMouse.x, io.MousePos.y - m_PanStartMouse.y);
			}
			else
			{
				m_PanningPreview = false;
			}
		}
		else if (canvasHovered && !io.WantTextInput &&
			m_DragMode == WidgetDragMode::None && !m_ResizingDesign)
		{
			const float nudge = 12.0f;
			if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
				m_PreviewPan.x += nudge;
			if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
				m_PreviewPan.x -= nudge;
			if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
				m_PreviewPan.y += nudge;
			if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
				m_PreviewPan.y -= nudge;
		}

		bool anchorConsumed = false;
		WidgetNode* selectedNode = FindSelected();
		if (selectedNode && selectedNode->type != WidgetType::Canvas && hasSelected)
		{
			DrawPreviewAnchorTools(*selectedNode, selectedParent, ox, oy, scale, canvasHovered, anchorConsumed);
		}
		else if (m_AnchorPadDrag != 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			m_AnchorPadDrag = 0;
		}

		if (anchorConsumed || m_AnchorPadDrag != 0)
		{
			m_DragMode = WidgetDragMode::None;
			m_DragNodeName.clear();
			m_ResizingDesign = false;
		}

		auto beginDrag = [&](WidgetNode& node, WidgetDragMode mode, const UIRect& rect, const UIRect& parent)
		{
			EnsurePointAnchors(node, parent, rect);
			const UIRect fixed = UISystem::ResolveRect(node, parent);
			m_DragMode = mode;
			m_DragNodeName = node.name;
			m_DragStartMouse = mouseLogical;
			m_DragStartPos = node.position;
			m_DragStartSize = node.size;
			m_DragStartRect = fixed;
			m_DragParentRect = parent;
			m_SelectedName = node.name;
			m_ResizingDesign = false;
			m_PanningPreview = false;
			m_AnchorPadDrag = 0;
		};

		if (m_ResizingDesign)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				const float dx = (io.MousePos.x - m_DesignResizeStart.x) / glm::max(scale, 1e-4f);
				const float dy = (io.MousePos.y - m_DesignResizeStart.y) / glm::max(scale, 1e-4f);
				m_Asset.SetDesignSize(m_DesignResizeStartSize.x + dx, m_DesignResizeStartSize.y + dy);
				m_Dirty = true;
			}
			else
			{
				m_ResizingDesign = false;
			}
		}
		else if (!anchorConsumed && m_AnchorPadDrag == 0 && !m_PanningPreview && m_DragMode == WidgetDragMode::None && canvasHovered)
		{
			if (overDesignHandle)
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
			}
			else switch (hoverHandle)
			{
				case WidgetDragMode::ResizeNW:
				case WidgetDragMode::ResizeSE:
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
					break;
				case WidgetDragMode::ResizeNE:
				case WidgetDragMode::ResizeSW:
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
					break;
				case WidgetDragMode::ResizeN:
				case WidgetDragMode::ResizeS:
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
					break;
				case WidgetDragMode::ResizeW:
				case WidgetDragMode::ResizeE:
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
					break;
				default:
				{
					for (auto it = hits.rbegin(); it != hits.rend(); ++it)
					{
						if (it->rect.Contains(mouseLogical.x, mouseLogical.y))
						{
							ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
							break;
						}
					}
					break;
				}
			}
		}

		if (!anchorConsumed && m_AnchorPadDrag == 0 && !m_PanningPreview && !io.KeyAlt && canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			if (overDesignHandle)
			{
				m_ResizingDesign = true;
				m_DragMode = WidgetDragMode::None;
				m_DesignResizeStart = glm::vec2(io.MousePos.x, io.MousePos.y);
				m_DesignResizeStartSize = m_Asset.GetDesignSize();
				m_SelectedName = m_Asset.GetRoot().name;
			}
			else
			{
			WidgetNode* selected = FindSelected();
			if (selected && selected->type != WidgetType::Canvas && hoverHandle != WidgetDragMode::None)
			{
				beginDrag(*selected, hoverHandle, selectedRect, selectedParent);
			}
			else
			{
				std::string picked;
				UIRect pickedRect{};
				UIRect pickedParent{};
				for (auto it = hits.rbegin(); it != hits.rend(); ++it)
				{
					if (it->rect.Contains(mouseLogical.x, mouseLogical.y))
					{
						picked = it->name;
						pickedRect = it->rect;
						pickedParent = it->parentRect;
						break;
					}
				}

				if (!picked.empty())
				{
					WidgetNode* node = WidgetAsset::FindByName(m_Asset.GetRoot(), picked);
					if (node && node->type != WidgetType::Canvas)
						beginDrag(*node, WidgetDragMode::Move, pickedRect, pickedParent);
				}
				else
				{
					m_SelectedName = m_Asset.GetRoot().name;
					m_DragMode = WidgetDragMode::None;
				}
			}
			}
		}

		if (m_DragMode != WidgetDragMode::None && m_AnchorPadDrag == 0)
		{
			if (canvasActive || ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				WidgetNode* node = WidgetAsset::FindByName(m_Asset.GetRoot(), m_DragNodeName);
				if (node)
					ApplyDrag(*node, m_DragParentRect, mouseLogical.x, mouseLogical.y);
			}
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				m_DragMode = WidgetDragMode::None;
				m_DragNodeName.clear();
			}
		}

		hasSelected = false;
		hits.clear();
		CollectPreviewHits(m_Asset.GetRoot(), screen, hits);
		for (const PreviewHit& hit : hits)
		{
			if (hit.name == m_SelectedName)
			{
				selectedRect = hit.rect;
				hasSelected = true;
				break;
			}
		}

		if (hasSelected)
		{
			const ImVec2 p0 = toScreen(selectedRect.x, selectedRect.y);
			const ImVec2 p1 = toScreen(selectedRect.x + selectedRect.w, selectedRect.y + selectedRect.h);
			dl->AddRect(p0, p1, IM_COL32(255, 200, 60, 255), 0.0f, 0, 2.0f);

			const float hs = handleScreen;
			auto drawHandle = [&](float lx, float ly)
			{
				const ImVec2 c = toScreen(lx, ly);
				dl->AddRectFilled(ImVec2(c.x - hs * 0.5f, c.y - hs * 0.5f),
					ImVec2(c.x + hs * 0.5f, c.y + hs * 0.5f), IM_COL32(255, 200, 60, 255));
				dl->AddRect(ImVec2(c.x - hs * 0.5f, c.y - hs * 0.5f),
					ImVec2(c.x + hs * 0.5f, c.y + hs * 0.5f), IM_COL32(20, 20, 20, 255));
			};

			const float x0 = selectedRect.x;
			const float y0 = selectedRect.y;
			const float x1 = selectedRect.x + selectedRect.w;
			const float y1 = selectedRect.y + selectedRect.h;
			const float cx = (x0 + x1) * 0.5f;
			const float cy = (y0 + y1) * 0.5f;
			drawHandle(x0, y0);
			drawHandle(cx, y0);
			drawHandle(x1, y0);
			drawHandle(x0, cy);
			drawHandle(x1, cy);
			drawHandle(x0, y1);
			drawHandle(cx, y1);
			drawHandle(x1, y1);
		}

		ImGui::PopClipRect();
		ImGui::EndChild();
	}

	void WidgetEditorWindow::OnUpdate()
	{
		if (!m_Open)
			return;

		ImGui::SetNextWindowSize(ImVec2(1100.0f, 640.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Widget Editor", &m_Open))
		{
			ImGui::End();
			return;
		}
		m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		DrawToolbar();
		ImGui::Separator();

		DrawHierarchy();
		if (m_PendingDelete)
		{
			m_PendingDelete = false;
			DeleteSelected();
		}

		// Delete key removes the selected widget (not the root canvas).
		if (m_Focused
			&& m_RenamingName.empty()
			&& !ImGui::GetIO().WantTextInput
			&& ImGui::IsKeyPressed(ImGuiKey_Delete, false)
			&& m_SelectedName != m_Asset.GetRoot().name)
		{
			DeleteSelected();
		}

		ImGui::SameLine();
		DrawPreview();
		ImGui::SameLine();
		DrawComponentsPanel();

		ImGui::End();
	}
}
