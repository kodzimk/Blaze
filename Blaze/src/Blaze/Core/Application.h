
#include "Core.h"

#include"Blaze/Core/Window.h"
#include"Blaze/Renderer/Renderer.h"

namespace Blaze {

	class BLAZE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void KeyButtonEvent(int key);
		void WindowResize();
		void WindowClose();
		void Run();

	public:
		Renderer* m_renderer;
		Window *m_window;
		bool m_running;
	};
	Application* CreateApplication();
}