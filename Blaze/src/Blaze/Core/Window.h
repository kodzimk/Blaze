#pragma once

#include"bzpch.h"
#include<GLFW/glfw3.h>

namespace Blaze
{
	struct WindowProp {
		std::string title;
		unsigned int width, height;

		WindowProp(unsigned int width, unsigned int height, std::string title)
			: width(width), height(height), title(title)
		{

		}
	};

	class Window
	{
	public:
		Window(unsigned int width, int height, std::string title);
		virtual ~Window();

		void DestroyWindow();
		void OnUpdate();
		void Init(WindowProp& prop);
		Window* Create();
	private:
		GLFWwindow* window;
		WindowProp prop;
	};
}

