#include "ObjectWindow.h"

#include "../vendor/imgui/src/imgui.h"   

namespace Blaze
{

	ObjectWindow::ObjectWindow()
		: onFocus(false)
	{

	}
	ObjectWindow::~ObjectWindow()
	{

	}

	void ObjectWindow::OnUpdate()
	{
		ImGui::Begin("Objects", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_None);

		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(300, 900));
		
		for (size_t i = 0; i < m_objects.size(); i++)
		{
			ImGui::SetCursorPos(m_objects[i].pos);
			ImGui::Button(m_objects[i].name.c_str(), m_objects[i].size);
		}
	
		ImGui::End();
	}

	void ObjectWindow::AddObject(const char* name, ImVec2 size, ImVec2 pos)
	{
		m_objects.push_back(ObjectProp(name, size, pos));
	}
}
