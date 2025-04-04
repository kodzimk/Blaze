
#include"bzpch.h"
#include "Application.h"

namespace Blaze {
	Application::Application()
		: m_running(true)
	{
		m_renderer = new Renderer();
		m_window = Create();
	}

	Application::~Application()
	{
		m_window->DestroyWindow();
		delete m_window;
		delete m_renderer;
	}

	void Application::Run()
	{	
		std::vector<float> vertices = {
			-0.5f,-0.5f,
			-0.5f,0.5f,
			0.0f,0.5f
		};

		m_renderer->CreateObject(vertices);
		while(m_running)
		{   
			m_window->Clear(1.0f,0.0f,0.0f,1.0f);
			m_renderer->Render();
			m_window->OnUpdate();
		}
	}

}
