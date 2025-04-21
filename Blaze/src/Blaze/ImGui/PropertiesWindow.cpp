#include "PropertiesWindow.h"

#include "../vendor/imgui/src/imgui.h"   
#include"../vendor/imgui/include/imgui_impl_glfw.h"
#include"../vendor/imgui/include/imgui_impl_opengl3.h"

#include"bzpch.h"

namespace Blaze
{
	float PropertiesWindow::width  = 300.f;
	float PropertiesWindow::height = 881.f;
	PropertiesWindow::PropertiesWindow()
		: onFocus(false)
	{
		
	}

	PropertiesWindow::~PropertiesWindow()
	{

	}

	void PropertiesWindow::OnUpdate()
	{
		ImGui::Begin("Properties", NULL,ImGuiWindowFlags_NoCollapse);
		ImGui::SetWindowSize(ImVec2(width, height));
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

	void PropertiesWindow::SetSize(float w, float h)
	{
		 width = w;
		 height = h;
	}
}
