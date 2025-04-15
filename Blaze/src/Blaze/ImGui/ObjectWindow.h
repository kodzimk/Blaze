#pragma once

struct ImGuiIO;
struct ImGuiStyle;
struct GLFWwindow;

namespace Blaze
{
	class ObjectWindow
	{
	public:
		ObjectWindow(GLFWwindow* window);
		~ObjectWindow();

		void OnUpdate();
		void Init(GLFWwindow* window);
		void NewFrame();
		void EndFrame();

		bool OnFocus() const { return onFocus; }
		ImGuiIO* GetIO() const { return io; }
	private:
		ImGuiIO* io;
		ImGuiStyle* io_style;
		bool onFocus;
	};
}

