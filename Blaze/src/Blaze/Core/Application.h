
#include "Core.h"

#include"Blaze/Core/Window.h"
#include"Blaze/Renderer/Renderer.h"

namespace Blaze {

	class BLAZE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

	public:
		Renderer* m_renderer;
		Window *m_window;

	private:
		bool m_running;
	};
	// To be defined in CLIENT
	Application* CreateApplication();
}