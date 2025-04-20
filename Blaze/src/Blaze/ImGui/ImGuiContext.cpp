#include "ImGuiContext.h"

#include "../vendor/imgui/src/imgui.h"   
#include"../vendor/imgui/include/imgui_impl_glfw.h"
#include"../vendor/imgui/include/imgui_impl_opengl3.h"

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

		io->ConfigFlags |= ImGuiConfigFlags_None;

		ImGui::StyleColorsDark();
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 450");

		io_style = &ImGui::GetStyle();
		io_style->Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.0f, 1.f, 0.0f);
		io_style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.0f, 1.f, 0.0f);
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
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
	void Context::OnUpdate()
	{
		NewFrame();
		m_startWindow->OnUpdate();
		m_propWindow->OnUpdate();
		m_objectWindow->OnUpdate();
		m_contextWindow->OnUpdate();
		EndFrame();
	}
}