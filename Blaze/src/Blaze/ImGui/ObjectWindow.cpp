#include "ObjectWindow.h"

#include"imgui.h"
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
		ImGui::Begin("Objects");

		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(400, 700));

		ImGui::End();
		ImGui::Render();
	}

	void ObjectWindow::Init(GLFWwindow* window)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		io = &ImGui::GetIO(); 
		ImGui::StyleColorsDark();
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 330");
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
