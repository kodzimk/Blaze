#include "ContentBrowser.h"

#include "../vendor/imgui/src/imgui.h"   

namespace Blaze
{
	float ContentBrowser::m_height = 250.f;
	float ContentBrowser::m_width = 1000.f;

	ContentBrowser::ContentBrowser()
		: onFocus(false)
	{
	}

	ContentBrowser::~ContentBrowser()
	{
	}

	void ContentBrowser::OnUpdate()
	{
		ImGui::Begin("Content Browser",NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImGui::SetWindowSize(ImVec2(m_width, m_height));
		ImGui::End();
	}

	void ContentBrowser::SetSize(float width, float height)
	{
		m_width = width;
		m_height = height;
	}
}