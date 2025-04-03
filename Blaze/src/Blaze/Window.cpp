#include"hzpch.h"

#include"Window.h"
#include"ErrorHandler.h"

namespace Blaze {
	Window::Window(unsigned int width, int height, std::string title)
		: prop(width, height, title)
	{
		Init(prop);
	}

	void Window::Init(WindowProp& prop)
	{
		if (!glfwInit())
		{
			BZ_CORE_ERROR("Cant initialize 'glfw' library");
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

		window = glfwCreateWindow(prop.width, prop.height, prop.title.c_str(), NULL, NULL);

		if (window == NULL)
		{
			BZ_CORE_ERROR("Cant create window", "Invalid Title symbol");
		}
		else
		{
			BZ_CORE_INFO("Creating window {0} ({1}, {2})", prop.title, prop.width, prop.height);
		}
	}

	Window* Window::Create()
	{
		return new Window(1600,900,"Blaze Engine");
	}

	Window::~Window()
	{
	}

	void Window::DestroyWindow()
	{
		glfwDestroyWindow(window);
		BZ_INFO("Window Destroyed");
	}

	void Window::OnUpdate()
	{
		glfwPollEvents();
	}
}
