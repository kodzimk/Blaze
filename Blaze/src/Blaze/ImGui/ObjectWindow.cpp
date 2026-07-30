#include "ObjectWindow.h"

#include "../vendor/imgui/src/imgui.h"   
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Gizmo.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Logging/Log.h"
#include <cstring>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace Blaze
{
	float ObjectWindow::height = 881.f;
	float ObjectWindow::width  = 300.f;

	static const char* s_SceneObjectPayload = "BLAZE_SCENE_OBJECT";

	HubAction ProjectHub::s_Action = HubAction::None;

	ProjectHub::ProjectHub()
	{
	}

	ProjectHub::~ProjectHub()
	{
	}

	HubAction ProjectHub::ConsumeAction()
	{
		HubAction action = s_Action;
		s_Action = HubAction::None;
		return action;
	}

	void ProjectHub::OnUpdate()
	{
		if (Project::HasProject())
			return;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->GetCenter().x, viewport->GetCenter().y),
			ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(640.0f, 460.0f), ImGuiCond_Always);

		ImGui::Begin("Blaze Projects", nullptr,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking);

		ImGui::TextUnformatted("Welcome to Blaze");
		ImGui::TextDisabled("Open one of your projects or create a new one");
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Create New Project...", ImVec2(200.0f, 34.0f)))
		{
			if (Project::NewProject())
				s_Action = HubAction::CreateProject;
		}
		ImGui::SameLine();
		if (ImGui::Button("Open From Disk...", ImVec2(200.0f, 34.0f)))
		{
			if (Project::OpenProjectDialog())
				s_Action = HubAction::LoadProject;
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("Recent Projects");
		ImGui::Separator();

		std::vector<std::string> projects = Project::GetRecentProjects();

		ImGui::BeginChild("HubProjectList", ImVec2(0, 0), true);
		if (projects.empty())
		{
			ImGui::TextDisabled("No projects yet. Create your first one!");
		}
		else
		{
			for (size_t i = 0; i < projects.size(); ++i)
			{
				const std::string& path = projects[i];
				std::string name = std::filesystem::path(path).stem().string();

				ImGui::PushID((int)i);
				bool openClicked = ImGui::Button("Open", ImVec2(60.0f, 0.0f));
				ImGui::SameLine();
				ImGui::BeginGroup();
				ImGui::TextUnformatted(name.c_str());
				ImGui::TextDisabled("%s", path.c_str());
				ImGui::EndGroup();
				if (openClicked || ImGui::IsItemClicked())
				{
					Project::SetProjectFromFile(path);
					s_Action = HubAction::LoadProject;
				}
				ImGui::PopID();
				ImGui::Separator();
			}
		}
		ImGui::EndChild();

		ImGui::End();
	}

	FileAction MenuBar::s_FileAction = FileAction::None;

	MenuBar::MenuBar()
		: onFocus(false)
	{
	}

	MenuBar::~MenuBar()
	{
	}

	FileAction MenuBar::ConsumeFileAction()
	{
		FileAction action = s_FileAction;
		s_FileAction = FileAction::None;
		return action;
	}

	void MenuBar::OnUpdate(bool* contentBrowserOpen, bool* objectsOpen, bool* gameObjectOpen, bool* componentsOpen, bool* toolbarOpen, bool* projectSettingsOpen)
	{
		if (!ImGui::BeginMainMenuBar())
			return;

		onFocus = ImGui::IsWindowFocused();

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Project..."))
				s_FileAction = FileAction::NewProject;
			if (ImGui::MenuItem("Open Project...", "Ctrl+O"))
				s_FileAction = FileAction::OpenProject;
			ImGui::Separator();
			if (ImGui::MenuItem("Save", "Ctrl+S"))
				s_FileAction = FileAction::Save;
			if (ImGui::MenuItem("Save As..."))
				s_FileAction = FileAction::SaveAs;
			ImGui::Separator();
			if (ImGui::MenuItem("Project Settings..."))
			{
				if (projectSettingsOpen)
					*projectSettingsOpen = true;
				s_FileAction = FileAction::ProjectSettings;
			}
			if (ImGui::MenuItem("Build Project..."))
				s_FileAction = FileAction::BuildProject;
			ImGui::Separator();
			if (ImGui::MenuItem("Close Project"))
				s_FileAction = FileAction::CloseProject;
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			if (contentBrowserOpen)
				ImGui::MenuItem("Content Browser", nullptr, contentBrowserOpen);
			if (objectsOpen)
				ImGui::MenuItem("Objects", nullptr, objectsOpen);
			if (gameObjectOpen)
				ImGui::MenuItem("GameObject", nullptr, gameObjectOpen);
			if (componentsOpen)
				ImGui::MenuItem("Components", nullptr, componentsOpen);
			if (toolbarOpen)
				ImGui::MenuItem("Toolbar", nullptr, toolbarOpen);
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	ObjectWindow::ObjectWindow()
		: onFocus(false), m_Selected(nullptr), m_PendingPick(nullptr), m_PendingDelete(nullptr),
		  m_PendingDeleteFolderId(-1), m_RenamingObject(nullptr), m_RenamingFolderId(-1)
	{
	}

	ObjectWindow::~ObjectWindow()
	{
	}

	void ObjectWindow::SyncHierarchy(const std::vector<GameObject*>& objects)
	{
		std::unordered_set<GameObject*> live(objects.begin(), objects.end());

		auto prune = [&](std::vector<GameObject*>& list)
		{
			list.erase(std::remove_if(list.begin(), list.end(),
				[&](GameObject* o) {
					return !o || live.find(o) == live.end() || o->GetParent() != nullptr;
				}), list.end());
		};

		prune(m_RootObjects);
		for (Folder& folder : m_Folders)
			prune(folder.objects);

		std::unordered_set<GameObject*> placed(m_RootObjects.begin(), m_RootObjects.end());
		for (const Folder& folder : m_Folders)
			placed.insert(folder.objects.begin(), folder.objects.end());

		for (GameObject* object : objects)
		{
			if (!object || object->GetParent())
				continue;
			if (placed.find(object) == placed.end())
				m_RootObjects.push_back(object);
		}
	}

	void ObjectWindow::SetObjects(const std::vector<GameObject*>& objects)
	{
		m_Objects = objects;
		SyncHierarchy(objects);
	}

	void ObjectWindow::SetSelectedObject(GameObject* object)
	{
		m_Selected = object;
		if (object)
			m_MapSelected = false;
	}

	void ObjectWindow::SetMapSelected(bool selected)
	{
		m_MapSelected = selected;
		if (selected)
		{
			m_Selected = nullptr;
			m_PendingPick = nullptr;
			m_PendingFocus = nullptr;
		}
	}

	void ObjectWindow::ClearHierarchy()
	{
		m_Folders.clear();
		m_RootObjects.clear();
		m_NextFolderId = 1;
		m_Selected = nullptr;
		m_PendingPick = nullptr;
		m_PendingDelete = nullptr;
		m_PendingFocus = nullptr;
		m_PendingDeleteFolderId = -1;
		m_MapSelected = false;
		m_RenamingObject = nullptr;
		m_RenamingFolderId = -1;
	}

	void ObjectWindow::SaveHierarchy(std::ostream& out) const
	{
		std::unordered_map<GameObject*, int> index;
		for (size_t i = 0; i < m_Objects.size(); ++i)
		{
			if (m_Objects[i])
				index[m_Objects[i]] = (int)i;
		}

		out << "hierarchy\n";
		for (const Folder& folder : m_Folders)
		{
			out << "folder " << folder.id << " " << (folder.open ? 1 : 0) << " " << folder.name << "\n";
			for (GameObject* object : folder.objects)
			{
				auto it = index.find(object);
				if (it != index.end())
					out << "obj " << it->second << "\n";
			}
			out << "endfolder\n";
		}

		out << "root\n";
		for (GameObject* object : m_RootObjects)
		{
			auto it = index.find(object);
			if (it != index.end())
				out << "obj " << it->second << "\n";
		}
		out << "endroot\n";
		out << "endhierarchy\n";
	}

	void ObjectWindow::LoadHierarchy(std::istream& in, const std::vector<GameObject*>& objects)
	{
		ClearHierarchy();
		m_Objects = objects;

		Folder* currentFolder = nullptr;
		bool inRoot = false;
		std::string line;
		while (std::getline(in, line))
		{
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
				line.pop_back();

			if (line == "endhierarchy")
				break;

			std::istringstream stream(line);
			std::string key;
			stream >> key;

			if (key == "folder")
			{
				Folder folder;
				int open = 1;
				stream >> folder.id >> open;
				folder.open = open != 0;
				std::getline(stream, folder.name);
				if (!folder.name.empty() && folder.name[0] == ' ')
					folder.name.erase(0, 1);
				m_Folders.push_back(folder);
				currentFolder = &m_Folders.back();
				inRoot = false;
				if (folder.id >= m_NextFolderId)
					m_NextFolderId = folder.id + 1;
			}
			else if (key == "endfolder")
			{
				currentFolder = nullptr;
			}
			else if (key == "root")
			{
				inRoot = true;
				currentFolder = nullptr;
			}
			else if (key == "endroot")
			{
				inRoot = false;
			}
			else if (key == "obj")
			{
				int idx = -1;
				stream >> idx;
				if (idx < 0 || idx >= (int)objects.size() || !objects[idx])
					continue;

				GameObject* object = objects[idx];
				if (currentFolder)
					currentFolder->objects.push_back(object);
				else if (inRoot)
					m_RootObjects.push_back(object);
			}
		}

		SyncHierarchy(objects);
	}

	GameObject* ObjectWindow::ConsumePickedObject()
	{
		GameObject* picked = m_PendingPick;
		m_PendingPick = nullptr;
		return picked;
	}

	GameObject* ObjectWindow::ConsumePendingDelete()
	{
		GameObject* deleted = m_PendingDelete;
		m_PendingDelete = nullptr;
		return deleted;
	}

	GameObject* ObjectWindow::ConsumePendingFocus()
	{
		GameObject* focus = m_PendingFocus;
		m_PendingFocus = nullptr;
		return focus;
	}

	void ObjectWindow::CreateFolder()
	{
		Folder folder;
		folder.id = m_NextFolderId++;
		folder.name = "New Folder";
		int suffix = 1;
		auto nameTaken = [&](const std::string& name)
		{
			for (const Folder& f : m_Folders)
				if (f.name == name) return true;
			return false;
		};
		while (nameTaken(folder.name))
		{
			folder.name = "New Folder (" + std::to_string(suffix) + ")";
			++suffix;
		}

		m_Folders.push_back(folder);
		StartRenameFolder(folder.id);
		BZ_INFO("Created hierarchy folder: {0}", folder.name);
	}

	void ObjectWindow::DeleteFolder(int folderId)
	{
		for (auto it = m_Folders.begin(); it != m_Folders.end(); ++it)
		{
			if (it->id != folderId)
				continue;

			m_RootObjects.insert(m_RootObjects.end(), it->objects.begin(), it->objects.end());
			if (m_RenamingFolderId == folderId)
			{
				m_RenamingFolderId = -1;
				m_FocusRename = false;
			}
			m_Folders.erase(it);
			return;
		}
	}

	void ObjectWindow::RemoveObjectFromHierarchy(GameObject* object)
	{
		if (!object)
			return;

		m_RootObjects.erase(std::remove(m_RootObjects.begin(), m_RootObjects.end(), object), m_RootObjects.end());
		for (Folder& folder : m_Folders)
			folder.objects.erase(std::remove(folder.objects.begin(), folder.objects.end(), object), folder.objects.end());
	}

	void ObjectWindow::MoveObjectToFolder(GameObject* object, int folderId)
	{
		if (!object)
			return;

		if (object->GetParent())
			object->ClearParent(true);

		RemoveObjectFromHierarchy(object);
		if (folderId < 0)
		{
			m_RootObjects.push_back(object);
			return;
		}

		for (Folder& folder : m_Folders)
		{
			if (folder.id == folderId)
			{
				folder.objects.push_back(object);
				folder.open = true;
				return;
			}
		}

		m_RootObjects.push_back(object);
	}

	void ObjectWindow::StartRenameObject(GameObject* object)
	{
		if (!object)
			return;

		m_RenamingObject = object;
		m_RenamingFolderId = -1;
		std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
		std::strncpy(m_RenameBuffer, object->GetName().c_str(), sizeof(m_RenameBuffer) - 1);
		m_FocusRename = true;
	}

	void ObjectWindow::StartRenameFolder(int folderId)
	{
		for (const Folder& folder : m_Folders)
		{
			if (folder.id != folderId)
				continue;

			m_RenamingFolderId = folderId;
			m_RenamingObject = nullptr;
			std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
			std::strncpy(m_RenameBuffer, folder.name.c_str(), sizeof(m_RenameBuffer) - 1);
			m_FocusRename = true;
			return;
		}
	}

	void ObjectWindow::CommitRename()
	{
		std::string newName = m_RenameBuffer;
		while (!newName.empty() && (newName.front() == ' ' || newName.front() == '\t'))
			newName.erase(newName.begin());
		while (!newName.empty() && (newName.back() == ' ' || newName.back() == '\t'))
			newName.pop_back();

		GameObject* object = m_RenamingObject;
		int folderId = m_RenamingFolderId;
		m_RenamingObject = nullptr;
		m_RenamingFolderId = -1;
		m_FocusRename = false;

		if (newName.empty())
		{
			BZ_INFO("Rename cancelled: empty name");
			return;
		}

		if (object)
		{
			object->SetName(newName);
			BZ_INFO("Renamed object to: {0}", newName);
			return;
		}

		if (folderId >= 0)
		{
			for (Folder& folder : m_Folders)
			{
				if (folder.id == folderId)
				{
					folder.name = newName;
					BZ_INFO("Renamed folder to: {0}", newName);
					return;
				}
			}
		}
	}

	void ObjectWindow::DrawObjectProp(GameObject* object)
	{
		if (!object)
			return;

		ImGui::PushID(object);
		bool selected = (object == m_Selected);

		if (m_RenamingObject == object)
		{
			if (m_FocusRename)
			{
				ImGui::SetKeyboardFocusHere();
				m_FocusRename = false;
			}

			bool renameDone = ImGui::InputText("##objrename", m_RenameBuffer, sizeof(m_RenameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			if (renameDone || ImGui::IsItemDeactivatedAfterEdit())
				CommitRename();
		}
		else
		{
			std::string label = object->GetName();
			if (object->IsCamera())
				label += object->DrivesView() ? " (Main Camera)" : " (Camera)";
			else if (object->GetType() == ObjectType::Circle)
				label += " (Circle)";
			else if (object->GetType() == ObjectType::Box)
				label += " (Box)";
			else if (object->GetType() == ObjectType::Sound)
				label += " (Sound)";

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, ImGui::GetStyle().ItemSpacing.y));

			if (object->IsCamera())
			{
				unsigned int tex = Gizmo2D::GetCameraIconTextureID();
				if (tex != 0)
				{
					ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(16.0f, 16.0f));
					ImGui::SameLine();
				}
			}
			else if (object->IsSound())
			{
				unsigned int tex = Gizmo2D::GetSoundIconTextureID();
				if (tex != 0)
				{
					ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(16.0f, 16.0f));
					ImGui::SameLine();
				}
			}

			ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick);
			bool hovered = ImGui::IsItemHovered();
			bool clicked = ImGui::IsItemClicked();
			bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
			{
				ImGui::SetDragDropPayload(s_SceneObjectPayload, &object, sizeof(GameObject*));
				if (object->IsCamera())
				{
					unsigned int tex = Gizmo2D::GetCameraIconTextureID();
					if (tex != 0)
						ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(16.0f, 16.0f));
				}
				else if (object->IsSound())
				{
					unsigned int tex = Gizmo2D::GetSoundIconTextureID();
					if (tex != 0)
						ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(16.0f, 16.0f));
				}
				ImGui::TextUnformatted(label.c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_SceneObjectPayload))
				{
					GameObject* child = *(GameObject**)payload->Data;
					if (child && child != object)
					{
						RemoveObjectFromHierarchy(child);
						if (child->SetParent(object, true))
							BZ_INFO("Parented '{0}' under '{1}'", child->GetName(), object->GetName());
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::PopStyleVar();

			if (clicked)
			{
				m_Selected = object;
				m_PendingPick = object;
				m_MapSelected = false;
			}
			if (doubleClicked)
			{
				m_Selected = object;
				m_PendingPick = object;
				m_PendingFocus = object;
				m_MapSelected = false;
			}
		}

		if (ImGui::BeginPopupContextItem("ObjectItemContext"))
		{
			m_Selected = object;
			m_PendingPick = object;

			if (ImGui::MenuItem("Focus"))
				m_PendingFocus = object;
			if (ImGui::MenuItem("Rename"))
				StartRenameObject(object);
			if (object->GetParent() && ImGui::MenuItem("Unparent"))
			{
				GameObject* parent = object->GetParent();
				object->ClearParent(true);
				RemoveObjectFromHierarchy(object);
				m_RootObjects.push_back(object);
				BZ_INFO("Unparented '{0}' from '{1}'", object->GetName(), parent ? parent->GetName() : "");
			}
			if (ImGui::MenuItem("Delete"))
			{
				RemoveObjectFromHierarchy(object);
				m_PendingDelete = object;
			}
			ImGui::EndPopup();
		}

		for (GameObject* child : object->GetChildren())
		{
			if (!child)
				continue;
			ImGui::Indent(12.0f);
			DrawObjectProp(child);
			ImGui::Unindent(12.0f);
		}

		ImGui::PopID();
	}

	void ObjectWindow::DrawFolder(Folder& folder)
	{
		ImGui::PushID(folder.id);

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (folder.open)
			flags |= ImGuiTreeNodeFlags_DefaultOpen;

		bool open = false;
		if (m_RenamingFolderId == folder.id)
		{
			open = ImGui::TreeNodeEx("##foldernode", flags, "");
			ImGui::SameLine();
			if (m_FocusRename)
			{
				ImGui::SetKeyboardFocusHere();
				m_FocusRename = false;
			}
			ImGui::SetNextItemWidth(-1.0f);
			bool renameDone = ImGui::InputText("##folderrename", m_RenameBuffer, sizeof(m_RenameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			if (renameDone || ImGui::IsItemDeactivatedAfterEdit())
				CommitRename();
		}
		else
		{
			open = ImGui::TreeNodeEx("##foldernode", flags, "   %s", folder.name.c_str());
			{
				unsigned int folderTex = Gizmo2D::GetFolderIconTextureID();
				if (folderTex != 0)
				{
					ImVec2 p = ImGui::GetItemRectMin();
					const float icon = 16.0f;
					p.x += ImGui::GetTreeNodeToLabelSpacing();
					p.y += (ImGui::GetItemRectSize().y - icon) * 0.5f;
					ImGui::GetWindowDrawList()->AddImage(
						(ImTextureID)(intptr_t)folderTex, p, ImVec2(p.x + icon, p.y + icon));
				}
			}		}

		folder.open = open;

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_SceneObjectPayload))
			{
				GameObject* object = *(GameObject**)payload->Data;
				MoveObjectToFolder(object, folder.id);
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginPopupContextItem("FolderItemContext"))
		{
			if (ImGui::MenuItem("Rename"))
				StartRenameFolder(folder.id);
			if (ImGui::MenuItem("Delete Folder"))
				m_PendingDeleteFolderId = folder.id;
			ImGui::EndPopup();
		}

		if (open)
		{
			for (GameObject* object : folder.objects)
				DrawObjectProp(object);
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	std::string ObjectWindow::GetActiveMapDisplayName()
	{
		const std::string& mapPath = Project::GetActiveMap();
		if (mapPath.empty())
			return "Untitled Map";
		return std::filesystem::path(mapPath).stem().string();
	}

	void ObjectWindow::DrawMapRoot()
	{
		const std::string mapName = GetActiveMapDisplayName();

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_DefaultOpen |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_Framed;
		if (m_MapSelected)
			flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::PushID("ActiveMapRoot");
		const bool open = ImGui::TreeNodeEx("##maproot", flags, "   %s", mapName.c_str());
		{
			unsigned int mapTex = Gizmo2D::GetMapIconTextureID();
			if (mapTex != 0)
			{
				ImVec2 p = ImGui::GetItemRectMin();
				const float icon = 16.0f;
				p.x += ImGui::GetTreeNodeToLabelSpacing();
				p.y += (ImGui::GetItemRectSize().y - icon) * 0.5f;
				ImGui::GetWindowDrawList()->AddImage(
					(ImTextureID)(intptr_t)mapTex, p, ImVec2(p.x + icon, p.y + icon));
			}
		}
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			SetMapSelected(true);

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_SceneObjectPayload))
			{
				GameObject* object = *(GameObject**)payload->Data;
				MoveObjectToFolder(object, -1);
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginPopupContextItem("MapRootContext"))
		{
			if (ImGui::MenuItem("Select Map"))
				SetMapSelected(true);
			if (ImGui::MenuItem("Create Folder"))
				CreateFolder();
			ImGui::EndPopup();
		}

		if (open)
		{
			if (m_Folders.empty() && m_RootObjects.empty())
				ImGui::TextDisabled("No objects in map");

			for (Folder& folder : m_Folders)
				DrawFolder(folder);

			for (GameObject* object : m_RootObjects)
				DrawObjectProp(object);

			ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 28.0f));
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_SceneObjectPayload))
				{
					GameObject* object = *(GameObject**)payload->Data;
					MoveObjectToFolder(object, -1);
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	void ObjectWindow::DrawContextMenu()
	{
		if (ImGui::BeginPopupContextWindow("ObjectsWindowContext",
			ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Folder"))
				CreateFolder();
			ImGui::EndPopup();
		}
	}

	void ObjectWindow::OnUpdate()
	{
		if (!m_Open)
			return;

		if (!ImGui::Begin("Objects", &m_Open))
		{
			ImGui::End();
			return;
		}

		onFocus = ImGui::IsWindowFocused();

		if (Project::HasProject())
			DrawMapRoot();
		else
			ImGui::TextDisabled("No project loaded");

		DrawContextMenu();

		if (m_PendingDeleteFolderId >= 0)
		{
			DeleteFolder(m_PendingDeleteFolderId);
			m_PendingDeleteFolderId = -1;
		}

		ImGui::End();
	}

	void ObjectWindow::SetSize(float w, float h)
	{
		 width = w;
		 height = h;
	}
}
