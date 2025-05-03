#include"bzpch.h"

#include"Application.h"
#include"Blaze/Event/ApplicationEvent.h"
#include"Blaze/Core/Input.h"
#include"Blaze/Event/MouseButtonCodes.h"
#include"Blaze/Logging/Log.h"
#include"Blaze/Renderer/Renderer.h"
#include"GLFW/glfw3.h"

namespace Blaze {	
	#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;
	Application::Application()
		: m_running(true)
	{
		s_Instance = this;
		m_window = Window::Create();
		m_window->SetCallBackEvent(BIND_EVENT_FN(OnEvent));
	
		m_context = new Context((GLFWwindow*)m_window->GetNativeWindow());
	}

	Application::~Application()
	{
		delete m_context;
		delete m_window;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		if (e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}

		m_Minimized = false;
		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
		return true;
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_running = false;
		return true;
	}

	void Application::Run()
	{	
		while(m_running)
		{   
			m_window->Clear(0.1f, 0.1f, 0.1f,255.0f);
			if (Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_RIGHT))
			{
				glfwSetInputMode((GLFWwindow*)m_window->GetNativeWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				BZ_INFO("Locking");
			}
			else {
				glfwSetInputMode((GLFWwindow*)m_window->GetNativeWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}

			if (!m_Minimized)
			{
				for (Layer* layer : m_layerStack) {
					layer->OnUpdate();
				}

			}
			m_context->OnUpdate();
			m_window->OnUpdate();
		}

	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResize));

		for (auto it = m_layerStack.end(); it != m_layerStack.begin();)
		{
			(*--it)->OnEvent(e);
			if (e.m_Handled)
				break;
		}
	}

	void Application::PushLayer(Layer* layer)
	{
		m_layerStack.PushLayer(layer);
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_layerStack.PushOverlay(layer);
	}

}
