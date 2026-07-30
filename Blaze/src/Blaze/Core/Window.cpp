#include"Window.h"

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include"Blaze/Event/ApplicationEvent.h"
#include"Blaze/Event/KeyEvent.h"
#include"Blaze/Event/MouseEvent.h"
#include"Blaze/Logging/Log.h"
#include"Blaze/Renderer/Camera.h"
#include"Blaze/ImGui/ImGuiContext.h"
#include"Blaze/Dep/stb_image.h"

#include <filesystem>
#include <vector>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace Blaze {

	static void ApplyBlazeWindowIcon(GLFWwindow* window)
	{
		if (!window)
			return;

		auto tryLoad = [](const fs::path& path, int& w, int& h) -> unsigned char*
		{
			std::error_code ec;
			if (!fs::exists(path, ec))
				return nullptr;
			int channels = 0;
			return stbi_load(path.string().c_str(), &w, &h, &channels, 4);
		};

		const char* sizedNames[] = {
			"blaze-logo-16.png",
			"blaze-logo-32.png",
			"blaze-logo-48.png",
		};

		std::vector<GLFWimage> images;
		std::vector<unsigned char*> owned;

		auto addSizedFromDir = [&](const fs::path& dir) -> bool
		{
			std::vector<GLFWimage> localImages;
			std::vector<unsigned char*> localOwned;
			for (const char* name : sizedNames)
			{
				int w = 0, h = 0;
				unsigned char* pixels = tryLoad(dir / name, w, h);
				if (!pixels)
				{
					for (unsigned char* p : localOwned)
						stbi_image_free(p);
					return false;
				}
				GLFWimage img{};
				img.width = w;
				img.height = h;
				img.pixels = pixels;
				localImages.push_back(img);
				localOwned.push_back(pixels);
			}
			images = std::move(localImages);
			owned = std::move(localOwned);
			return true;
		};

		std::vector<fs::path> sizedDirs = {
			fs::path("content") / "icons",
			fs::path("../Blaze/res/icons"),
			fs::path("Blaze/res/icons"),
			fs::path("res/icons"),
		};
#ifdef _WIN32
		{
			char modulePath[MAX_PATH] = {};
			if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) > 0)
			{
				const fs::path exeDir = fs::path(modulePath).parent_path();
				sizedDirs.push_back(exeDir / "content" / "icons");
				sizedDirs.push_back(exeDir / "Blaze" / "res" / "icons");
				sizedDirs.push_back(exeDir / ".." / ".." / ".." / "Blaze" / "res" / "icons");
				sizedDirs.push_back(exeDir / ".." / ".." / ".." / "Sandbox" / "content" / "icons");
			}
		}
#endif

		for (const fs::path& dir : sizedDirs)
		{
			if (addSizedFromDir(dir))
			{
				BZ_CORE_INFO("Window icon: {0}", (dir / "blaze-logo-*.png").string());
				break;
			}
		}

		if (images.empty())
		{
			std::vector<fs::path> candidates = {
				fs::path("content") / "icons" / "blaze-logo.png",
				fs::path("../Blaze/res/icons") / "blaze-logo.png",
				fs::path("Blaze/res/icons") / "blaze-logo.png",
				fs::path("res/icons") / "blaze-logo.png",
			};
#ifdef _WIN32
			{
				char modulePath[MAX_PATH] = {};
				if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) > 0)
				{
					const fs::path exeDir = fs::path(modulePath).parent_path();
					candidates.push_back(exeDir / "content" / "icons" / "blaze-logo.png");
					candidates.push_back(exeDir / "Blaze" / "res" / "icons" / "blaze-logo.png");
					candidates.push_back(exeDir / ".." / ".." / ".." / "Blaze" / "res" / "icons" / "blaze-logo.png");
					candidates.push_back(exeDir / ".." / ".." / ".." / "Sandbox" / "content" / "icons" / "blaze-logo.png");
				}
			}
#endif
			for (const fs::path& path : candidates)
			{
				int w = 0, h = 0;
				unsigned char* pixels = tryLoad(path, w, h);
				if (!pixels)
					continue;
				GLFWimage img{};
				img.width = w;
				img.height = h;
				img.pixels = pixels;
				images.push_back(img);
				owned.push_back(pixels);
				BZ_CORE_INFO("Window icon: {0}", path.string());
				break;
			}
		}

		if (!images.empty())
			glfwSetWindowIcon(window, (int)images.size(), images.data());
		else
			BZ_CORE_WARN("Window icon not found (blaze-logo.png)");

		for (unsigned char* pixels : owned)
			stbi_image_free(pixels);
	}

	Window::Window(unsigned int width, int height, std::string title)
		: m_prop(width, height, title)
	{
		Init(m_prop);
	}

	void Window::Init(const WindowProp& prop)
	{
		if (!glfwInit())
		{
			BZ_CORE_ERROR("Cant initialize 'glfw' library");
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		m_window = glfwCreateWindow(prop.width, prop.height, prop.title.c_str(), NULL, NULL);

		if (m_window == NULL)
		{
			BZ_CORE_ERROR("Cant create window", "Invalid Title symbol");
		}
		else
		{
			BZ_CORE_INFO("Creating window {0} ({1}, {2})", prop.title, prop.width, prop.height);
			ApplyBlazeWindowIcon(m_window);
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

				// Don't zoom the scene while using the script editor or any ImGui UI.
				if (Blaze::UIBlocksMainEditor() || Blaze::UIWantCaptureMouse())
					return;

				if (Blaze::Camera::zoom + Yoffset / 20.f > 0)
				{
					Blaze::Camera::zoom += Yoffset / 20.f;
					Blaze::Camera::ScaleCamera();
				}
		});

		glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) 
		{
			WindowProp& prop = *(WindowProp*)glfwGetWindowUserPointer(window);
			WindowCloseEvent e;
			prop.m_callback(e);
		});

		glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window,int width,int height)
		{
				WindowProp& prop = *(WindowProp*)glfwGetWindowUserPointer(window);

				if (width < 800.f)
				{
					glfwSetWindowSize(window, prop.width, prop.height);
				}
				else
				{
					prop.width = width;
				}

				if (height < 450.f)
				{
					glfwSetWindowSize(window, prop.width, prop.height);
				}
				else
				{
					prop.height = height;
				}

				WindowResizeEvent e(prop.width, prop.height);
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

	void Window::SetTitle(const std::string& title)
	{
		m_prop.title = title;
		if (m_window)
			glfwSetWindowTitle(m_window, title.c_str());
	}

	Window* Window::Create()
	{
		return new Window(1600, 900, "Blaze Engine");
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

	void Window::Clear(float r, float b, float g, float a) const
	{
	
		glClearColor(r, g, b, a);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void Window::OnUpdate()
	{
		glfwSwapBuffers(m_window);
		glfwPollEvents();
	}
}