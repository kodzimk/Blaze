#include "ObjectWindow.h"

#include "../vendor/imgui/src/imgui.h"   

namespace Blaze
{

	MenuBar::MenuBar()
		: onFocus(false)
	{

	}
	MenuBar::~MenuBar()
	{

	}

	void MenuBar::OnUpdate()
	{
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Create")) {
				}
				if (ImGui::MenuItem("Open", "Ctrl+O")) {
				}
				if (ImGui::MenuItem("Save", "Ctrl+S")) {
				}
				if (ImGui::MenuItem("Save as..")) {
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	ObjectWindow::ObjectWindow()
		: onFocus(false)
	{
		m_menuBar = new MenuBar();
	}
	ObjectWindow::~ObjectWindow()
	{
		delete m_menuBar;
	}

	void ObjectWindow::OnUpdate()
	{
		ImGui::Begin("Objects", NULL,
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

		ImGui::SetWindowPos(ImVec2(0, 19));
		ImGui::SetWindowSize(ImVec2(300, 881));
		
		m_menuBar->OnUpdate();
		
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
