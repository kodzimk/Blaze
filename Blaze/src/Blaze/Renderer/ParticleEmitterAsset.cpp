#include "ParticleEmitterAsset.h"
#include "ParticleSystem2D.h"
#include "Blaze/Logging/Log.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cctype>

namespace Blaze
{
	float ParticleEmitterAsset::RandRange(float a, float b)
	{
		if (b < a)
			std::swap(a, b);
		const float t = (float)std::rand() / (float)RAND_MAX;
		return a + (b - a) * t;
	}

	void ParticleEmitterAsset::EmitOne(float x, float y) const
	{
		const float halfSpread = glm::max(m_Spread, 0.0f) * 0.5f;
		const float angleDeg = m_Direction + RandRange(-halfSpread, halfSpread);
		const float angle = glm::radians(angleDeg);
		const float speed = RandRange(m_SpeedMin, m_SpeedMax);

		ParticleEmitParams params;
		params.position = glm::vec2(x, y);
		params.velocity = glm::vec2(std::cos(angle) * speed, std::sin(angle) * speed);
		params.acceleration = m_Gravity;
		const float lifeJitter = 1.0f + RandRange(-m_LifeVariance, m_LifeVariance);
		params.life = glm::max(m_Life * lifeJitter, 0.01f);
		params.sizeStart = glm::max(m_SizeStart * RandRange(0.75f, 1.25f), 0.5f);
		params.sizeEnd = glm::max(m_SizeEnd, 0.0f);
		params.colorStart = m_ColorStart;
		params.colorEnd = m_ColorEnd;
		params.angularVelocity = RandRange(m_AngularVelMin, m_AngularVelMax);
		ParticleSystem2D::Emit(params);
	}

	void ParticleEmitterAsset::PlayAt(float x, float y) const
	{
		if (m_Mode == ParticleEmitMode::Continuous)
		{
			const int n = glm::clamp((int)std::ceil(glm::max(m_Rate, 1.0f) * 0.15f), 1, 64);
			for (int i = 0; i < n; ++i)
				EmitOne(x, y);
			return;
		}

		const int n = glm::clamp(m_Count, 0, 512);
		for (int i = 0; i < n; ++i)
			EmitOne(x, y);
	}

	void ParticleEmitterAsset::EmitContinuous(float x, float y, float dt, float& ioAccumulator) const
	{
		if (dt <= 0.0f)
			return;
		ioAccumulator += glm::max(m_Rate, 0.0f) * dt;
		int n = 0;
		while (ioAccumulator >= 1.0f && n < 128)
		{
			EmitOne(x, y);
			ioAccumulator -= 1.0f;
			++n;
		}
	}

	bool ParticleEmitterAsset::LoadFromFile(const std::string& path)
	{
		std::ifstream in(path);
		if (!in.is_open())
		{
			BZ_CORE_ERROR("Failed to open particle asset: {0}", path);
			return false;
		}

		*this = CreateDefault();
		m_Path = path;

		std::string line;
		bool headerOk = false;
		while (std::getline(in, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty() || line[0] == '#')
				continue;

			std::istringstream stream(line);
			std::string key;
			stream >> key;
			if (key == "BlazeParticle")
			{
				headerOk = true;
				continue;
			}
			if (!headerOk)
				continue;

			if (key == "name")
			{
				std::getline(stream, m_Name);
				if (!m_Name.empty() && m_Name[0] == ' ')
					m_Name.erase(m_Name.begin());
			}
			else if (key == "mode")
			{
				std::string mode;
				stream >> mode;
				for (char& c : mode)
					c = (char)std::tolower((unsigned char)c);
				m_Mode = (mode == "continuous" || mode == "loop")
					? ParticleEmitMode::Continuous
					: ParticleEmitMode::Burst;
			}
			else if (key == "count") { stream >> m_Count; }
			else if (key == "speedmin") { stream >> m_SpeedMin; }
			else if (key == "speedmax") { stream >> m_SpeedMax; }
			else if (key == "life") { stream >> m_Life; }
			else if (key == "lifevariance") { stream >> m_LifeVariance; }
			else if (key == "sizestart") { stream >> m_SizeStart; }
			else if (key == "sizeend") { stream >> m_SizeEnd; }
			else if (key == "colorstart") { stream >> m_ColorStart.r >> m_ColorStart.g >> m_ColorStart.b >> m_ColorStart.a; }
			else if (key == "colorend") { stream >> m_ColorEnd.r >> m_ColorEnd.g >> m_ColorEnd.b >> m_ColorEnd.a; }
			else if (key == "gravity") { stream >> m_Gravity.x >> m_Gravity.y; }
			else if (key == "gravityy") { stream >> m_Gravity.y; m_Gravity.x = 0.0f; }
			else if (key == "angularvelmin") { stream >> m_AngularVelMin; }
			else if (key == "angularvelmax") { stream >> m_AngularVelMax; }
			else if (key == "rate") { stream >> m_Rate; }
			else if (key == "direction") { stream >> m_Direction; }
			else if (key == "spread") { stream >> m_Spread; }
		}

		if (!headerOk)
		{
			BZ_CORE_ERROR("Invalid particle header: {0}", path);
			return false;
		}
		return true;
	}

	bool ParticleEmitterAsset::SaveToFile(const std::string& path) const
	{
		std::ofstream out(path);
		if (!out.is_open())
		{
			BZ_CORE_ERROR("Failed to save particle asset: {0}", path);
			return false;
		}

		out << "BlazeParticle 1\n";
		out << "name " << (m_Name.empty() ? "Particle" : m_Name) << "\n";
		out << "mode " << (m_Mode == ParticleEmitMode::Continuous ? "continuous" : "burst") << "\n";
		out << "count " << m_Count << "\n";
		out << "speedmin " << m_SpeedMin << "\n";
		out << "speedmax " << m_SpeedMax << "\n";
		out << "life " << m_Life << "\n";
		out << "lifevariance " << m_LifeVariance << "\n";
		out << "sizestart " << m_SizeStart << "\n";
		out << "sizeend " << m_SizeEnd << "\n";
		out << "colorstart " << m_ColorStart.r << " " << m_ColorStart.g << " " << m_ColorStart.b << " " << m_ColorStart.a << "\n";
		out << "colorend " << m_ColorEnd.r << " " << m_ColorEnd.g << " " << m_ColorEnd.b << " " << m_ColorEnd.a << "\n";
		out << "gravity " << m_Gravity.x << " " << m_Gravity.y << "\n";
		out << "angularvelmin " << m_AngularVelMin << "\n";
		out << "angularvelmax " << m_AngularVelMax << "\n";
		out << "rate " << m_Rate << "\n";
		out << "direction " << m_Direction << "\n";
		out << "spread " << m_Spread << "\n";
		return true;
	}

	ParticleEmitterAsset ParticleEmitterAsset::CreateDefault(const std::string& name)
	{
		ParticleEmitterAsset asset;
		asset.SetName(name.empty() ? "Particle" : name);
		asset.SetMode(ParticleEmitMode::Continuous);
		asset.SetRate(40.0f);
		asset.SetCount(18);
		asset.SetSpeedMin(80.0f);
		asset.SetSpeedMax(220.0f);
		asset.SetLife(0.55f);
		asset.SetSizeStart(10.0f);
		asset.SetSizeEnd(0.0f);
		asset.SetColorStart(glm::vec4(1.0f, 0.85f, 0.25f, 1.0f));
		asset.SetColorEnd(glm::vec4(0.5f, 0.2f, 0.05f, 0.0f));
		asset.SetGravity(glm::vec2(0.0f, -400.0f));
		asset.SetDirection(90.0f);
		asset.SetSpread(360.0f);
		return asset;
	}

	ParticleEmitterAsset ParticleEmitterAsset::CreateFire(const std::string& name)
	{
		ParticleEmitterAsset asset = CreateDefault(name.empty() ? "Fire" : name);
		asset.SetMode(ParticleEmitMode::Continuous);
		asset.SetRate(55.0f);
		asset.SetCount(24);
		asset.SetSpeedMin(40.0f);
		asset.SetSpeedMax(120.0f);
		asset.SetLife(0.7f);
		asset.SetSizeStart(14.0f);
		asset.SetSizeEnd(2.0f);
		asset.SetColorStart(glm::vec4(1.0f, 0.75f, 0.15f, 1.0f));
		asset.SetColorEnd(glm::vec4(1.0f, 0.15f, 0.02f, 0.0f));
		asset.SetGravity(glm::vec2(0.0f, 180.0f));
		asset.SetDirection(90.0f);
		asset.SetSpread(50.0f);
		asset.SetAngularVelMin(-90.0f);
		asset.SetAngularVelMax(90.0f);
		return asset;
	}

	ParticleEmitterAsset ParticleEmitterAsset::CreateSpark(const std::string& name)
	{
		ParticleEmitterAsset asset = CreateDefault(name.empty() ? "Spark" : name);
		asset.SetMode(ParticleEmitMode::Burst);
		asset.SetCount(28);
		asset.SetSpeedMin(150.0f);
		asset.SetSpeedMax(420.0f);
		asset.SetLife(0.4f);
		asset.SetSizeStart(6.0f);
		asset.SetSizeEnd(0.0f);
		asset.SetColorStart(glm::vec4(1.0f, 0.95f, 0.6f, 1.0f));
		asset.SetColorEnd(glm::vec4(1.0f, 0.4f, 0.05f, 0.0f));
		asset.SetGravity(glm::vec2(0.0f, -600.0f));
		asset.SetSpread(360.0f);
		return asset;
	}
}
