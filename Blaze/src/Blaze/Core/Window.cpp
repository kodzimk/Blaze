#include"Window.h"

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include"Blaze/Event/ApplicationEvent.h"
#include"Blaze/Event/KeyEvent.h"
#include"Blaze/Event/MouseEvent.h"

namespace Blaze {

	Window::Window(unsigned int width, int height, std::string title)
		: m_prop(width, height, title)
	{
		Init(m_prop);
	}

	void Window::Init(WindowProp& prop)
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

		glfwSetWindowUserPointer(m_window, &this->m_prop);
		glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
				WindowProp& prop = *(WindowProp*)glfwGetWindowUserPointer(window);
				switch (action)
				{
				    case GLFW_PRESS:
				    {
						KeyPressedEvent e(key, 0);
						prop.m_callback(e);
				    	break;
				    }
				    case GLFW_RELEASE:
				    {
						KeyReleasedEvent e(key);
						prop.m_callback(e);
				    	break; 
				    }
				    case GLFW_REPEAT:
				    {
						KeyPressedEvent e(key, 1);
						prop.m_callback(e);
				    	break;
				    }
				}
		});

		glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods)
		{
				WindowProp& prop = *(WindowProp*)glfwGetWindowUserPointer(window);
				switch (action)
				{
				    case GLFW_PRESS:
				    {
				    	MouseButtonPressedEvent e(button);
						prop.m_callback(e);
				    	break;
				    }
				    case GLFW_RELEASE:
				    {
				    	MouseButtonReleasedEvent e(button);
						prop.m_callback(e);
				    	break;
				    }
				}
		});

		glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double Xpos, double Ypos)
		{
				WindowProp& prop = *(WindowProp*)glfwGetWindowUserPointer(window);
				MouseMovedEvent e((float)Xpos, (float)Ypos);
				prop.m_callback(e);
		});

		glfwSetScrollCallback(m_window, [](GLFWwindow* window, double Xoffset, double Yoffset)
		{
				WindowProp& prop = *(WindowProp*)glfwGetWindowUserPointer(window);
				MouseScrolledEvent e((float)Xoffset, (float)Yoffset);
				prop.m_callback(e);
		});

		glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) 
		{
			WindowProp& prop = *(WindowProp*)glfwGetWindowUserPointer(window);
			WindowCloseEvent e;
			prop.m_callback(e);
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
		DestroyWindow();
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

	Window* Create()
	{
		return new Window(1600, 900, "Blaze Engine");
	}
}