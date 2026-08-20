#pragma once
#include "Blaze/Core/Core.h"

namespace Blaze
{
	class BLAZE_API SceneWindow
	{
	public:
		SceneWindow();
		virtual ~SceneWindow();

		void OnUpdate();

		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }

		static void EnsureSize(int width, int height);
		static void BeginCapture();
		static void EndCapture(int windowWidth, int windowHeight);

		static unsigned int GetColorTexture() { return s_TextureId; }
		static unsigned int GetFramebuffer() { return s_Framebuffer; }
		static int GetFrameWidth() { return s_FrameWidth; }
		static int GetFrameHeight() { return s_FrameHeight; }

		static int GetDesiredWidth() { return s_DesiredWidth; }
		static int GetDesiredHeight() { return s_DesiredHeight; }

		static bool IsMouseOverSceneView();
		static bool WindowToScenePixel(float windowX, float windowY, float& outSceneX, float& outSceneY);
		static bool TryGetMouseScenePixel(float& outSceneX, float& outSceneY);

		static bool WantsEditorInput();

	private:
		bool m_Open = true;

		static unsigned int s_Framebuffer;
		static unsigned int s_TextureId;
		static int s_FrameWidth;
		static int s_FrameHeight;
		static int s_DesiredWidth;
		static int s_DesiredHeight;
		static float s_ImageMinX;
		static float s_ImageMinY;
		static float s_ImageMaxX;
		static float s_ImageMaxY;
		static bool s_Hovered;
	};
}
