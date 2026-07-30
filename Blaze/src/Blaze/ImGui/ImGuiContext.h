#pragma once
#include"Blaze/Core/Core.h"

#include"bzpch.h"
#include"ObjectWindow.h"
#include"ContentBrowser.h"
#include"PropertiesWindow.h"
#include"ComponentsWindow.h"
#include"ScriptEditorWindow.h"
#include"ViewPort.h"

namespace Blaze
{
	BLAZE_API bool UIWantCaptureMouse();
	BLAZE_API bool UIWantTextInput();
	BLAZE_API bool UIBlocksMainEditor();

	class Context
	{
	public:
		Context(GLFWwindow* window);
		Context(const Context& context) = default;
		virtual ~Context();

		void Init(GLFWwindow* window);
		void NewFrame();
		void EndFrame();
		void OnUpdate();

	private:
		ImGuiIO* io = nullptr;
		ImGuiStyle* io_style = nullptr;

	public:
		StartWindow* m_startWindow;
		PropertiesWindow* m_propWindow;
		ComponentsWindow* m_componentsWindow;
		ObjectWindow* m_objectWindow;
		ContentBrowser* m_contextWindow;
		ScriptEditorWindow* m_scriptEditor;
		ViewPort* m_ViewPort;
		MenuBar* m_menuBar;
		GameWindow* m_gameWindow;
		ProjectHub* m_projectHub;
	};
}

