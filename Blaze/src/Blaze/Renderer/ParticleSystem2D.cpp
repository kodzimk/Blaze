#include "ParticleSystem2D.h"
#include "Blaze/Renderer/Camera.h"
#include "Blaze/Renderer/Gizmo.h"
#include <glad/glad.h>
#include <cmath>
#include <cstdlib>

namespace Blaze
{
	std::vector<ParticleSystem2D::Particle> ParticleSystem2D::s_Particles;
	int ParticleSystem2D::s_MaxParticles = 2048;

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

		// Recycle oldest.
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

		p->position = params.position;
		p->velocity = params.velocity;
		p->acceleration = params.acceleration;
		p->maxLife = glm::max(params.life, 0.01f);
		p->life = p->maxLife;
		p->sizeStart = glm::max(params.sizeStart, 0.5f);
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

		GLboolean blendWas = glIsEnabled(GL_BLEND);
		GLint srcRGB = GL_SRC_ALPHA;
		GLint dstRGB = GL_ONE_MINUS_SRC_ALPHA;
		if (blendWas)
		{
			glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
			glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
		}
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

			s_Drawer.DrawParticle(camera, p.position, size, color, p.rotation);
		}

		glBlendFunc(srcRGB, dstRGB);
		if (!blendWas)
			glDisable(GL_BLEND);
	}

	void ParticleSystem2D::Clear()
	{
		for (Particle& p : s_Particles)
			p.alive = false;
		s_Particles.clear();
	}
}
