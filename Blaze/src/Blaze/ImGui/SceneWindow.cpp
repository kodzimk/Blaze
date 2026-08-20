#include "SceneWindow.h"

#include "../vendor/imgui/src/imgui.h"
#include <glad/glad.h>
#include <algorithm>
#include <cmath>

namespace Blaze
{
	unsigned int SceneWindow::s_Framebuffer = 0;
	unsigned int SceneWindow::s_TextureId = 0;
	int SceneWindow::s_FrameWidth = 1280;
	int SceneWindow::s_FrameHeight = 720;
	int SceneWindow::s_DesiredWidth = 1280;
	int SceneWindow::s_DesiredHeight = 720;
	float SceneWindow::s_ImageMinX = 0.0f;
	float SceneWindow::s_ImageMinY = 0.0f;
	float SceneWindow::s_ImageMaxX = 0.0f;
	float SceneWindow::s_ImageMaxY = 0.0f;
	bool SceneWindow::s_Hovered = false;

	SceneWindow::SceneWindow()
	{
	}

	SceneWindow::~SceneWindow()
	{
		if (s_Framebuffer)
		{
			glDeleteFramebuffers(1, &s_Framebuffer);
			s_Framebuffer = 0;
		}
		if (s_TextureId)
		{
			glDeleteTextures(1, &s_TextureId);
			s_TextureId = 0;
		}
	}

	void SceneWindow::EnsureSize(int width, int height)
	{
		width = std::max(width, 1);
		height = std::max(height, 1);
		if (s_Framebuffer != 0 && width == s_FrameWidth && height == s_FrameHeight)
			return;

		if (s_TextureId)
			glDeleteTextures(1, &s_TextureId);
		if (s_Framebuffer)
			glDeleteFramebuffers(1, &s_Framebuffer);

		glGenFramebuffers(1, &s_Framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, s_Framebuffer);

		glGenTextures(1, &s_TextureId);
		glBindTexture(GL_TEXTURE_2D, s_TextureId);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_TextureId, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		s_FrameWidth = width;
		s_FrameHeight = height;
	}

	void SceneWindow::BeginCapture()
	{
		EnsureSize(s_DesiredWidth, s_DesiredHeight);
		glBindFramebuffer(GL_FRAMEBUFFER, s_Framebuffer);
		glViewport(0, 0, s_FrameWidth, s_FrameHeight);
		glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void SceneWindow::EndCapture(int windowWidth, int windowHeight)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, std::max(windowWidth, 1), std::max(windowHeight, 1));
	}

	void SceneWindow::OnUpdate()
	{
		s_Hovered = false;
		if (!m_Open)
		{
			s_ImageMinX = s_ImageMinY = s_ImageMaxX = s_ImageMaxY = 0.0f;
			return;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		if (!ImGui::Begin("Scene", &m_Open, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			ImGui::PopStyleVar();
			s_ImageMinX = s_ImageMinY = s_ImageMaxX = s_ImageMaxY = 0.0f;
			return;
		}

		const ImVec2 avail = ImGui::GetContentRegionAvail();
		s_DesiredWidth = std::max((int)std::floor(avail.x), 1);
		s_DesiredHeight = std::max((int)std::floor(avail.y), 1);

		if (s_TextureId != 0)
		{
			const ImVec2 size((float)s_DesiredWidth, (float)s_DesiredHeight);
			ImGui::Image((ImTextureID)(intptr_t)s_TextureId, size, ImVec2(0, 1), ImVec2(1, 0));

			const ImVec2 mn = ImGui::GetItemRectMin();
			const ImVec2 mx = ImGui::GetItemRectMax();
			s_ImageMinX = mn.x;
			s_ImageMinY = mn.y;
			s_ImageMaxX = mx.x;
			s_ImageMaxY = mx.y;

			s_Hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
				|| IsMouseOverSceneView();
		}
		else
		{
			ImGui::Dummy(avail);
			ImGui::SetCursorScreenPos(ImGui::GetItemRectMin());
			ImGui::TextDisabled("Waiting for Scene frame...");
			s_ImageMinX = s_ImageMinY = s_ImageMaxX = s_ImageMaxY = 0.0f;
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	bool SceneWindow::IsMouseOverSceneView()
	{
		if (s_ImageMaxX <= s_ImageMinX || s_ImageMaxY <= s_ImageMinY)
			return false;
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		return mouse.x >= s_ImageMinX && mouse.x <= s_ImageMaxX
			&& mouse.y >= s_ImageMinY && mouse.y <= s_ImageMaxY;
	}

	bool SceneWindow::WindowToScenePixel(float windowX, float windowY, float& outSceneX, float& outSceneY)
	{
		const float dw = s_ImageMaxX - s_ImageMinX;
		const float dh = s_ImageMaxY - s_ImageMinY;
		if (dw <= 1.0f || dh <= 1.0f)
			return false;
		outSceneX = (windowX - s_ImageMinX) / dw * (float)s_FrameWidth;
		outSceneY = (windowY - s_ImageMinY) / dh * (float)s_FrameHeight;
		return true;
	}

	bool SceneWindow::TryGetMouseScenePixel(float& outSceneX, float& outSceneY)
	{
		if (!IsMouseOverSceneView())
			return false;
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		return WindowToScenePixel(mouse.x, mouse.y, outSceneX, outSceneY);
	}

	bool SceneWindow::WantsEditorInput()
	{
		return s_Hovered || IsMouseOverSceneView();
	}
}
