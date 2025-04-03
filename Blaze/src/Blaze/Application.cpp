
#include"../hzpch.h"
#include"Window.h"
#include "Application.h"

#include<GLFW/glfw3.h>

namespace Blaze {
	Application::Application()
	{
		
	}


	Application::~Application()
	{
	}

	void Application::Run()
	{	
		Window window(1600, 900, "Blaze Engine");
		int index = 1000;
		while (index > 0)
		{
			window.OnUpdate();
			index--;
		}
		while (true);
	}

}
