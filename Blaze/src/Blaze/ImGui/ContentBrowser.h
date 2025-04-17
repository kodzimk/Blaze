#pragma once
#include"bzpch.h"

struct GLFWwindow;
struct ImVec2;
struct ImGuiIO;
struct ImGuiStyle;
struct ImGuiContext;

namespace Blaze
{
	class ContentBrowser
	{
	public:
		ContentBrowser();
		virtual ~ContentBrowser();

		void OnUpdate();

		bool OnFocus() const { return onFocus; }

	private:
		bool onFocus;
	};
}

