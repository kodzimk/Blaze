#include "ImGuiContext.h"

#include "../vendor/imgui/src/imgui.h" 
#include "../vendor/imgui/src/imgui_internal.h"  
#include"../vendor/imgui/include/imgui_impl_glfw.h"
#include"../vendor/imgui/include/imgui_impl_opengl3.h"
#include"Blaze/Core/Window.h"
#include"GLFW/glfw3.h"

namespace Blaze
{
	Context::Context( GLFWwindow* window)
	{
		m_contextWindow = new ContentBrowser();
		m_objectWindow = new ObjectWindow();
		m_propWindow = new PropertiesWindow();
		m_startWindow = new StartWindow();

		Init(window);
	}

	Context::~Context()
	{	
		delete m_objectWindow;
		delete m_contextWindow;
		delete m_propWindow;
		delete m_startWindow;

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
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 450");

		io_style = &ImGui::GetStyle();
		io_style->Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.0f, 1.f, 0.0f);
		io_style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.0f, 0.f, 0.0f);
		io_style->Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.f, 1.0f);
		io_style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		io_style->Colors[ImGuiCol_WindowBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
		io_style->Colors[ImGuiCol_Separator] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
		io_style->Colors[ImGuiCol_DragDropTarget] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
		io_style->Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.f, 0.0f);
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
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());	
		glfwMakeContextCurrent((GLFWwindow*)Window::GetNativeWindow());
	}
	void Context::OnUpdate()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		// Set up dockspace flags
		ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		// Set up window class
		ImGuiWindowClass window_class;
		window_class.ClassId = 0; // Default class ID
		window_class.ParentViewportId = viewport->ID; // Parent viewport ID
		NewFrame();

		// Create a new window for the dockspace
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);

		io_style->Colors[ImGuiCol_WindowBg] = ImVec4(0.2f, 0.2f, 0.2f, 0.0f);
		ImGui::DockSpaceOverViewport(viewport, dockspace_flags, &window_class);


		io_style->Colors[ImGuiCol_WindowBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
		m_contextWindow->OnUpdate();
		m_propWindow->OnUpdate();
		m_objectWindow->OnUpdate();
		EndFrame();
	}
}