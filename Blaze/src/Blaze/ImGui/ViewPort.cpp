#include "ViewPort.h"

#include "../vendor/imgui/src/imgui.h" 
#include "../vendor/imgui/src/imgui_internal.h"  
#include"Blaze/Core/Window.h"
#include"GLFW/glfw3.h"

namespace Blaze
{
	ImGuiWindowClass ViewPort::m_Window;

	ViewPort::ViewPort()
	{
		m_ViewPort = ImGui::GetMainViewport();
		m_DockFlags = ImGuiDockNodeFlags_None;
		m_Window.ClassId = 0;
		m_Window.ParentViewportId = m_ViewPort->ID;
	}

	ViewPort::~ViewPort()
	{
	}
}