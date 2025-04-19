#include "Core.h"

#include"Blaze/Core/Window.h"
#include"Blaze/Renderer/Renderer.h"
#include"Blaze/ImGui/ImGuiContext.h"
#include"Blaze/Event/ApplicationEvent.h"
#include"Blaze/Core/LayerStack.h"

namespace Blaze {

	class BLAZE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void WindowResize();
		bool Application::OnWindowClose(WindowCloseEvent& e);
		void Run();
		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PopLayer(Layer* layer);
	private:
		Window *m_window;
		Renderer* m_renderer;
		Context* m_context;

		bool m_running;
		LayerStack m_layerStack;
	};
	Application* CreateApplication();
}