#pragma once
#include"Blaze/Renderer/VertexArray.h"
#include"Blaze/Renderer/Shader.h"
#include"glm.hpp"

namespace Blaze
{
	class Camera;
	class GameObject;
	struct CameraProp;

	enum class GizmoAxis
	{
		None = 0,
		X,
		Y,
		XY
	};

	enum class GizmoMode
	{
		Translate = 0,
		Rotate,
		Scale
	};

	class BLAZE_API Gizmo2D
	{
	public:
		Gizmo2D();
		virtual ~Gizmo2D();

		void SetMode(GizmoMode mode) { m_Mode = mode; }
		GizmoMode GetMode() const { return m_Mode; }

		void Draw(const Camera& camera, const GameObject& object);
		void DrawCameraRange(const Camera& camera, const GameObject& cameraObject);
		void DrawCameraIcon(const Camera& camera, const GameObject& cameraObject);
		void DrawSoundIcon(const Camera& camera, const GameObject& soundObject);
		void DrawCollision(const Camera& camera, const GameObject& object);
		GizmoAxis HitTest(const CameraProp& prop, const GameObject& object, const glm::vec2& worldPoint) const;

		// Shared editor icons for ImGui (palette / hierarchy). 0 if not ready.
		static unsigned int GetSoundIconTextureID();
		static unsigned int GetCameraIconTextureID();
		static unsigned int GetScriptIconTextureID();
		static unsigned int GetMapIconTextureID();
		static unsigned int GetFolderIconTextureID();
		static int GetSoundIconSize();
		static int GetCameraIconSize();
		static int GetScriptIconSize();
		static int GetMapIconSize();
		static int GetFolderIconSize();
	private:
		void DrawQuad(const glm::vec2& center, const glm::vec2& halfSize, const glm::vec4& color, float rotationDegrees = 0.0f);
		void DrawArrowHead(const glm::vec2& center, const glm::vec2& axis0, const glm::vec2& axis1, const glm::vec4& color);
		void DrawRing(const glm::vec2& center, const glm::vec2& radius, const glm::vec4& color);
		void DrawTexturedIcon(const glm::mat4& viewMatrix, const glm::vec2& center, const glm::vec2& halfSize, unsigned int textureID, float rotationDegrees = 0.0f);
		void EnsureSoundIcon();
		void EnsureCameraIcon();
	private:
		GizmoMode m_Mode = GizmoMode::Translate;

		Shader m_Shader;
		Shader m_IconShader;

		VertexArray m_QuadVAO;
		VertexBuffer m_QuadVBO;
		unsigned int m_QuadEBO;

		VertexArray m_TriVAO;
		VertexBuffer m_TriVBO;

		VertexArray m_RingVAO;
		VertexBuffer m_RingVBO;
		int m_RingVertexCount = 0;

		VertexArray m_IconVAO;
		VertexBuffer m_IconVBO;
		unsigned int m_IconEBO = 0;

		unsigned int m_SoundIconTex = 0;
		unsigned int m_CameraIconTex = 0;
		static unsigned int s_SharedSoundIconTex;
		static unsigned int s_SharedCameraIconTex;
		static unsigned int s_SharedScriptIconTex;
		static unsigned int s_SharedMapIconTex;
		static unsigned int s_SharedFolderIconTex;
		static const int s_SoundIconSize = 64;
		static const int s_CameraIconSize = 64;
		static const int s_ScriptIconSize = 64;
		static const int s_MapIconSize = 64;
		static const int s_FolderIconSize = 64;
	};
}
