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
		GizmoAxis HitTest(const CameraProp& prop, const GameObject& object, const glm::vec2& worldPoint) const;
	private:
		void DrawQuad(const glm::vec2& center, const glm::vec2& halfSize, const glm::vec4& color);
		void DrawArrowHead(const glm::vec2& center, const glm::vec2& axis0, const glm::vec2& axis1, const glm::vec4& color);
		void DrawRing(const glm::vec2& center, const glm::vec2& radius, const glm::vec4& color);
	private:
		GizmoMode m_Mode = GizmoMode::Translate;

		Shader m_Shader;

		VertexArray m_QuadVAO;
		VertexBuffer m_QuadVBO;
		unsigned int m_QuadEBO;

		VertexArray m_TriVAO;
		VertexBuffer m_TriVBO;

		VertexArray m_RingVAO;
		VertexBuffer m_RingVBO;
		int m_RingVertexCount = 0;
	};
}
