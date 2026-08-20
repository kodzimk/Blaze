#pragma once
#include "Blaze/Core/Core.h"
#include <vector>

namespace Blaze
{
	class GameObject;

	class BLAZE_API PostProcess2D
	{
	public:
		static GameObject* FindActive(const std::vector<GameObject*>& objects);

		static void Apply(const std::vector<GameObject*>& objects,
			unsigned int srcTexture, unsigned int destFramebuffer, int width, int height);

		static void BeginSceneCapture(int width, int height);
		static void EndSceneCapturePresent(const std::vector<GameObject*>& objects,
			int windowWidth, int windowHeight);

		static void Shutdown();

	private:
		static void EnsureResources(int width, int height);
		static void DrawFullscreen(unsigned int srcTexture, bool grade, GameObject* settings);
		static void BlitTexture(unsigned int srcTexture, unsigned int destFramebuffer, int width, int height);
	};
}
