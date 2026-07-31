#pragma once
#include"Blaze/Core/Core.h"
#include"bzpch.h"

namespace Blaze
{
	class GameObject;

	class BLAZE_API StartWindow
	{
	public:
		StartWindow();
		virtual ~StartWindow();

		void OnUpdate();
		bool OnFocus() const { return onFocus; }
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }
		static void SetSize(float width, float height);

		static bool IsPlaying();
		static void SetPlaying(bool playing);
	private:
		bool onFocus;
		bool m_Open = true;
		static bool s_Playing;
	public:
		static float width;
		static float height;
	};

	class BLAZE_API GameWindow
	{
	public:
		GameWindow();
		virtual ~GameWindow();

		void OnUpdate();
		// Call after all ImGui windows so nothing overrides the cursor this frame.
		static void ApplyPlayCursor();

		static void BeginCapture();
		static void EndCapture(int windowWidth, int windowHeight);

		static int GetFrameWidth() { return s_FrameWidth; }
		static int GetFrameHeight() { return s_FrameHeight; }
		static bool WindowToGamePixel(float windowX, float windowY, float& outGameX, float& outGameY);
		static bool IsMouseOverGameView();
		// Maps current ImGui mouse into game framebuffer pixels (OpenGL Y-up). False if not over the Game image.
		static bool TryGetMouseGamePixel(float& outGameX, float& outGameY);
	private:
		static unsigned int s_Framebuffer;
		static unsigned int s_TextureId;
		static int s_FrameWidth;
		static int s_FrameHeight;
		static float s_ImageMinX;
		static float s_ImageMinY;
		static float s_ImageMaxX;
		static float s_ImageMaxY;
		static bool s_HideCursor;
	};

	class BLAZE_API PropertiesWindow
	{
	public:
		PropertiesWindow();
		virtual ~PropertiesWindow();

		void OnUpdate();
		bool OnFocus() const { return onFocus; }
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }
		static void SetSize(float width, float height);

		static bool ConsumePendingDrop(std::string& outType);
	private:
		bool onFocus;
		bool m_Open = true;

		static std::string s_DraggingType;
		static std::string s_PendingDropType;
	public:
		static float width;
		static float height;
	};
}
