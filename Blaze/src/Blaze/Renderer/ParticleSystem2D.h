#pragma once
#include "Blaze/Core/Core.h"
#include "glm.hpp"
#include <string>
#include <vector>

namespace Blaze
{
	class Camera;
	class GameObject;

	struct ParticleEmitParams
	{
		glm::vec2 position{ 0.0f };
		glm::vec2 velocity{ 0.0f };
		glm::vec2 acceleration{ 0.0f, -400.0f }; // mild gravity
		float life = 0.6f;
		float sizeStart = 12.0f;
		float sizeEnd = 2.0f;
		glm::vec4 colorStart{ 1.0f, 0.85f, 0.25f, 1.0f };
		glm::vec4 colorEnd{ 1.0f, 0.2f, 0.05f, 0.0f };
		float rotation = 0.0f;
		float angularVelocity = 0.0f;
	};

	class BLAZE_API ParticleSystem2D
	{
	public:
		static void Emit(const ParticleEmitParams& params);
		static void Emit(float x, float y, float vx, float vy, float life, float size,
			float r = 1.0f, float g = 0.8f, float b = 0.2f, float a = 1.0f);

		// Random burst around a point (speed in pixels/sec).
		static void Burst(float x, float y, int count,
			float speedMin, float speedMax, float life, float size,
			float r = 1.0f, float g = 0.8f, float b = 0.2f, float a = 1.0f,
			float gravityY = -400.0f);

		static void SetMaxParticles(int count);
		static int GetAliveCount();

		static void Update(float dt);
		static void Flush(const Camera& camera);
		static void Clear();

		// Scene Particle Emitter component.
		// Continuous assets keep emitting until StopSceneEmitter / play stop.
		// Burst: fires once, or loops while previewLoop is true (editor Play).
		static bool StartSceneEmitter(GameObject* object, bool previewLoop = true);
		static void StopSceneEmitter(GameObject* object);
		static bool IsSceneEmitterActive(GameObject* object);
		static void UpdateSceneEmitters(float dt);

		static void OnPlayModeStart(const std::vector<GameObject*>& objects);
		static void OnPlayModeUpdate(float dt);
		static void OnPlayModeStop();
		static void RemoveObject(GameObject* object);

	private:
		struct Particle
		{
			glm::vec2 position{ 0.0f };
			glm::vec2 velocity{ 0.0f };
			glm::vec2 acceleration{ 0.0f };
			float life = 0.0f;
			float maxLife = 1.0f;
			float sizeStart = 8.0f;
			float sizeEnd = 0.0f;
			glm::vec4 colorStart{ 1.0f };
			glm::vec4 colorEnd{ 1.0f, 1.0f, 1.0f, 0.0f };
			float rotation = 0.0f;
			float angularVelocity = 0.0f;
			bool alive = false;
		};

		static Particle* AllocParticle();
		static float RandRange(float a, float b);

		static std::vector<Particle> s_Particles;
		static int s_MaxParticles;
	};
}
