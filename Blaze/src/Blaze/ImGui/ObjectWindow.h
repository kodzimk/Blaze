#pragma once
#include"bzpch.h"
#include"../vendor/imgui/src/imgui.h"
#include <iosfwd>

namespace Blaze
{
	class GameObject;

	enum class FileAction
	{
		None = 0,
		NewProject,
		OpenProject,
		Save,
		SaveAs,
		CloseProject,
		ProjectSettings,
		BuildSettings,
		BuildProject
	};

	enum class HubAction
	{
		None = 0,
		LoadProject,
		CreateProject
	};

	// Startup screen shown while no project is loaded: lists recent projects
	// and lets the user create or open one.
	class BLAZE_API ProjectHub
	{
	public:
		ProjectHub();
		virtual ~ProjectHub();

		void OnUpdate();

		static HubAction ConsumeAction();
	private:
		static HubAction s_Action;
	};

	class BLAZE_API MenuBar
	{
	public:
		MenuBar();
		virtual ~MenuBar();

		void OnUpdate(bool* contentBrowserOpen, bool* objectsOpen, bool* gameObjectOpen, bool* componentsOpen, bool* toolbarOpen, bool* projectSettingsOpen = nullptr, bool* sceneOpen = nullptr);
		bool OnFocus() const { return onFocus; }

		static FileAction ConsumeFileAction();

	private:
		bool onFocus;
		static FileAction s_FileAction;
	};

	class BLAZE_API ObjectWindow
	{
	public:
		ObjectWindow();
		virtual ~ObjectWindow();

		void OnUpdate();
		bool OnFocus() const { return onFocus; }
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }
		static void SetSize(float width, float height);

		void SetObjects(const std::vector<GameObject*>& objects);
		void SetSelectedObject(GameObject* object);
		void SetSelectedObjects(const std::vector<GameObject*>& objects, GameObject* primary);
		void ClearHierarchy();
		void SaveHierarchy(std::ostream& out) const;
		void LoadHierarchy(std::istream& in, const std::vector<GameObject*>& objects);

		GameObject* ConsumePickedObject();
		bool ConsumePickedAdditive();
		GameObject* ConsumePendingDelete();
		GameObject* ConsumePendingFocus();
		GameObject* ConsumePendingSavePrefab();
		GameObject* ConsumePendingDuplicate();
		bool IsMapSelected() const { return m_MapSelected; }
		void SetMapSelected(bool selected);

	private:
		struct Folder
		{
			int id = 0;
			std::string name;
			bool open = true;
			std::vector<GameObject*> objects;
		};

		void SyncHierarchy(const std::vector<GameObject*>& objects);
		void CreateFolder();
		void DeleteFolder(int folderId);
		void RemoveObjectFromHierarchy(GameObject* object);
		void MoveObjectToFolder(GameObject* object, int folderId); // -1 = root
		void StartRenameObject(GameObject* object);
		void StartRenameFolder(int folderId);
		void CommitRename();
		void DrawObjectProp(GameObject* object);
		void DrawFolder(Folder& folder);
		void DrawMapRoot();
		void DrawContextMenu();
		static std::string GetActiveMapDisplayName();
	private:
		bool onFocus;
		bool m_Open = true;
		std::vector<GameObject*> m_Objects;
		std::vector<GameObject*> m_RootObjects;
		std::vector<Folder> m_Folders;
		int m_NextFolderId = 1;

		GameObject* m_Selected = nullptr;
		std::vector<GameObject*> m_MultiSelected;
		GameObject* m_PendingPick = nullptr;
		bool m_PendingPickAdditive = false;
		GameObject* m_PendingDelete = nullptr;
		GameObject* m_PendingFocus = nullptr;
		GameObject* m_PendingSavePrefab = nullptr;
		GameObject* m_PendingDuplicate = nullptr;
		int m_PendingDeleteFolderId = -1;
		bool m_MapSelected = false;

		GameObject* m_RenamingObject = nullptr;
		int m_RenamingFolderId = -1;
		char m_RenameBuffer[256] = {};
		bool m_FocusRename = false;

	public:
		static float width;
		static float height;
	};
}
