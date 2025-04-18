#include "PropertiesWindow.h"

#include "../vendor/imgui/src/imgui.h"   
#include"imgui_impl_glfw.h"
#include"imgui_impl_opengl3.h"

namespace Blaze
{
	PropertiesWindow::PropertiesWindow()
		: onFocus(false)
	{

	}

	PropertiesWindow::~PropertiesWindow()
	{

	}

	void PropertiesWindow::OnUpdate()
	{
		ImGui::Begin("Properties", NULL,
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

		ImGui::SetWindowPos(ImVec2(1300, 19));
		ImGui::SetWindowSize(ImVec2(300, 881));
		ImGui::Text("Name Of Object: Object1");

		ImGui::End();
	}
	
	StartWindow::StartWindow()
		: onFocus(false)
	{
	}

	StartWindow::~StartWindow()
	{
	}

	void StartWindow::OnUpdate()
	{
		ImGui::Begin(" ", NULL,
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImGui::SetWindowPos(ImVec2(300, 19));
		ImGui::SetWindowSize(ImVec2(1000, 20));
		ImGui::Button("Start");

		ImGui::End();
	}
}
