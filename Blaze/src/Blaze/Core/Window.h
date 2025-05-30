#pragma once

#include"bzpch.h"
struct GLFWwindow;

namespace Blaze
{
	class Event;
	using EventCallbackfn = std::function<void(Event&)>;
	struct WindowProp {
		EventCallbackfn m_callback;
		std::string title;
		unsigned int width, height;
		bool vSync;

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
		void Clear(float r = 0.0f, float b = 0.0f, float g = 0.0f, float a = 1.0f) const;
		void Init(const WindowProp& prop);
		void SetCallBackEvent(const EventCallbackfn& cb) { m_prop.m_callback = cb; };
		inline void* GetNativeWindow() const {	return m_window; }
		inline unsigned int GetWidth() const { return m_prop.width; }
		inline unsigned int GetHeight() const { return m_prop.height; }

		void SetVSync(bool enabled);
		bool IsVSync() const;

		static 	Window* Create();
	private:
	    GLFWwindow* m_window;
		WindowProp m_prop;

	};
}