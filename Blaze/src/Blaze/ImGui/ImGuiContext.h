#pragma once
#include"Blaze/Core/Core.h"

#include"bzpch.h"
#include"ObjectWindow.h"
#include"ContentBrowser.h"
#include"PropertiesWindow.h"
#include"ComponentsWindow.h"
#include"ScriptEditorWindow.h"
#include"WidgetEditorWindow.h"
#include"LocalizationEditorWindow.h"
#include"ParticleEditorWindow.h"
#include"ProjectSettingsWindow.h"
#include"BuildSettingsWindow.h"
#include"ViewPort.h"

struct ImGuiViewport;
typedef unsigned int ImGuiID;
struct ImVec2;

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
		void SetupDefaultDockLayout(ImGuiID dockspaceId, const ImVec2& size);

		ImGuiIO* io = nullptr;
		ImGuiStyle* io_style = nullptr;
		bool m_DefaultDockChecked = false;
		bool m_HadProjectLastFrame = false;

	public:
		StartWindow* m_startWindow;
		PropertiesWindow* m_propWindow;
		ComponentsWindow* m_componentsWindow;
		ObjectWindow* m_objectWindow;
		ContentBrowser* m_contextWindow;
		ScriptEditorWindow* m_scriptEditor;
		WidgetEditorWindow* m_widgetEditor;
		LocalizationEditorWindow* m_locEditor;
		ParticleEditorWindow* m_particleEditor;
		ProjectSettingsWindow* m_projectSettings;
		BuildSettingsWindow* m_buildSettings;
		ViewPort* m_ViewPort;
		MenuBar* m_menuBar;
		GameWindow* m_gameWindow;
		ProjectHub* m_projectHub;
	};
}
