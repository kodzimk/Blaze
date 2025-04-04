#include"bzpch.h"

#include"Window.h"

#include<glad/glad.h>
#include<GLFW/glfw3.h>

#include"Blaze/Event/KeyEvent.h"
#include"Blaze/Event/MouseEvent.h"


namespace Blaze {
	Window::Window(unsigned int width, int height, std::string title)
		: prop(width, height, title)
	{
		Init(prop);
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

		window = glfwCreateWindow(prop.width, prop.height, prop.title.c_str(), NULL, NULL);

		if (window == NULL)
		{
			BZ_CORE_ERROR("Cant create window", "Invalid Title symbol");
		}
		else
		{
			BZ_CORE_INFO("Creating window {0} ({1}, {2})", prop.title, prop.width, prop.height);
		}


		glfwSetKeyCallback(window,[](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
				WindowProp& prop = *(WindowProp*)glfwGetWindowUserPointer(window);
				switch (action)
				{
				    case GLFW_PRESS:
				    {
				    	KeyPressedEvent event_press(key, false);
				    	event_press.ToString();
				    	break;
				    }
				    case GLFW_RELEASE:
				    {
				    	KeyReleasedEvent event_release(key);
				    	event_release.ToString();
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

		glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods)
		{

				WindowProp& prop = *(WindowProp*)glfwGetWindowUserPointer(window);
				MouseEventPressed mouse(button, mods);
				mouse.ToString();
		});

		glfwSetCursorPosCallback(window, [](GLFWwindow* window, double Xpos, double Ypos) 
		{
				WindowProp& prop = *(WindowProp*)glfwGetWindowUserPointer(window);
				MouseEventMoved mouse(Xpos, Ypos);
		});

		glfwSetScrollCallback(window, [](GLFWwindow* window, double Xoffset, double Yoffset)
		{
				WindowProp& prop = *(WindowProp*)glfwGetWindowUserPointer(window);
				MouseScrollEvent mouse(Xoffset, Yoffset);
				mouse.ToString();
		});
		
		glfwMakeContextCurrent(window);
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			BZ_CORE_ERROR("Failed to load 'Glad' library");
		}

		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	}

	void Window::SetVSync(bool enabled) 
	{
		prop.vSync = enabled;
	}

	bool Window::IsVSync() const
	{
		return prop.vSync;
	}

	Window::~Window()
	{
	}

	void Window::DestroyWindow()
	{
		glfwDestroyWindow(window);
		BZ_INFO("Window Destroyed");
	}

	void Window::OnUpdate()
	{
		glClear(GL_COLOR_BUFFER_BIT);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	Window* Create()
	{
		return new Window(1600, 900, "Blaze Engine");
	}
}
