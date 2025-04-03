#pragma once

#include"bzpch.h"
#include<GLFW/glfw3.h>

namespace Blaze
{
	struct WindowProp {
		std::string title;
		unsigned int width, height;
		bool vSync;

		WindowProp(unsigned int width, unsigned int height, std::string title)
			: width(width), height(height), title(title),vSync(false)
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

		void SetVSync(bool enabled);
		bool IsVSync() const;

	private:
		GLFWwindow* window;
		WindowProp prop;
	};

	Window* Create();
}

