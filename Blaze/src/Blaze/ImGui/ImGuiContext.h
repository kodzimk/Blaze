#pragma once
#include"bzpch.h"
#include"ObjectWindow.h"
#include"ContentBrowser.h"

namespace Blaze
{
	class Context
	{
	public:
		Context(GLFWwindow* window);
		virtual ~Context();

		void Init(GLFWwindow* window);
		void NewFrame();
		void EndFrame();
		void OnUpdate();

	private:
		std::unique_ptr<ImGuiIO> io;
		ImGuiStyle* io_style;

	public:
		std::unique_ptr<ObjectWindow> m_objectWindow;
		std::unique_ptr<ContentBrowser> m_contentWindow;
	};
}

