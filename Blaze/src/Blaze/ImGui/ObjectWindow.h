#pragma once
#include"bzpch.h"
#include "../vendor/imgui/src/imgui.h"   

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
		ObjectWindow();
		~ObjectWindow();

		void OnUpdate();
		void AddObject(const char* name, ImVec2 size, ImVec2 pos);
		
		bool OnFocus() const { return onFocus; }
		ImVec2 GetWindowSize() {
			return ImVec2(300, 900);
		}
	private:
		bool onFocus;
		std::vector<ObjectProp> m_objects;
	};
}

