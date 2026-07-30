#pragma once
#include"Blaze/Core/Core.h"
#include"Blaze/Editor/UndoStack.h"
#include"bzpch.h"

namespace Blaze
{
	class GameObject;
	class Texture;

	class BLAZE_API ComponentsWindow
	{
	public:
		ComponentsWindow();
		virtual ~ComponentsWindow();

		void OnUpdate();
		void SetSelectedObject(GameObject* object)
		{
			if (m_Selected != object)
			{
				m_TransformEditActive = false;
				m_CompTileStrokeActive = false;
				m_CompTileStrokeChanges.clear();
			}
			m_Selected = object;
			if (object)
				m_MapSelected = false;
		}
		void SetMapSelected(bool selected)
		{
			m_MapSelected = selected;
			if (selected)
			{
				m_Selected = nullptr;
				m_TransformEditActive = false;
				m_CompTileStrokeActive = false;
				m_CompTileStrokeChanges.clear();
			}
		}
		void SetObjects(const std::vector<GameObject*>& objects) { m_Objects = objects; }
		bool OnFocus() const { return onFocus; }
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }
		static void SetSize(float width, float height);

		static int GetTilePaintBrush() { return s_TilePaintBrush; }
		static void SetTilePaintBrush(int id) { s_TilePaintBrush = id; }
		static bool IsTilePaintMode() { return s_TilePaintMode; }
		static void SetTilePaintMode(bool enabled) { s_TilePaintMode = enabled; }
		static void ToggleTilePaintMode() { s_TilePaintMode = !s_TilePaintMode; }
	private:
		Texture* GetThumbnail(const std::string& path);
		void AssignMainCamera(GameObject* camera);
	private:
		bool onFocus;
		bool m_Open = true;
		bool m_MapSelected = false;
		GameObject* m_Selected = nullptr;
		std::vector<GameObject*> m_Objects;
		std::unordered_map<std::string, Texture*> m_Thumbnails;
		bool m_TransformEditActive = false;
		LocalTransformSnap m_TransformEditBefore;
		bool m_CompTileStrokeActive = false;
		std::vector<TileChange> m_CompTileStrokeChanges;
		static int s_TilePaintBrush;
		static bool s_TilePaintMode;
	public:
		static float width;
		static float height;
	};
}
