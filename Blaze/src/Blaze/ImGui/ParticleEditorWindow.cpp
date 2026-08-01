#include "ParticleEditorWindow.h"
#include "../vendor/imgui/src/imgui.h"
#include "Blaze/Logging/Log.h"
#include "Blaze/Core/Time.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdlib>

namespace Blaze
{
	ParticleEditorWindow* ParticleEditorWindow::s_Instance = nullptr;

	ParticleEditorWindow::ParticleEditorWindow()
	{
		s_Instance = this;
	}

	ParticleEditorWindow::~ParticleEditorWindow()
	{
		if (s_Instance == this)
			s_Instance = nullptr;
	}

	void ParticleEditorWindow::OpenFile(const std::string& path)
	{
		if (s_Instance)
			s_Instance->Open(path);
	}

	bool ParticleEditorWindow::IsBlockingMainEditor()
	{
		return s_Instance && s_Instance->m_Open && s_Instance->m_WindowFocused;
	}

	float ParticleEditorWindow::RandRange(float a, float b)
	{
		if (b < a)
			std::swap(a, b);
		const float t = (float)std::rand() / (float)RAND_MAX;
		return a + (b - a) * t;
	}

	void ParticleEditorWindow::Open(const std::string& path)
	{
		if (path.empty())
			return;
		if (m_Dirty && !m_Asset.GetPath().empty() && m_Asset.GetPath() != path)
			SaveFile();

		if (!m_Asset.LoadFromFile(path))
		{
			BZ_INFO("Failed to open particle asset: {0}", path);
			return;
		}

		std::snprintf(m_NameBuf, sizeof(m_NameBuf), "%s", m_Asset.GetName().c_str());
		m_Dirty = false;
		m_Open = true;
		ClearPreview();
		PlayPreviewBurst();
	}

	bool ParticleEditorWindow::SaveFile()
	{
		if (m_Asset.GetPath().empty())
			return false;
		m_Asset.SetName(m_NameBuf);
		if (!m_Asset.SaveToFile(m_Asset.GetPath()))
			return false;
		m_Dirty = false;
		BZ_INFO("Saved particle asset: {0}", m_Asset.GetPath());
		return true;
	}

	void ParticleEditorWindow::ClearPreview()
	{
		for (PreviewParticle& p : m_Preview)
			p.alive = false;
		m_EmitAccum = 0.0f;
		m_BurstCooldown = 0.0f;
	}

	void ParticleEditorWindow::SpawnPreviewOne(float originX, float originY)
	{
		PreviewParticle* slot = nullptr;
		for (PreviewParticle& p : m_Preview)
		{
			if (!p.alive)
			{
				slot = &p;
				break;
			}
		}
		if (!slot)
		{
			if ((int)m_Preview.size() >= s_MaxPreview)
			{
				slot = &m_Preview[0];
				for (PreviewParticle& p : m_Preview)
				{
					if (p.life < slot->life)
						slot = &p;
				}
			}
			else
			{
				m_Preview.push_back(PreviewParticle{});
				slot = &m_Preview.back();
			}
		}

		const float halfSpread = glm::max(m_Asset.GetSpread(), 0.0f) * 0.5f;
		const float angleDeg = m_Asset.GetDirection() + RandRange(-halfSpread, halfSpread);
		const float angle = glm::radians(angleDeg);
		const float speed = RandRange(m_Asset.GetSpeedMin(), m_Asset.GetSpeedMax());
		const float lifeJitter = 1.0f + RandRange(-m_Asset.GetLifeVariance(), m_Asset.GetLifeVariance());

		slot->position = glm::vec2(originX, originY);
		slot->velocity = glm::vec2(std::cos(angle) * speed, std::sin(angle) * speed);
		slot->acceleration = m_Asset.GetGravity();
		slot->maxLife = glm::max(m_Asset.GetLife() * lifeJitter, 0.01f);
		slot->life = slot->maxLife;
		slot->sizeStart = glm::max(m_Asset.GetSizeStart() * RandRange(0.75f, 1.25f), 0.5f);
		slot->sizeEnd = glm::max(m_Asset.GetSizeEnd(), 0.0f);
		slot->colorStart = m_Asset.GetColorStart();
		slot->colorEnd = m_Asset.GetColorEnd();
		slot->rotation = 0.0f;
		slot->angularVelocity = RandRange(m_Asset.GetAngularVelMin(), m_Asset.GetAngularVelMax());
		slot->alive = true;
	}

	void ParticleEditorWindow::PlayPreviewBurst()
	{
		const float ox = 0.0f;
		const float oy = 0.0f;
		if (m_Asset.GetMode() == ParticleEmitMode::Continuous)
		{
			const int n = glm::clamp((int)std::ceil(glm::max(m_Asset.GetRate(), 1.0f) * 0.12f), 1, 48);
			for (int i = 0; i < n; ++i)
				SpawnPreviewOne(ox, oy);
		}
		else
		{
			const int n = glm::clamp(m_Asset.GetCount(), 0, 512);
			for (int i = 0; i < n; ++i)
				SpawnPreviewOne(ox, oy);
		}
		m_BurstCooldown = glm::max(m_Asset.GetLife() * 1.35f, 0.35f);
	}

	void ParticleEditorWindow::UpdatePreview(float dt)
	{
		if (dt <= 0.0f)
			return;

		if (m_Asset.GetMode() == ParticleEmitMode::Continuous && m_LoopPreview)
		{
			m_EmitAccum += glm::max(m_Asset.GetRate(), 0.0f) * dt;
			int n = 0;
			while (m_EmitAccum >= 1.0f && n < 64)
			{
				SpawnPreviewOne(0.0f, 0.0f);
				m_EmitAccum -= 1.0f;
				++n;
			}
		}
		else if (m_Asset.GetMode() == ParticleEmitMode::Burst && m_LoopPreview)
		{
			m_BurstCooldown -= dt;
			if (m_BurstCooldown <= 0.0f)
				PlayPreviewBurst();
		}

		for (PreviewParticle& p : m_Preview)
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

	void ParticleEditorWindow::DrawInspector()
	{
		if (ImGui::InputText("Name", m_NameBuf, sizeof(m_NameBuf)))
			m_Dirty = true;

		int mode = (m_Asset.GetMode() == ParticleEmitMode::Continuous) ? 1 : 0;
		const char* modes[] = { "Burst", "Continuous (Loop)" };
		if (ImGui::Combo("Mode", &mode, modes, 2))
		{
			m_Asset.SetMode(mode == 1 ? ParticleEmitMode::Continuous : ParticleEmitMode::Burst);
			m_Dirty = true;
			ClearPreview();
			PlayPreviewBurst();
		}

		if (m_Asset.GetMode() == ParticleEmitMode::Burst)
		{
			int count = m_Asset.GetCount();
			if (ImGui::SliderInt("Count", &count, 1, 256))
			{
				m_Asset.SetCount(count);
				m_Dirty = true;
			}
		}
		else
		{
			float rate = m_Asset.GetRate();
			if (ImGui::DragFloat("Rate (p/s)", &rate, 1.0f, 1.0f, 500.0f))
			{
				m_Asset.SetRate(rate);
				m_Dirty = true;
			}
		}

		float speedMin = m_Asset.GetSpeedMin();
		float speedMax = m_Asset.GetSpeedMax();
		if (ImGui::DragFloat("Speed Min", &speedMin, 1.0f, 0.0f, 2000.0f) ||
			ImGui::DragFloat("Speed Max", &speedMax, 1.0f, 0.0f, 2000.0f))
		{
			m_Asset.SetSpeedMin(speedMin);
			m_Asset.SetSpeedMax(glm::max(speedMax, speedMin));
			m_Dirty = true;
		}

		float life = m_Asset.GetLife();
		float lifeVar = m_Asset.GetLifeVariance();
		if (ImGui::DragFloat("Life", &life, 0.01f, 0.05f, 10.0f))
		{
			m_Asset.SetLife(life);
			m_Dirty = true;
		}
		if (ImGui::SliderFloat("Life Variance", &lifeVar, 0.0f, 1.0f))
		{
			m_Asset.SetLifeVariance(lifeVar);
			m_Dirty = true;
		}

		float sizeStart = m_Asset.GetSizeStart();
		float sizeEnd = m_Asset.GetSizeEnd();
		if (ImGui::DragFloat("Size Start", &sizeStart, 0.1f, 0.5f, 200.0f) ||
			ImGui::DragFloat("Size End", &sizeEnd, 0.1f, 0.0f, 200.0f))
		{
			m_Asset.SetSizeStart(sizeStart);
			m_Asset.SetSizeEnd(sizeEnd);
			m_Dirty = true;
		}

		float c0[4] = { m_Asset.GetColorStart().r, m_Asset.GetColorStart().g, m_Asset.GetColorStart().b, m_Asset.GetColorStart().a };
		float c1[4] = { m_Asset.GetColorEnd().r, m_Asset.GetColorEnd().g, m_Asset.GetColorEnd().b, m_Asset.GetColorEnd().a };
		if (ImGui::ColorEdit4("Color Start", c0))
		{
			m_Asset.SetColorStart(glm::vec4(c0[0], c0[1], c0[2], c0[3]));
			m_Dirty = true;
		}
		if (ImGui::ColorEdit4("Color End", c1))
		{
			m_Asset.SetColorEnd(glm::vec4(c1[0], c1[1], c1[2], c1[3]));
			m_Dirty = true;
		}

		float grav[2] = { m_Asset.GetGravity().x, m_Asset.GetGravity().y };
		if (ImGui::DragFloat2("Gravity", grav, 1.0f))
		{
			m_Asset.SetGravity(glm::vec2(grav[0], grav[1]));
			m_Dirty = true;
		}

		float dir = m_Asset.GetDirection();
		float spread = m_Asset.GetSpread();
		if (ImGui::SliderFloat("Direction", &dir, 0.0f, 360.0f) ||
			ImGui::SliderFloat("Spread", &spread, 0.0f, 360.0f))
		{
			m_Asset.SetDirection(dir);
			m_Asset.SetSpread(spread);
			m_Dirty = true;
		}

		float angMin = m_Asset.GetAngularVelMin();
		float angMax = m_Asset.GetAngularVelMax();
		if (ImGui::DragFloat("Angular Min", &angMin, 1.0f) ||
			ImGui::DragFloat("Angular Max", &angMax, 1.0f))
		{
			m_Asset.SetAngularVelMin(angMin);
			m_Asset.SetAngularVelMax(angMax);
			m_Dirty = true;
		}

		ImGui::Separator();
		ImGui::TextDisabled("Presets");
		if (ImGui::Button("Fire"))
		{
			std::string path = m_Asset.GetPath();
			std::string name = m_NameBuf;
			m_Asset = ParticleEmitterAsset::CreateFire(name);
			m_Asset.SetPath(path);
			std::snprintf(m_NameBuf, sizeof(m_NameBuf), "%s", m_Asset.GetName().c_str());
			m_Dirty = true;
			ClearPreview();
			PlayPreviewBurst();
		}
		ImGui::SameLine();
		if (ImGui::Button("Spark"))
		{
			std::string path = m_Asset.GetPath();
			std::string name = m_NameBuf;
			m_Asset = ParticleEmitterAsset::CreateSpark(name);
			m_Asset.SetPath(path);
			std::snprintf(m_NameBuf, sizeof(m_NameBuf), "%s", m_Asset.GetName().c_str());
			m_Dirty = true;
			ClearPreview();
			PlayPreviewBurst();
		}
		ImGui::SameLine();
		if (ImGui::Button("Default"))
		{
			std::string path = m_Asset.GetPath();
			std::string name = m_NameBuf;
			m_Asset = ParticleEmitterAsset::CreateDefault(name);
			m_Asset.SetPath(path);
			m_Dirty = true;
			ClearPreview();
			PlayPreviewBurst();
		}
	}

	void ParticleEditorWindow::DrawPreview()
	{
		ImGui::BeginChild("##particlePreview", ImVec2(0, 0), true);
		const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
		ImDrawList* draw = ImGui::GetWindowDrawList();

		draw->AddRectFilled(canvasPos,
			ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
			IM_COL32(18, 18, 22, 255));

		const float cx = canvasPos.x + canvasSize.x * 0.5f;
		const float cy = canvasPos.y + canvasSize.y * 0.65f;
		const float scale = 0.35f; // world px → screen

		// Origin crosshair
		draw->AddLine(ImVec2(cx - 8, cy), ImVec2(cx + 8, cy), IM_COL32(80, 80, 90, 255));
		draw->AddLine(ImVec2(cx, cy - 8), ImVec2(cx, cy + 8), IM_COL32(80, 80, 90, 255));

		for (const PreviewParticle& p : m_Preview)
		{
			if (!p.alive)
				continue;
			const float t = 1.0f - (p.life / p.maxLife);
			const float size = glm::mix(p.sizeStart, p.sizeEnd, t) * scale;
			const glm::vec4 color = glm::mix(p.colorStart, p.colorEnd, t);
			if (size < 0.4f || color.a < 0.02f)
				continue;

			const float sx = cx + p.position.x * scale;
			const float sy = cy - p.position.y * scale; // Y up in sim → screen Y down
			const float half = size * 0.5f;
			const float rad = glm::radians(p.rotation);
			const float c = std::cos(rad);
			const float s = std::sin(rad);

			ImVec2 corners[4] = {
				ImVec2(sx + (-half) * c - (-half) * s, sy + (-half) * s + (-half) * c),
				ImVec2(sx + ( half) * c - (-half) * s, sy + ( half) * s + (-half) * c),
				ImVec2(sx + ( half) * c - ( half) * s, sy + ( half) * s + ( half) * c),
				ImVec2(sx + (-half) * c - ( half) * s, sy + (-half) * s + ( half) * c),
			};
			const ImU32 col = IM_COL32(
				(int)(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f),
				(int)(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f),
				(int)(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f),
				(int)(glm::clamp(color.a, 0.0f, 1.0f) * 255.0f));
			draw->AddConvexPolyFilled(corners, 4, col);
		}

		ImGui::Dummy(canvasSize);
		ImGui::EndChild();
	}

	void ParticleEditorWindow::OnUpdate()
	{
		if (!m_Open)
			return;

		ImGui::SetNextWindowSize(ImVec2(960.0f, 560.0f), ImGuiCond_FirstUseEver);
		std::string title = "Particle Editor";
		if (!m_Asset.GetPath().empty())
			title += " - " + m_Asset.GetPath();
		if (m_Dirty)
			title += " *";
		title += "###ParticleEditor";

		if (!ImGui::Begin(title.c_str(), &m_Open))
		{
			ImGui::End();
			return;
		}
		m_WindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		if (ImGui::Button("Save"))
			SaveFile();
		ImGui::SameLine();
		if (ImGui::Button("Reload") && !m_Asset.GetPath().empty())
			Open(m_Asset.GetPath());
		ImGui::SameLine();
		if (ImGui::Button("Play"))
		{
			ClearPreview();
			PlayPreviewBurst();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
			ClearPreview();
		ImGui::SameLine();
		ImGui::Checkbox("Loop Preview", &m_LoopPreview);

		ImGui::Separator();

		const float dt = glm::clamp(Time::GetDeltaTime(), 0.0f, 0.05f);
		UpdatePreview(dt > 0.0f ? dt : (1.0f / 60.0f));

		ImGui::Columns(2, nullptr, true);
		ImGui::SetColumnWidth(0, 340.0f);
		DrawInspector();
		ImGui::NextColumn();
		DrawPreview();
		ImGui::Columns(1);

		ImGui::End();
	}
}
