#include "ImGuiContext.h"

#include "../vendor/imgui/src/imgui.h"   
#include"imgui_impl_glfw.h"
#include"imgui_impl_opengl3.h"

namespace Blaze
{
	Context::Context(GLFWwindow* window)
	{
		m_contentWindow = std::make_unique<ContentBrowser>(ContentBrowser());
		m_objectWindow = std::make_unique<ObjectWindow>(ObjectWindow());
		m_objectWindow->AddObject("Object1", ImVec2(70, 20), ImVec2(20, 50));
		Init(window);
	}
	Context::~Context()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();

		delete io_style;
	}

	void Context::Init(GLFWwindow* window)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		io = std::make_unique<ImGuiIO>(ImGui::GetIO());

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
		m_objectWindow->OnUpdate();
		m_contentWindow->OnUpdate();
	}
}