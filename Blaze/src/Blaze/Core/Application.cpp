#include"bzpch.h"

#include"Application.h"
#include"Blaze/Event/ApplicationEvent.h"

namespace Blaze {	
	Application::Application()
		: m_running(true)
	{
		m_renderer = new Renderer();
		m_window = Create();
		std::function<void(Event&)> callback = std::bind(&Application::OnEvent, this, std::placeholders::_1);
		m_window->SetCallBackEvent(callback);

		m_context = new Context(m_window->GetWindow());

	}

	Application::~Application()
	{
		m_window->DestroyWindow();

		delete m_context;
		delete m_window;
		delete m_renderer;
	}

	void Application::KeyButtonEvent(int key)
	{
		
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
			m_window->OnUpdate();
		}
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);

		std::function<bool(WindowCloseEvent&)> callback = std::bind(&Application::OnWindowClose, this, std::placeholders::_1);
		dispatcher.Dispatch<WindowCloseEvent>(callback);

		BZ_CORE_INFO("{0}", e.GetName());
	}

}
