#pragma once

#include"bzpch.h"
#include <functional>

struct GLFWwindow;

namespace Blaze
{
	class Event;

	struct WindowProp {
		std::string title;
		unsigned int width, height;
		bool vSync;

		std::function<void(Event&)>  m_callBack;

		WindowProp(unsigned int width, unsigned int height, std::string title)
			: width(width), height(height), title(title), vSync(false)
		{

		}
	};

	class BLAZE_API Window
	{
	public:


		Window(unsigned int width, int height, std::string title);
		virtual ~Window();

		void DestroyWindow();
		void OnUpdate();
		void Clear(float r = 0.0f, float b = 0.0f, float g = 0.0f, float a = 1.0f);
		void Init(WindowProp& prop);
		void SetCallBackEvent(std::function<void(Event&)> cb) { m_callback = cb; };

		GLFWwindow* GetWindow() const { return m_window; }

		void SetVSync(bool enabled);
		bool IsVSync() const;

	private:
		GLFWwindow* m_window;
		WindowProp m_prop;

	public:
		std::function<void(Event&)> m_callback;
	};

	Window* Create();
}