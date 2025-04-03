
#include "Core.h"

#include"Blaze/Core/Window.h"

namespace Blaze {

	class BLAZE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

	public:
		Window *window;
	};
	// To be defined in CLIENT
	Application* CreateApplication();
}