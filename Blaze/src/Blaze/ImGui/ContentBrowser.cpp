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
		ImGui::SetWindowSize(ImVec2(m_width, m_height));
		ImGui::Begin("Content Browser");

		ImGui::End();
	}

	void ContentBrowser::SetSize(float width, float height)
	{
		m_width = width;
		m_height = height;
	}
}