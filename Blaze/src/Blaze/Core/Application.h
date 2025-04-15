
#include "Core.h"

#include"Blaze/Core/Window.h"
#include"Blaze/Renderer/Renderer.h"
#include"Blaze/ImGui/ObjectWindow.h"

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
		ObjectWindow* m_objectWindow;
		Renderer* m_renderer;
		Window *m_window;
		bool m_running;
	};
	// To be defined in CLIENT
	Application* CreateApplication();
}