#include "ObjectWindow.h"

#include "../vendor/imgui/src/imgui.h"   
#include"imgui_impl_glfw.h"
#include"imgui_impl_opengl3.h"

namespace Blaze
{
	ObjectWindow::ObjectWindow(GLFWwindow* window)
	{
		io_style = nullptr;
		io = nullptr;
		Init(window);
	}
	ObjectWindow::~ObjectWindow()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		delete io;
		delete io_style;
	}
	void ObjectWindow::OnUpdate()
	{
		ImGui::Begin("Components", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_None);

		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(400, 700));
		
		ImDrawList* list = ImGui::GetWindowDrawList();

		ImGui::Button("Object1");

	
		ImGui::End();
		ImGui::Render();
	}

	void ObjectWindow::Init(GLFWwindow* window)
	{
		IMGUI_CHECKVERSION();
		io_cntxt = ImGui::CreateContext();
		io = &ImGui::GetIO(); 

		io->ConfigFlags |= ImGuiConfigFlags_None;

		ImGui::StyleColorsDark();
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 450");

		io_style = &ImGui::GetStyle();
		io_style->Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.0f, 1.f, 0.0f);
		io_style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.0f, 1.f, 0.0f);
	}

	void ObjectWindow::NewFrame()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ObjectWindow::EndFrame()
	{
		ImGui::UpdatePlatformWindows();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
}
