#include"bzpch.h"

#include"Window.h"

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include"Blaze/Core/Application.h"
#include"Blaze/Event/ApplicationEvent.h"
#include"Blaze/ImGui/ObjectWindow.h"


namespace Blaze {

	Window::Window(unsigned int width, int height, std::string title, Application* app)
		: m_prop(width, height, title)
	{
		this->app = app;
		Init(m_prop,io);

	}

	void Window::Init(WindowProp& prop,ImGuiIO* io)
	{
		if (!glfwInit())
		{
			BZ_CORE_ERROR("Cant initialize 'glfw' library");
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		m_window = glfwCreateWindow(prop.width, prop.height, prop.title.c_str(), NULL, NULL);

		if (m_window == NULL)
		{
			BZ_CORE_ERROR("Cant create window", "Invalid Title symbol");
		}
		else
		{
			BZ_CORE_INFO("Creating window {0} ({1}, {2})", prop.title, prop.width, prop.height);
		}


		glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
			{
				Window& win = *(Window*)glfwGetWindowUserPointer(window);
				switch (action)
				{
				    case GLFW_PRESS:
				    {
				    	KeyPressedEvent event_press(key, 0);
						win.app->KeyButtonEvent(key);
				    	break;
				    }
				    case GLFW_RELEASE:
				    {
				    	KeyReleasedEvent event_release(key);
				    	break;
				    }
				    case GLFW_REPEAT:
				    {
				    	KeyPressedEvent event(key, true);
				    	event.ToString();
				    	break;
				    }
				}
			});

		glfwSetWindowUserPointer(m_window, this);

		glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods)
			{
				Window& win = *(Window*)glfwGetWindowUserPointer(window);
				MousePressedEvent mouse(button);
			});

		glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double Xpos, double Ypos)
			{
				Window& win = *(Window*)glfwGetWindowUserPointer(window);
				MouseMovedEvent mouse(Xpos, Ypos);
			});

		glfwSetScrollCallback(m_window, [](GLFWwindow* window, double Xoffset, double Yoffset)
			{
				Window& win = *(Window*)glfwGetWindowUserPointer(window);
				MouseScrollEvent mouse(Xoffset);
			});

		glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) {
			Window& win = *(Window*)glfwGetWindowUserPointer(window);
			win.app->WindowClose();
			});


		glfwMakeContextCurrent(m_window);
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			BZ_CORE_ERROR("Failed to load 'Glad' library");
		}
	}

	void Window::SetVSync(bool enabled)
	{
		m_prop.vSync = enabled;
	}

	bool Window::IsVSync() const
	{
		return m_prop.vSync;
	}

	Window::~Window()
	{
	}

	void Window::DestroyWindow()
	{
		glfwDestroyWindow(m_window);
		BZ_INFO("Window Destroyed");
	}

	void Window::Clear(float r, float b, float g, float a)
	{
		glClearColor(r, g, b, a);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void Window::OnUpdate()
	{
		glfwSwapBuffers(m_window);
		glfwPollEvents();
	}

	Window* Create(Application* app)
	{
		return new Window(1600, 900, "Blaze Engine", app);
	}
}