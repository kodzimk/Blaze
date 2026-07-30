#pragma once
#include"Blaze/Core/Core.h"
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
		void SetSelectedObject(GameObject* object) { m_Selected = object; }
		void SetObjects(const std::vector<GameObject*>& objects) { m_Objects = objects; }
		bool OnFocus() const { return onFocus; }
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }
		static void SetSize(float width, float height);
	private:
		Texture* GetThumbnail(const std::string& path);
		void AssignMainCamera(GameObject* camera);
	private:
		bool onFocus;
		bool m_Open = true;
		GameObject* m_Selected = nullptr;
		std::vector<GameObject*> m_Objects;
		std::unordered_map<std::string, Texture*> m_Thumbnails;
	public:
		static float width;
		static float height;
	};
}
