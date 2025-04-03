
#include"bzpch.h"
#include "Application.h"

#include"Window.h"

namespace Blaze {
	Application::Application()
	{
		
	}


	Application::~Application()
	{
	}

	void Application::Run()
	{	
		Window window(1600,900,"Blaze Engine");
		window.DestroyWindow();
		glfwTerminate();
	}

}
