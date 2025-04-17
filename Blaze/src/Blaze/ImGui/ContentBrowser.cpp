#include "ContentBrowser.h"

#include "../vendor/imgui/src/imgui.h"   

namespace Blaze
{
	ContentBrowser::ContentBrowser()
	{
	}

	ContentBrowser::~ContentBrowser()
	{
	}

	void ContentBrowser::OnUpdate()
	{
		ImGui::Begin("Content Browser", NULL,ImGuiWindowFlags_MenuBar);

		ImGui::SetWindowPos(ImVec2(300, 700));
		ImGui::SetWindowSize(ImVec2(700, 300));


		ImGui::End();
	}
}