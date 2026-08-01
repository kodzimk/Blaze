#include "ParticleSystem2D.h"
#include "ParticleEmitterAsset.h"
#include "Blaze/Renderer/Camera.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Gizmo.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Logging/Log.h"
#include <glad/glad.h>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <filesystem>

namespace Blaze
{
	std::vector<ParticleSystem2D::Particle> ParticleSystem2D::s_Particles;
	int ParticleSystem2D::s_MaxParticles = 2048;

	struct SceneParticleEmitter
	{
		ParticleEmitterAsset asset;
		float accum = 0.0f;
		float burstCooldown = 0.0f;
		bool continuous = false;
		bool loopBurst = false;
	};

	static std::unordered_map<GameObject*, SceneParticleEmitter> s_SceneEmitters;
	static GameObject* s_QueuedPlay = nullptr;
	static GameObject* s_QueuedStop = nullptr;
	static bool s_QueuedPlayPreviewLoop = true;

	static bool LoadEmitterForObject(GameObject* object, ParticleEmitterAsset& out)
	{
		if (!object)
			return false;
		const std::string& path = object->GetParticlePath();
		if (path.empty())
			return false;

		if (out.LoadFromFile(path))
			return true;

		namespace fs = std::filesystem;
		std::error_code ec;
		if (Project::HasProject())
		{
			fs::path full = fs::path(Project::GetDirectory()) / path;
			if (fs::exists(full, ec) && out.LoadFromFile(full.generic_string()))
				return true;
			full = fs::path(Project::GetContentDirectory()) / fs::path(path).filename();
			if (fs::exists(full, ec) && out.LoadFromFile(full.generic_string()))
				return true;
		}

		BZ_CORE_ERROR("Failed to load particle asset: {0}", path);
		return false;
	}

	float ParticleSystem2D::RandRange(float a, float b)
	{
		if (b < a)
			std::swap(a, b);
		const float t = (float)std::rand() / (float)RAND_MAX;
		return a + (b - a) * t;
	}

	ParticleSystem2D::Particle* ParticleSystem2D::AllocParticle()
	{
		if ((int)s_Particles.size() < s_MaxParticles)
		{
			s_Particles.push_back(Particle{});
			return &s_Particles.back();
		}

		for (Particle& p : s_Particles)
		{
			if (!p.alive)
				return &p;
		}

		Particle* oldest = &s_Particles[0];
		for (Particle& p : s_Particles)
		{
			if (p.life < oldest->life)
				oldest = &p;
		}
		return oldest;
	}

	void ParticleSystem2D::SetMaxParticles(int count)
	{
		s_MaxParticles = glm::clamp(count, 16, 16384);
		if ((int)s_Particles.size() > s_MaxParticles)
			s_Particles.resize((size_t)s_MaxParticles);
	}

	int ParticleSystem2D::GetAliveCount()
	{
		int n = 0;
		for (const Particle& p : s_Particles)
		{
			if (p.alive)
				++n;
		}
		return n;
	}

	void ParticleSystem2D::Emit(const ParticleEmitParams& params)
	{
		Particle* p = AllocParticle();
		if (!p)
			return;

		// Positions are world (same as GetWorldPosition). Speed / gravity / size
		// are authored in mesh/pixel units — scale into world via ortho.
		const float sx = Camera::m_prop.projection[0][0];
		const float sy = Camera::m_prop.projection[1][1];

		p->position = params.position;
		p->velocity = params.velocity * glm::vec2(sx, sy);
		p->acceleration = params.acceleration * glm::vec2(sx, sy);
		p->maxLife = glm::max(params.life, 0.01f);
		p->life = p->maxLife;
		p->sizeStart = glm::max(params.sizeStart, 0.5f); // pixels; scaled in DrawParticle
		p->sizeEnd = glm::max(params.sizeEnd, 0.0f);
		p->colorStart = params.colorStart;
		p->colorEnd = params.colorEnd;
		p->rotation = params.rotation;
		p->angularVelocity = params.angularVelocity;
		p->alive = true;
	}

	void ParticleSystem2D::Emit(float x, float y, float vx, float vy, float life, float size,
		float r, float g, float b, float a)
	{
		ParticleEmitParams params;
		params.position = glm::vec2(x, y);
		params.velocity = glm::vec2(vx, vy);
		params.life = life;
		params.sizeStart = size;
		params.sizeEnd = size * 0.15f;
		params.colorStart = glm::vec4(r, g, b, a);
		params.colorEnd = glm::vec4(r, g, b, 0.0f);
		Emit(params);
	}

	void ParticleSystem2D::Burst(float x, float y, int count,
		float speedMin, float speedMax, float life, float size,
		float r, float g, float b, float a, float gravityY)
	{
		count = glm::clamp(count, 0, 512);
		for (int i = 0; i < count; ++i)
		{
			const float angle = RandRange(0.0f, 6.2831853f);
			const float speed = RandRange(speedMin, speedMax);
			ParticleEmitParams params;
			params.position = glm::vec2(x, y);
			params.velocity = glm::vec2(std::cos(angle) * speed, std::sin(angle) * speed);
			params.acceleration = glm::vec2(0.0f, gravityY);
			params.life = life * RandRange(0.7f, 1.15f);
			params.sizeStart = size * RandRange(0.75f, 1.25f);
			params.sizeEnd = 0.0f;
			params.colorStart = glm::vec4(r, g, b, a);
			params.colorEnd = glm::vec4(r * 0.5f, g * 0.35f, b * 0.2f, 0.0f);
			params.angularVelocity = RandRange(-360.0f, 360.0f);
			Emit(params);
		}
	}

	void ParticleSystem2D::Update(float dt)
	{
		if (dt <= 0.0f)
			return;

		for (Particle& p : s_Particles)
		{
			if (!p.alive)
				continue;

			p.life -= dt;
			if (p.life <= 0.0f)
			{
				p.alive = false;
				continue;
			}

			p.velocity += p.acceleration * dt;
			p.position += p.velocity * dt;
			p.rotation += p.angularVelocity * dt;
		}
	}

	void ParticleSystem2D::Flush(const Camera& camera)
	{
		static Gizmo2D s_Drawer;
		Flush(camera, s_Drawer);
	}

	void ParticleSystem2D::Flush(const Camera& camera, Gizmo2D& drawer)
	{
		GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
		GLboolean blendWas = glIsEnabled(GL_BLEND);
		GLint srcRGB = GL_SRC_ALPHA;
		GLint dstRGB = GL_ONE_MINUS_SRC_ALPHA;
		if (blendWas)
		{
			glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
			glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
		}
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		for (const Particle& p : s_Particles)
		{
			if (!p.alive)
				continue;

			const float t = 1.0f - (p.life / p.maxLife);
			const float size = glm::mix(p.sizeStart, p.sizeEnd, t);
			const glm::vec4 color = glm::mix(p.colorStart, p.colorEnd, t);
			if (size < 0.25f || color.a < 0.01f)
				continue;

			drawer.DrawParticle(camera, p.position, size, color, p.rotation);
		}

		glBlendFunc(srcRGB, dstRGB);
		if (!blendWas)
			glDisable(GL_BLEND);
		if (depthWas)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
	}

	void ParticleSystem2D::Clear()
	{
		for (Particle& p : s_Particles)
			p.alive = false;
		s_Particles.clear();
	}

	bool ParticleSystem2D::HasActiveSceneEmitters()
	{
		return !s_SceneEmitters.empty();
	}

	bool ParticleSystem2D::StartSceneEmitter(GameObject* object, bool previewLoop)
	{
		if (!object || !object->HasParticleEmitter())
			return false;

		ParticleEmitterAsset asset;
		if (!LoadEmitterForObject(object, asset))
			return false;

		const glm::vec3 world = object->GetWorldPosition();
		if (asset.GetMode() == ParticleEmitMode::Continuous)
		{
			SceneParticleEmitter slot;
			slot.asset = asset;
			slot.accum = 0.0f;
			slot.continuous = true;
			slot.loopBurst = false;
			s_SceneEmitters[object] = std::move(slot);
			// Seed immediately so Components Play is visible on the next flush.
			float& accum = s_SceneEmitters[object].accum;
			asset.EmitContinuous(world.x, world.y, 0.35f, accum);
			return true;
		}

		asset.PlayAt(world.x, world.y);
		if (previewLoop)
		{
			SceneParticleEmitter slot;
			slot.asset = asset;
			slot.continuous = false;
			slot.loopBurst = true;
			slot.burstCooldown = glm::max(asset.GetLife() * 1.25f, 0.35f);
			s_SceneEmitters[object] = std::move(slot);
		}
		else
		{
			s_SceneEmitters.erase(object);
		}
		return true;
	}

	void ParticleSystem2D::StopSceneEmitter(GameObject* object)
	{
		if (!object)
			return;
		s_SceneEmitters.erase(object);
	}

	bool ParticleSystem2D::IsSceneEmitterActive(GameObject* object)
	{
		return object && s_SceneEmitters.find(object) != s_SceneEmitters.end();
	}

	void ParticleSystem2D::UpdateSceneEmitters(float dt)
	{
		if (dt <= 0.0f || s_SceneEmitters.empty())
			return;

		for (auto& entry : s_SceneEmitters)
		{
			GameObject* object = entry.first;
			if (!object || !object->IsActive())
				continue;

			SceneParticleEmitter& slot = entry.second;
			const glm::vec3 world = object->GetWorldPosition();
			if (slot.continuous)
			{
				slot.asset.EmitContinuous(world.x, world.y, dt, slot.accum);
			}
			else if (slot.loopBurst)
			{
				slot.burstCooldown -= dt;
				if (slot.burstCooldown <= 0.0f)
				{
					slot.asset.PlayAt(world.x, world.y);
					slot.burstCooldown = glm::max(slot.asset.GetLife() * 1.25f, 0.35f);
				}
			}
		}
	}

	void ParticleSystem2D::QueuePlay(GameObject* object)
	{
		s_QueuedStop = nullptr;
		s_QueuedPlay = object;
		s_QueuedPlayPreviewLoop = true;
	}

	void ParticleSystem2D::QueueStop(GameObject* object)
	{
		s_QueuedPlay = nullptr;
		s_QueuedStop = object;
	}

	void ParticleSystem2D::ProcessQueuedCommands()
	{
		if (s_QueuedStop)
		{
			StopSceneEmitter(s_QueuedStop);
			s_QueuedStop = nullptr;
		}
		if (s_QueuedPlay)
		{
			GameObject* object = s_QueuedPlay;
			s_QueuedPlay = nullptr;
			if (!StartSceneEmitter(object, s_QueuedPlayPreviewLoop))
				BZ_CORE_ERROR("Particle Play failed (missing/invalid .particle path) on '{0}'",
					object ? object->GetName() : "?");
			else
				BZ_INFO("Particle Play: {0}", object->GetParticlePath());
		}
	}

	void ParticleSystem2D::OnPlayModeStart(const std::vector<GameObject*>& objects)
	{
		s_SceneEmitters.clear();
		for (GameObject* object : objects)
		{
			if (!object || !object->HasParticleEmitter() || !object->GetParticlePlayOnStart())
				continue;
			if (object->GetParticlePath().empty())
				continue;
			StartSceneEmitter(object, false);
		}
	}

	void ParticleSystem2D::OnPlayModeUpdate(float dt)
	{
		UpdateSceneEmitters(dt);
	}

	void ParticleSystem2D::OnPlayModeStop()
	{
		s_SceneEmitters.clear();
	}

	void ParticleSystem2D::RemoveObject(GameObject* object)
	{
		StopSceneEmitter(object);
	}
}
