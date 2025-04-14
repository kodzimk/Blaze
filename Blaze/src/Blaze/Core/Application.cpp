
#include"bzpch.h"
#include "Application.h"
#include"Blaze/Event/ApplicationEvent.h"
#include<glm.hpp>

namespace Blaze {

#define BZ_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

	Application::Application()
		: m_running(true)
	{
		m_renderer = new Renderer();
		m_window = Create(this);
	}

	Application::~Application()
	{
		m_window->DestroyWindow();
		delete m_window;
		delete m_renderer;
	}

	void Application::WindowResize()
	{

	}

	void Application::WindowClose()
	{
		BZ_INFO("Window close");
		m_running = false;
	}

	void Application::BindEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BZ_BIND_EVENT_FN(Application::WindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(BZ_BIND_EVENT_FN(Application::WindowResize));
	}

	void Application::Run()
	{	
		std::vector<float> vertices = {
			-0.5f,-0.5f,
			-0.5f,0.5f,
			0.0f,0.5f
		};

		m_renderer->CreateCamera(0.5f,0.0f,0.5f);
		m_renderer->CreateObject(vertices);
		while(m_running)
		{   
			m_window->Clear(1.0f,0.0f,0.0f,1.0f);
			m_renderer->Render();
			m_window->OnUpdate();
		}
	}

}
