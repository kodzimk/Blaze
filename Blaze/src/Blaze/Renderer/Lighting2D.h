#pragma once
#include "Blaze/Core/Core.h"
#include "glm.hpp"
#include <vector>

namespace Blaze
{
	class Shader;
	class GameObject;

	// Global ambient + per-frame upload of point lights into the sprite shader.
	class BLAZE_API Lighting2D
	{
	public:
		static constexpr int MaxLights = 16;

		static void SetAmbient(const glm::vec3& ambient);
		static void SetAmbient(float r, float g, float b);
		static glm::vec3 GetAmbient();

		// When false, sprites render unlit (albedo only). Default true with ambient (1,1,1).
		static void SetEnabled(bool enabled);
		static bool IsEnabled();

		static void Upload(Shader& shader, const std::vector<GameObject*>& objects);

	private:
		static glm::vec3 s_Ambient;
		static bool s_Enabled;
	};
}
