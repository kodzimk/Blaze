#pragma once
#include"bzpch.h"
#include"../vendor/imgui/src/imgui.h"

namespace Blaze
{
	struct ObjectProp
	{
		std::string name;
		ImVec2 size;
		ImVec2 pos;

		ObjectProp(std::string name, ImVec2 size, ImVec2 pos) :
			name(name), size(size), pos(pos)
		{

		}
	};

	class MenuBar
	{
	public:
		MenuBar();
		virtual ~MenuBar();

		void OnUpdate();
		bool OnFocus() const { return onFocus; }

	private:
		bool onFocus;

	};

	class BLAZE_API ObjectWindow
	{
	public:
		ObjectWindow();
		virtual ~ObjectWindow();

		void OnUpdate();
		void AddObject(const char* name, ImVec2 size, ImVec2 pos);
		bool OnFocus() const { return onFocus; }
		static void SetSize(float width, float height);

	private:
		bool onFocus;
		std::vector<ObjectProp> m_objects;

	public:
		MenuBar* m_menuBar;
		static float width;
		static float height;
	};
}

