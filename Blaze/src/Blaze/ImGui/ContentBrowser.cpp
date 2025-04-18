#include "ContentBrowser.h"

#include "../vendor/imgui/src/imgui.h"   

namespace Blaze
{
	ContentBrowser::ContentBrowser()
		: onFocus(false)
	{
	}

	ContentBrowser::~ContentBrowser()
	{
	}

	void ContentBrowser::OnUpdate()
	{
		ImGui::Begin("Content Browser",NULL,
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

		ImGui::SetWindowPos(ImVec2(300, 650));
		ImGui::SetWindowSize(ImVec2(1000, 250));


		ImGui::End();
	}
}