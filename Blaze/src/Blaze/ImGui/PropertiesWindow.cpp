#include "PropertiesWindow.h"

#include "../vendor/imgui/src/imgui.h"   
#include"../vendor/imgui/include/imgui_impl_glfw.h"
#include"../vendor/imgui/include/imgui_impl_opengl3.h"

#include"bzpch.h"

namespace Blaze
{
	float PropertiesWindow::width  = 300.f;
	float PropertiesWindow::height = 881.f;

	float StartWindow::width = 1000.f;
	float StartWindow::height = 50.f;

	PropertiesWindow::PropertiesWindow()
		: onFocus(false)
	{
		
	}

	PropertiesWindow::~PropertiesWindow()
	{

	}

	void PropertiesWindow::OnUpdate()
	{
		ImGui::Begin("Properties");
		ImGui::SetWindowSize(ImVec2(width, height));
		ImGui::Text("Name Of Object: Object1");

		ImGui::End();
	}

	void PropertiesWindow::SetSize(float w, float h)
	{
		width = w;
		height = h;
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
		ImGui::SetNextWindowSize(ImVec2(width, height));

		ImGui::Begin(" ", NULL,
		 ImGuiWindowFlags_NoCollapse| ImGuiWindowFlags_NoTitleBar
		 | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::Button("Start");
		ImGui::End();
	}

	void StartWindow::SetSize(float w, float h)
	{
		width = w;
		height = h;
	}
}
