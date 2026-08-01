#pragma once
#include "Blaze/Core/Core.h"
#include <vector>

namespace Blaze
{
	class GameObject;

	// Fullscreen color-grade from the first active PostProcess GameObject.
	class BLAZE_API PostProcess2D
	{
	public:
		static GameObject* FindActive(const std::vector<GameObject*>& objects);

		// Grade srcTex into destFbo (no-op if no active PostProcess).
		// When src is attached to destFbo, copies through an internal ping texture first.
		static void Apply(const std::vector<GameObject*>& objects,
			unsigned int srcTexture, unsigned int destFramebuffer, int width, int height);

		// Offscreen path (editor Scene + Runtime): capture scene here, then present to FB 0.
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
