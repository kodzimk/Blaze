#include "ImGuiContext.h"

#include "../vendor/imgui/src/imgui.h" 
#include "../vendor/imgui/src/imgui_internal.h"  
#include"../vendor/imgui/include/imgui_impl_glfw.h"
#include"../vendor/imgui/include/imgui_impl_opengl3.h"
#include"Blaze/Core/Application.h"
#include"Blaze/Core/Project.h"
#include"Blaze/Scripting/ScriptEngine.h"
#include"Blaze/ImGui/ParticleEditorWindow.h"
#include"GLFW/glfw3.h"
#include <cstdio>

namespace Blaze
{
	bool UIWantCaptureMouse()
	{
		return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
	}

	bool UIWantTextInput()
	{
		return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput;
	}

	bool UIBlocksMainEditor()
	{
		if (ScriptEditorWindow::IsBlockingMainEditor())
			return true;
		if (WidgetEditorWindow::IsBlockingMainEditorStatic())
			return true;
		if (ParticleEditorWindow::IsBlockingMainEditor())
			return true;
		return UIWantTextInput();
	}

	Context::Context( GLFWwindow* window)
	{

		Init(window);
		m_contextWindow = new ContentBrowser();
		m_scriptEditor = new ScriptEditorWindow();
		m_widgetEditor = new WidgetEditorWindow();
		m_locEditor = new LocalizationEditorWindow();
		m_particleEditor = new ParticleEditorWindow();
		m_projectSettings = new ProjectSettingsWindow();
		m_buildSettings = new BuildSettingsWindow();
		m_objectWindow = new ObjectWindow();
		m_propWindow = new PropertiesWindow();
		m_componentsWindow = new ComponentsWindow();
		m_startWindow = new StartWindow();
		m_ViewPort = new ViewPort();
		m_menuBar = new MenuBar();
		m_gameWindow = new GameWindow();
		m_projectHub = new ProjectHub();
	}

	Context::~Context()
	{	
		delete m_projectHub;
		delete m_gameWindow;
		delete m_menuBar;
		delete m_objectWindow;
		delete m_contextWindow;
		delete m_scriptEditor;
		delete m_widgetEditor;
		delete m_locEditor;
		delete m_particleEditor;
		delete m_projectSettings;
		delete m_buildSettings;
		delete m_propWindow;
		delete m_componentsWindow;
		delete m_startWindow;
		delete m_ViewPort;

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
	}

	void Context::Init(GLFWwindow* window)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		io = &ImGui::GetIO();
         
		io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;         
		io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		ImGui::StyleColorsDark();

		{
			const char* fontCandidates[] = {
				"C:/Windows/Fonts/consola.ttf",
				"C:/Windows/Fonts/cascadiamono.ttf",
				"C:/Windows/Fonts/cour.ttf",
				"C:/Windows/Fonts/arial.ttf",
				"C:/Windows/Fonts/segoeui.ttf",
			};
			ImFontConfig cfg;
			cfg.OversampleH = 2;
			cfg.OversampleV = 1;
			cfg.PixelSnapH = true;
			ImFont* loaded = nullptr;
			for (const char* path : fontCandidates)
			{
				loaded = io->Fonts->AddFontFromFileTTF(path, 16.0f, &cfg, io->Fonts->GetGlyphRangesCyrillic());
				if (loaded)
					break;
			}
			if (!loaded)
				io->Fonts->AddFontDefault();
		}

		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 450");

		io_style = &ImGui::GetStyle();
		auto& colors = ImGui::GetStyle().Colors;

		colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	
		colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

	
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.f, 0.0f);
		colors[ImGuiCol_DockingPreview] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	}

	void Context::NewFrame()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void Context::EndFrame()
	{
		ImGui::Render();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());	
	}

	void Context::SetupDefaultDockLayout(ImGuiID dockspaceId, const ImVec2& size)
	{
		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, size);

		ImGuiID dockMain = dockspaceId;
		ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.16f, nullptr, &dockMain);
		ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.24f, nullptr, &dockMain);
		ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.28f, nullptr, &dockMain);
		ImGuiID dockTop = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Up, 0.07f, nullptr, &dockMain);

		ImGui::DockBuilderDockWindow("Objects", dockLeft);
		ImGui::DockBuilderDockWindow("Toolbar", dockTop);
		ImGui::DockBuilderDockWindow("Content Browser", dockBottom);
		ImGui::DockBuilderDockWindow("Components", dockRight);
		ImGui::DockBuilderDockWindow("GameObject", dockRight);

		ImGui::DockBuilderFinish(dockspaceId);
	}

	void Context::OnUpdate()
	{
		NewFrame();

		io_style->Colors[ImGuiCol_WindowBg] = ImVec4(0.2f, 0.2f, 0.2f, 0.0f);

		const ImGuiViewport* viewport = m_ViewPort->GetViewPort();
		if (viewport == nullptr)
			viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags hostFlags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		if (m_ViewPort->GetDockFlags() & ImGuiDockNodeFlags_PassthruCentralNode)
			hostFlags |= ImGuiWindowFlags_NoBackground;

		char hostLabel[32];
		std::snprintf(hostLabel, sizeof(hostLabel), "DockSpaceViewport_%08X", viewport->ID);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin(hostLabel, nullptr, hostFlags);
		ImGui::PopStyleVar(3);

		ImGuiID dockspaceId = ImGui::GetID("DockSpace");

		const bool hasProject = Project::HasProject();
		const bool projectJustOpened = hasProject && !m_HadProjectLastFrame;
		m_HadProjectLastFrame = hasProject;

		if (hasProject)
		{
			ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockspaceId);
			const bool needsDefault =
				projectJustOpened ||
				(!m_DefaultDockChecked && (root == nullptr || !root->IsSplitNode()));

			if (needsDefault)
				SetupDefaultDockLayout(dockspaceId, viewport->WorkSize);

			m_DefaultDockChecked = true;
		}

		ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), m_ViewPort->GetDockFlags(), &m_ViewPort->GetWindowClass());
		ImGui::End();

		io_style->Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);

		// until a project is picked, only the project hub is shown
		if (!hasProject)
		{
			m_DefaultDockChecked = false;
			m_projectHub->OnUpdate();
			EndFrame();
			return;
		}

		m_menuBar->OnUpdate(
			m_contextWindow->GetOpenPtr(),
			m_objectWindow->GetOpenPtr(),
			m_propWindow->GetOpenPtr(),
			m_componentsWindow->GetOpenPtr(),
			m_startWindow->GetOpenPtr(),
			m_projectSettings->GetOpenPtr());
		m_startWindow->OnUpdate();
		m_contextWindow->OnUpdate();
		m_scriptEditor->OnUpdate();
		m_widgetEditor->OnUpdate();
		m_locEditor->OnUpdate();
		m_particleEditor->OnUpdate();
		m_projectSettings->OnUpdate();
		m_buildSettings->OnUpdate();
		m_propWindow->OnUpdate();
		m_componentsWindow->OnUpdate();
		m_objectWindow->OnUpdate();
		m_gameWindow->OnUpdate();
		ScriptEngine::DrawErrorWindow();
		GameWindow::ApplyPlayCursor();
		EndFrame();
	}
}