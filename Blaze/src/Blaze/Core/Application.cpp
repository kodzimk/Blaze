#include"bzpch.h"

#include"Application.h"
#include"Blaze/Event/ApplicationEvent.h"
#include"Blaze/Core/Input.h"

namespace Blaze {	
	#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;
	Application::Application()
		: m_running(true)
	{
		s_Instance = this;
		m_renderer = new Renderer();
		m_window = Create();
		m_window->SetCallBackEvent(BIND_EVENT_FN(OnEvent));
	
		m_context = new Context((GLFWwindow*)m_window->GetNativeWindow());
	}

	Application::~Application()
	{
		delete m_context;
		delete m_window;
		delete m_renderer;
	}

	void Application::WindowResize()
	{

	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_running = false;
		return true;
	}

	void Application::Run()
	{	
		std::vector<float> vertices = {
			-0.5f,-0.5f,
			-0.5f,0.5f,
			0.0f,0.5f
		};

		m_renderer->CreateCamera(0.0f,0.0f,1.0f);
		m_renderer->CreateObject(vertices);
		while(m_running)
		{   
			m_window->Clear(1.0f,0.0f,0.0f,1.0f);
			m_renderer->Render();
			m_context->OnUpdate();

			for (Layer* layer : m_layerStack){
				layer->OnUpdate();
			}
			m_window->OnUpdate();

			float x = Input::GetMouseX();
			float y = Input::GetMouseY();
			BZ_INFO("{0},{1}", x,y);
		}
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
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
