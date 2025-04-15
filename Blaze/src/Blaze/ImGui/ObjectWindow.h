#pragma once
#include"bzpch.h"
#include "../vendor/imgui/src/imgui.h"   


struct GLFWwindow;


namespace Blaze
{
	struct ObjectProp
	{
		std::string name;
		ImVec2 size;
		ImVec2 pos;

		ObjectProp(std::string name, ImVec2 size, ImVec2 pos) :
			name(name), size(size), pos(pos)
		{

		}
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
		void AddObject(const char* name, ImVec2 size, ImVec2 pos);
		
		bool OnFocus() const { return onFocus; }
		ImGuiContext* GetContext() const { return io_cntxt; }
		ImGuiIO* GetIO() const { return io; }
	private:
		ImGuiContext* io_cntxt;
		ImGuiIO* io;
		ImGuiStyle* io_style;
		std::vector<ObjectProp> m_objects;
		bool onFocus;
	};
}

