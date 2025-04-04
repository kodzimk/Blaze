
#include"bzpch.h"
#include "Application.h"
#include<GLFW/glfw3.h>
#include<imgui.h>
#include<imgui_impl_glfw.h>
#include<imgui_impl_opengl3.h>

namespace Blaze {
	Application::Application()
	{
		IMGUI_CHECKVERSION();
		window = Create();
	}

	Application::~Application()
	{
		window->DestroyWindow();
		delete window;
	}

	void Application::Run()
	{	
		int index = 10000000;
		while (index > 0)
		{   
			window->OnUpdate();
			index--;
		}
	}

}
