#include "Core.h"

#include"Blaze/Core/Window.h"
#include"Blaze/Renderer/Renderer.h"
#include"Blaze/ImGui/ImGuiContext.h"
#include"Blaze/Event/ApplicationEvent.h"

namespace Blaze {

	class BLAZE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void KeyButtonEvent(int key);
		void WindowResize();
		bool Application::OnWindowClose(WindowCloseEvent& e);
		void Run();
		void OnEvent(Event& e);

	public:
		Window *m_window;
		Renderer* m_renderer;
		Context* m_context;

	private:
		bool m_running;
	};
	Application* CreateApplication();
}