#pragma once
#include"Blaze/Core/Core.h"

#include"bzpch.h"
#include"ObjectWindow.h"
#include"ContentBrowser.h"
#include"PropertiesWindow.h"

namespace Blaze
{
	class Context
	{
	public:
		Context(GLFWwindow* window);
		Context(const Context&) = default;
		virtual ~Context();

		void Init(GLFWwindow* window);
		void NewFrame();
		void EndFrame();
		void OnUpdate();

	private:
		ImGuiIO* io = nullptr;
		ImGuiStyle* io_style = nullptr;

	public:
		MenuBar* m_menuBar;
		PropertiesWindow* m_propWindow;
		ObjectWindow* m_objectWindow;
		ContentBrowser* m_contextWindow;
	};
}

