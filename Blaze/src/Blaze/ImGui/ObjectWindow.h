#pragma once
#include"bzpch.h"

struct ImGuiIO;
struct ImGuiStyle;
struct GLFWwindow;
struct ImGuiContext;
struct ImVec2;

namespace Blaze
{
	struct ObjectProp
	{
		std::string name;
		int size;
	};

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
		ImGuiContext* GetContext() const { return io_cntxt; }
		ImGuiIO* GetIO() const { return io; }
	private:
		ImGuiContext* io_cntxt;
		ImGuiIO* io;
		ImGuiStyle* io_style;
		std::vector< ObjectProp> m_objects;
		bool onFocus;
	};
}

