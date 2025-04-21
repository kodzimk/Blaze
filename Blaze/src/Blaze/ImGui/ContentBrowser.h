#pragma once
#include"bzpch.h"

struct GLFWwindow;
struct ImVec2;
struct ImGuiIO;
struct ImGuiStyle;
struct ImGuiContext;

namespace Blaze
{
	class BLAZE_API ContentBrowser
	{
	public:
		ContentBrowser();
		virtual ~ContentBrowser();

		void OnUpdate();
		bool OnFocus() const { return onFocus; }
		static void SetSize(float width, float height);
	private:
		bool onFocus;
	public:
		static float m_width;
		static float m_height;
	};
}

