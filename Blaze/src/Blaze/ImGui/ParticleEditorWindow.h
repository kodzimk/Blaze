#pragma once
#include "bzpch.h"
#include "Blaze/Renderer/ParticleEmitterAsset.h"
#include "glm.hpp"
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace Blaze
{
	class BLAZE_API ParticleEditorWindow
	{
	public:
		ParticleEditorWindow();
		~ParticleEditorWindow();

		void OnUpdate();
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }

		static void OpenFile(const std::string& path);
		static bool IsBlockingMainEditor();

	private:
		struct PreviewParticle
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

		void Open(const std::string& path);
		bool SaveFile();
		void DrawInspector();
		void DrawPreview();
		void UpdatePreview(float dt);
		void ClearPreview();
		void SpawnPreviewOne(float originX, float originY);
		void PlayPreviewBurst();
		static float RandRange(float a, float b);

		bool m_Open = false;
		bool m_Dirty = false;
		bool m_LoopPreview = true;
		bool m_WindowFocused = false;
		ParticleEmitterAsset m_Asset;

		std::vector<PreviewParticle> m_Preview;
		float m_EmitAccum = 0.0f;
		float m_BurstCooldown = 0.0f;
		char m_NameBuf[128] = {};

		static ParticleEditorWindow* s_Instance;
		static const int s_MaxPreview = 512;
	};
}
