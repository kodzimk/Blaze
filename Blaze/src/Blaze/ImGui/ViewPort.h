#pragma once

#include"../vendor/imgui/include/imgui_impl_glfw.h"

namespace Blaze
{
	class ViewPort
	{
	public:
		ViewPort();
		virtual ~ViewPort();

		inline ImGuiViewport* GetViewPort() const { return m_ViewPort; }
		inline ImGuiDockNodeFlags GetDockFlags() const { return m_DockFlags; }
		inline ImGuiWindowClass GetWindowClass() const { return m_Window; }
	private:
		ImGuiViewport* m_ViewPort;
		ImGuiDockNodeFlags m_DockFlags;
		static ImGuiWindowClass m_Window;
	};
}

