#pragma once

#include"bzpch.h"

struct GLFWwindow;

namespace Blaze
{
	struct WindowProp {
		std::string title;
		unsigned int width, height;
		bool vSync;

		WindowProp(unsigned int width, unsigned int height, std::string title)
			: width(width), height(height), title(title),vSync(false)
		{

		}
	};

	class Window
	{
	public:
		Window(unsigned int width, int height, std::string title);
		virtual ~Window();

		void DestroyWindow();
		void OnUpdate();
		void Clear(float r = 0.0f, float b = 0.0f, float g = 0.0f, float a = 1.0f);
		void Init(WindowProp& prop);

		void SetVSync(bool enabled);
		bool IsVSync() const;

	private:
		GLFWwindow* m_window;
		WindowProp m_prop;
	};

	Window* Create();
}

