#include "ImGuiContext.h"

#include "../vendor/imgui/src/imgui.h" 
#include "../vendor/imgui/src/imgui_internal.h"  
#include"../vendor/imgui/include/imgui_impl_glfw.h"
#include"../vendor/imgui/include/imgui_impl_opengl3.h"
#include"Blaze/Core/Application.h"
#include"GLFW/glfw3.h"

namespace Blaze
{
	Context::Context( GLFWwindow* window)
	{

		Init(window);
		m_contextWindow = new ContentBrowser();
		m_objectWindow = new ObjectWindow();
		m_propWindow = new PropertiesWindow();
		m_startWindow = new StartWindow();
		m_ViewPort = new ViewPort();
	}

	Context::~Context()
	{	
		delete m_objectWindow;
		delete m_contextWindow;
		delete m_propWindow;
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

	void Context::OnUpdate()
	{
		NewFrame();

		io_style->Colors[ImGuiCol_WindowBg] = ImVec4(0.2f, 0.2f, 0.2f, 0.0f);
		ImGui::DockSpaceOverViewport(m_ViewPort->GetViewPort(), m_ViewPort->GetDockFlags(), &m_ViewPort->GetWindowClass());

		io_style->Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
		m_startWindow->OnUpdate();
		m_contextWindow->OnUpdate();
		m_propWindow->OnUpdate();
		m_objectWindow->OnUpdate();
		EndFrame();
	}
}