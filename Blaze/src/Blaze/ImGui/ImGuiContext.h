#pragma once
#include"Blaze/Core/Core.h"

#include"bzpch.h"
#include"ObjectWindow.h"
#include"ContentBrowser.h"

namespace Blaze
{
	class BLAZE_API Context
	{
	public:
		Context(GLFWwindow* window);
		Context() = default;
		virtual ~Context();

		void Init(GLFWwindow* window);
		void NewFrame();
		void EndFrame();
		void OnUpdate();

	private:
		std::unique_ptr<ImGuiIO> io;
		ImGuiStyle* io_style = nullptr;

	public:
		std::unique_ptr<ContentBrowser> m_contentWindow;
	};
}

