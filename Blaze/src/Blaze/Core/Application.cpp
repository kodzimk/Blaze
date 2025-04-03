
#include"bzpch.h"
#include "Application.h"

namespace Blaze {
	Application::Application()
	{
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
		glfwTerminate();
	}

}
