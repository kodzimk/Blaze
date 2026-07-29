#include "PropertiesWindow.h"

#include<glad/glad.h>
#include "../vendor/imgui/src/imgui.h"   
#include"../vendor/imgui/include/imgui_impl_glfw.h"
#include"../vendor/imgui/include/imgui_impl_opengl3.h"

#include"bzpch.h"
#include"Blaze/Logging/Log.h"
#include <cstring>

namespace Blaze
{
	float PropertiesWindow::width  = 300.f;
	float PropertiesWindow::height = 881.f;

	float StartWindow::width = 1000.f;
	float StartWindow::height = 50.f;

	std::string PropertiesWindow::s_DraggingType;
	std::string PropertiesWindow::s_PendingDropType;

	static const char* s_GameObjectPayload = "BLAZE_GAMEOBJECT";

	struct GameObjectType
	{
		const char* displayName;
		const char* typeId;
		const char* description;
	};

	static const GameObjectType s_AvailableTypes[] =
	{
		{ "Sprite", "sprite", "Textured 2D quad" },
		{ "Empty",  "empty",  "Empty transform object" },
		{ "Camera", "camera", "Scene view camera" },
	};

	PropertiesWindow::PropertiesWindow()
		: onFocus(false)
	{
	}

	PropertiesWindow::~PropertiesWindow()
	{
	}

	void PropertiesWindow::OnUpdate()
	{
		if (!m_Open)
			return;

		if (!ImGui::Begin("GameObject", &m_Open))
		{
			ImGui::End();
			return;
		}
		onFocus = ImGui::IsWindowFocused();

		ImGui::TextUnformatted("Available Objects");
		ImGui::TextDisabled("Drag onto the scene to create");
		ImGui::Separator();

		const float thumbSize = 72.0f;
		const float cellSize = thumbSize + 28.0f;
		int columns = (int)(ImGui::GetContentRegionAvail().x / cellSize);
		if (columns < 1) columns = 1;
		ImGui::Columns(columns, nullptr, false);

		for (const GameObjectType& type : s_AvailableTypes)
		{
			ImGui::PushID(type.typeId);

			if (ImGui::Button(type.displayName, ImVec2(thumbSize, thumbSize)))
			{
			}

			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(s_GameObjectPayload, type.typeId, strlen(type.typeId) + 1);
				s_DraggingType = type.typeId;
				ImGui::Text("%s", type.displayName);
				ImGui::EndDragDropSource();
			}

			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", type.description);

			ImGui::TextWrapped("%s", type.displayName);
			ImGui::PopID();
			ImGui::NextColumn();
		}

		ImGui::Columns(1);
		ImGui::End();

		if (!s_DraggingType.empty() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
			{
				s_PendingDropType = s_DraggingType;
				BZ_INFO("GameObject type dropped on scene: {0}", s_PendingDropType);
			}
			s_DraggingType.clear();
		}
	}

	bool PropertiesWindow::ConsumePendingDrop(std::string& outType)
	{
		if (s_PendingDropType.empty())
			return false;

		outType = s_PendingDropType;
		s_PendingDropType.clear();
		return true;
	}

	void PropertiesWindow::SetSize(float w, float h)
	{
		width = w;
		height = h;
	}
	
	bool StartWindow::s_Playing = false;

	StartWindow::StartWindow()
		: onFocus(false)
	{
	}

	StartWindow::~StartWindow()
	{
	}

	bool StartWindow::IsPlaying()
	{
		return s_Playing;
	}

	void StartWindow::SetPlaying(bool playing)
	{
		s_Playing = playing;
	}

	void StartWindow::OnUpdate()
	{
		if (!m_Open)
			return;

		ImGui::SetNextWindowSize(ImVec2(width, height));

		if (!ImGui::Begin("Toolbar", &m_Open,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImGui::End();
			return;
		}
		onFocus = ImGui::IsWindowFocused();

		if (ImGui::Button(s_Playing ? "Stop" : "Start"))
			s_Playing = !s_Playing;

		ImGui::End();
	}

	void StartWindow::SetSize(float w, float h)
	{
		width = w;
		height = h;
	}

	unsigned int GameWindow::s_Framebuffer = 0;
	unsigned int GameWindow::s_TextureId = 0;
	int GameWindow::s_FrameWidth = 1280;
	int GameWindow::s_FrameHeight = 720;

	GameWindow::GameWindow()
	{
	}

	GameWindow::~GameWindow()
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

	void GameWindow::BeginCapture()
	{
		if (s_Framebuffer == 0)
		{
			glGenFramebuffers(1, &s_Framebuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, s_Framebuffer);

			glGenTextures(1, &s_TextureId);
			glBindTexture(GL_TEXTURE_2D, s_TextureId);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, s_FrameWidth, s_FrameHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_TextureId, 0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		else
		{
			glBindFramebuffer(GL_FRAMEBUFFER, s_Framebuffer);
		}

		glViewport(0, 0, s_FrameWidth, s_FrameHeight);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void GameWindow::EndCapture(int windowWidth, int windowHeight)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, windowWidth, windowHeight);
	}

	void GameWindow::OnUpdate()
	{
		if (!StartWindow::IsPlaying())
			return;

		ImGui::SetNextWindowSize(ImVec2(960.0f, 570.0f), ImGuiCond_FirstUseEver);

		bool open = true;
		if (!ImGui::Begin("Game", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
		{
			ImGui::End();
			if (!open)
				StartWindow::SetPlaying(false);
			return;
		}

		if (s_TextureId != 0)
		{
			ImVec2 avail = ImGui::GetContentRegionAvail();
			float frameAspect = (float)s_FrameWidth / (float)s_FrameHeight;
			ImVec2 size(avail.x, avail.x / frameAspect);
			if (size.y > avail.y)
			{
				size.y = avail.y;
				size.x = avail.y * frameAspect;
			}

			ImVec2 cursor = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cursor.x + (avail.x - size.x) * 0.5f,
									   cursor.y + (avail.y - size.y) * 0.5f));

			ImGui::Image((ImTextureID)(intptr_t)s_TextureId, size, ImVec2(0, 1), ImVec2(1, 0));
		}
		else
		{
			ImGui::TextDisabled("Waiting for the first frame...");
		}

		ImGui::End();

		if (!open)
			StartWindow::SetPlaying(false);
	}
}
