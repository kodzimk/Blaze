#pragma once
#include "Blaze/Core/Core.h"
#include "glm.hpp"
#include <string>

namespace Blaze
{
	enum class ParticleEmitMode
	{
		Burst = 0,
		Continuous = 1
	};

	// Saveable particle emitter preset (.particle / BlazeParticle 1).
	class BLAZE_API ParticleEmitterAsset
	{
	public:
		ParticleEmitterAsset() = default;

		const std::string& GetPath() const { return m_Path; }
		void SetPath(const std::string& path) { m_Path = path; }
		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		ParticleEmitMode GetMode() const { return m_Mode; }
		void SetMode(ParticleEmitMode mode) { m_Mode = mode; }

		int GetCount() const { return m_Count; }
		void SetCount(int count) { m_Count = count; }

		float GetSpeedMin() const { return m_SpeedMin; }
		float GetSpeedMax() const { return m_SpeedMax; }
		void SetSpeedMin(float v) { m_SpeedMin = v; }
		void SetSpeedMax(float v) { m_SpeedMax = v; }

		float GetLife() const { return m_Life; }
		void SetLife(float v) { m_Life = v; }
		float GetLifeVariance() const { return m_LifeVariance; }
		void SetLifeVariance(float v) { m_LifeVariance = v; }

		float GetSizeStart() const { return m_SizeStart; }
		float GetSizeEnd() const { return m_SizeEnd; }
		void SetSizeStart(float v) { m_SizeStart = v; }
		void SetSizeEnd(float v) { m_SizeEnd = v; }

		const glm::vec4& GetColorStart() const { return m_ColorStart; }
		const glm::vec4& GetColorEnd() const { return m_ColorEnd; }
		void SetColorStart(const glm::vec4& c) { m_ColorStart = c; }
		void SetColorEnd(const glm::vec4& c) { m_ColorEnd = c; }

		glm::vec2 GetGravity() const { return m_Gravity; }
		void SetGravity(const glm::vec2& g) { m_Gravity = g; }

		float GetAngularVelMin() const { return m_AngularVelMin; }
		float GetAngularVelMax() const { return m_AngularVelMax; }
		void SetAngularVelMin(float v) { m_AngularVelMin = v; }
		void SetAngularVelMax(float v) { m_AngularVelMax = v; }

		float GetRate() const { return m_Rate; }
		void SetRate(float v) { m_Rate = v; }

		float GetDirection() const { return m_Direction; }
		void SetDirection(float degrees) { m_Direction = degrees; }
		float GetSpread() const { return m_Spread; }
		void SetSpread(float degrees) { m_Spread = degrees; }

		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;

		// One-shot: Burst mode fires count particles; Continuous fires ~rate particles once.
		void PlayAt(float x, float y) const;
		// Continuous helper: emit rate * dt particles (use in OnUpdate).
		void EmitContinuous(float x, float y, float dt, float& ioAccumulator) const;

		static ParticleEmitterAsset CreateDefault(const std::string& name = "Particle");
		static ParticleEmitterAsset CreateFire(const std::string& name = "Fire");
		static ParticleEmitterAsset CreateSpark(const std::string& name = "Spark");

	private:
		void EmitOne(float x, float y) const;
		static float RandRange(float a, float b);

		std::string m_Path;
		std::string m_Name = "Particle";
		ParticleEmitMode m_Mode = ParticleEmitMode::Burst;
		int m_Count = 18;
		float m_SpeedMin = 80.0f;
		float m_SpeedMax = 220.0f;
		float m_Life = 0.55f;
		float m_LifeVariance = 0.25f; // fraction of life randomized
		float m_SizeStart = 10.0f;
		float m_SizeEnd = 0.0f;
		glm::vec4 m_ColorStart{ 1.0f, 0.85f, 0.25f, 1.0f };
		glm::vec4 m_ColorEnd{ 0.5f, 0.2f, 0.05f, 0.0f };
		glm::vec2 m_Gravity{ 0.0f, -400.0f };
		float m_AngularVelMin = -360.0f;
		float m_AngularVelMax = 360.0f;
		float m_Rate = 40.0f;       // continuous particles / second
		float m_Direction = 90.0f;  // degrees (0 = +X, 90 = +Y)
		float m_Spread = 360.0f;    // full circle by default
	};
}
