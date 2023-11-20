#pragma once

#include"Core.h"

namespace Blaze{

	class _declspec(dllexport)Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
	};


	Application* CreateApplication();
}


