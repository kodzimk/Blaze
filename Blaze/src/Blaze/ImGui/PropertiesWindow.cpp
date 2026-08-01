#include "PropertiesWindow.h"

#include<glad/glad.h>
#include "../vendor/imgui/src/imgui.h"   
#include"../vendor/imgui/include/imgui_impl_glfw.h"
#include"../vendor/imgui/include/imgui_impl_opengl3.h"

#include"bzpch.h"
#include"Blaze/Logging/Log.h"
#include"Blaze/Core/Application.h"
#include"Blaze/Renderer/Gizmo.h"
#include"Blaze/Scripting/OnScreenDebug.h"
#include"Blaze/Scripting/ScriptEngine.h"
#include"Blaze/UI/UISystem.h"
#include "GLFW/glfw3.h"
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
		{ "Text", "text", "World-space text label" },
		{ "Tilemap", "tilemap", "Tileset grid map" },
		{ "Circle", "circle", "Circle collider object" },
		{ "Box",    "box",    "Box collider object" },
		{ "Camera", "camera", "Scene / game camera" },
		{ "Sound",  "sound",  "Audio source object" },
		{ "Light",  "light",  "2D point light" },
		{ "Spot Light", "spotlight", "2D cone / spot light" },
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

			const bool isSound = (strcmp(type.typeId, "sound") == 0);
			const bool isCamera = (strcmp(type.typeId, "camera") == 0);
			const bool isPointLight = (strcmp(type.typeId, "light") == 0);
			const bool isSpotLight = (strcmp(type.typeId, "spotlight") == 0);
			unsigned int iconTex = 0;
			if (isSound)
				iconTex = Gizmo2D::GetSoundIconTextureID();
			else if (isCamera)
				iconTex = Gizmo2D::GetCameraIconTextureID();
			else if (isPointLight)
				iconTex = Gizmo2D::GetPointLightIconTextureID();
			else if (isSpotLight)
				iconTex = Gizmo2D::GetSpotLightIconTextureID();

			if (iconTex != 0)
			{
				ImGui::ImageButton("##type", (ImTextureID)(intptr_t)iconTex,
					ImVec2(thumbSize, thumbSize), ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0.12f, 0.12f, 0.12f, 1.0f), ImVec4(1, 1, 1, 1));
			}
			else if (ImGui::Button(type.displayName, ImVec2(thumbSize, thumbSize)))
			{
			}

			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(s_GameObjectPayload, type.typeId, strlen(type.typeId) + 1);
				s_DraggingType = type.typeId;
				if (iconTex != 0)
					ImGui::Image((ImTextureID)(intptr_t)iconTex, ImVec2(32.0f, 32.0f));
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
	float GameWindow::s_ImageMinX = 0.0f;
	float GameWindow::s_ImageMinY = 0.0f;
	float GameWindow::s_ImageMaxX = 0.0f;
	float GameWindow::s_ImageMaxY = 0.0f;
	bool GameWindow::s_HideCursor = false;

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
		{
			if (s_HideCursor)
			{
				s_HideCursor = false;
				GLFWwindow* glfwWindow = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
				glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(960.0f, 570.0f), ImGuiCond_FirstUseEver);

		bool open = true;
		if (!ImGui::Begin("Game", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
		{
			s_HideCursor = false;
			ImGui::End();
			if (!open)
				StartWindow::SetPlaying(false);
			return;
		}

		bool overImage = false;
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
			{
				const ImVec2 mn = ImGui::GetItemRectMin();
				const ImVec2 mx = ImGui::GetItemRectMax();
				s_ImageMinX = mn.x;
				s_ImageMinY = mn.y;
				s_ImageMaxX = mx.x;
				s_ImageMaxY = mx.y;
			}
			overImage = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
				|| IsMouseOverGameView();

			// UI input must run here: image rect is current and mouse is in ImGui space.
			{
				float gx = 0.0f, gy = 0.0f;
				const bool over = TryGetMouseGamePixel(gx, gy);
				const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
				const bool pressed = over && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
				const bool released = over && ImGui::IsMouseReleased(ImGuiMouseButton_Left);

				if (!UISystem::IsReady())
					UISystem::Init();
				UISystem::UpdateInput(
					s_FrameWidth, s_FrameHeight,
					over ? gx : -1.0f, over ? gy : -1.0f,
					over && mouseDown, pressed, released);
			}

			OnScreenDebug::DrawOverItem();
		}
		else
		{
			ImGui::TextDisabled("Waiting for the first frame...");
			s_ImageMinX = s_ImageMinY = s_ImageMaxX = s_ImageMaxY = 0.0f;
		}

		const bool overWindow = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows
			| ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		// Hide while the pointer is in the Game view, or while the Game window is focused after a click.
		s_HideCursor = overImage || (focused && overWindow);

		ImGui::End();

		if (!open)
			StartWindow::SetPlaying(false);
	}

	void GameWindow::ApplyPlayCursor()
	{
		GLFWwindow* glfwWindow = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		if (!glfwWindow)
			return;

		if (StartWindow::IsPlaying() && s_HideCursor && !ScriptEngine::WantsCursorVisible())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_None);
			glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
		}
		else if (StartWindow::IsPlaying())
		{
			glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}

	bool GameWindow::IsMouseOverGameView()
	{
		if (s_ImageMaxX <= s_ImageMinX || s_ImageMaxY <= s_ImageMinY)
			return false;
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		return mouse.x >= s_ImageMinX && mouse.x <= s_ImageMaxX
			&& mouse.y >= s_ImageMinY && mouse.y <= s_ImageMaxY;
	}

	bool GameWindow::WindowToGamePixel(float windowX, float windowY, float& outGameX, float& outGameY)
	{
		const float dw = s_ImageMaxX - s_ImageMinX;
		const float dh = s_ImageMaxY - s_ImageMinY;
		if (dw <= 1.0f || dh <= 1.0f)
			return false;
		// UISystem uses top-left pixel space (ortho 0,W,H,0). ImGui image Y also grows downward.
		outGameX = (windowX - s_ImageMinX) / dw * (float)s_FrameWidth;
		outGameY = (windowY - s_ImageMinY) / dh * (float)s_FrameHeight;
		return true;
	}

	bool GameWindow::TryGetMouseGamePixel(float& outGameX, float& outGameY)
	{
		if (!IsMouseOverGameView())
			return false;
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		return WindowToGamePixel(mouse.x, mouse.y, outGameX, outGameY);
	}
}
