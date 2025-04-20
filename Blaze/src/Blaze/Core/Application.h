#include "Core.h"

#include"Blaze/Core/Window.h"
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

		inline static Application& Get() { return *s_Instance; }
		inline Window& GetWindow() const { return *m_window; }

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);
	private:
		Window *m_window;
		Context* m_context;

		bool m_running;
		LayerStack m_layerStack;
	private:
		static Application* s_Instance;
	};
	Application* CreateApplication();
}