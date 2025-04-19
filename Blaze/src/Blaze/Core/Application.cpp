#include"bzpch.h"

#include"Application.h"
#include"Blaze/Event/ApplicationEvent.h"

namespace Blaze {	
	#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)
	Application::Application()
		: m_running(true)
	{
		m_renderer = new Renderer();
		m_window = Create();
		m_window->SetCallBackEvent(BIND_EVENT_FN(OnEvent));
	
		m_context = new Context(m_window->GetWindow());
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

	void Application::PopLayer(Layer* layer)
	{
		m_layerStack.PopLayer(layer);
	}

}
