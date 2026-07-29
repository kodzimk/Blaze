#include "Gizmo.h"
#include<glad/glad.h>
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/GameObject.h"
#include<gtc/matrix_transform.hpp>
#include<gtc/constants.hpp>

namespace Blaze
{
	static const char* s_GizmoVertexSource = "#version 330 core\n"
		"layout (location = 0) in vec3 pos;\n"
		"uniform mat4 matrix;\n"
		"uniform mat4 transform;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = matrix * transform * vec4(pos, 1.0f);\n"
		"}\0";

	static const char* s_GizmoFragmentSource = "#version 330 core\n"
		"uniform vec4 color;\n"
		"out vec4 FragColor;\n"
		"void main()\n"
		"{\n"
		"   FragColor = color;\n"
		"}\0";

	static const float s_CenterHalf = 10.0f;
	static const float s_ArmLength = 90.0f;
	static const float s_ArmHalfThickness = 4.0f;
	static const float s_HeadHalfLength = 14.0f;
	static const float s_HeadHalfWidth = 10.0f;
	static const float s_ScaleTipHalf = 10.0f;
	static const float s_RingRadius = 110.0f;
	static const float s_RingHitBand = 14.0f;
	static const float s_HitPadding = 6.0f;

	static const glm::vec4 s_ColorX(0.9f, 0.2f, 0.2f, 1.0f);
	static const glm::vec4 s_ColorY(0.2f, 0.9f, 0.2f, 1.0f);
	static const glm::vec4 s_ColorCenter(0.95f, 0.85f, 0.2f, 1.0f);
	static const glm::vec4 s_ColorRing(0.25f, 0.6f, 1.0f, 1.0f);
	static const glm::vec4 s_ColorOutline(1.0f, 0.6f, 0.1f, 1.0f);
	static const glm::vec4 s_ColorCameraRange(0.2f, 0.75f, 1.0f, 0.95f);

	Gizmo2D::Gizmo2D()
	{
		m_Shader.CreateProgram(s_GizmoVertexSource, s_GizmoFragmentSource);
		m_Shader.LinkProgram();

		{
			float vertices[] =
			{
				 1.0f,  1.0f, 0.0f,
				 1.0f, -1.0f, 0.0f,
				-1.0f, -1.0f, 0.0f,
				-1.0f,  1.0f, 0.0f,
			};
			unsigned int indices[] = { 0, 1, 3, 1, 2, 3 };

			glGenBuffers(1, &m_QuadEBO);
			m_QuadVAO.Bind();
			m_QuadVBO.Bind();
			m_QuadVBO.SetFloat(vertices, sizeof(vertices), 3);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadEBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
			m_QuadVAO.AttribPointer(m_QuadVBO, 3, 0);
			m_QuadVBO.UnBind();
			m_QuadVAO.UnBind();
		}

		{
			float vertices[] =
			{
				 1.0f,  0.0f, 0.0f,
				-1.0f,  1.0f, 0.0f,
				-1.0f, -1.0f, 0.0f,
			};

			m_TriVAO.Bind();
			m_TriVBO.Bind();
			m_TriVBO.SetFloat(vertices, sizeof(vertices), 3);
			m_TriVAO.AttribPointer(m_TriVBO, 3, 0);
			m_TriVBO.UnBind();
			m_TriVAO.UnBind();
		}

		{
			const int segments = 48;
			std::vector<float> vertices;
			vertices.reserve((segments + 1) * 6);
			for (int i = 0; i <= segments; ++i)
			{
				float a = glm::two_pi<float>() * (float)i / (float)segments;
				float c = glm::cos(a), s = glm::sin(a);
				vertices.push_back(c);         vertices.push_back(s);         vertices.push_back(0.0f);
				vertices.push_back(c * 0.88f); vertices.push_back(s * 0.88f); vertices.push_back(0.0f);
			}
			m_RingVertexCount = (segments + 1) * 2;

			m_RingVAO.Bind();
			m_RingVBO.Bind();
			m_RingVBO.SetFloat(vertices.data(), (uint32_t)(vertices.size() * sizeof(float)), 3);
			m_RingVAO.AttribPointer(m_RingVBO, 3, 0);
			m_RingVBO.UnBind();
			m_RingVAO.UnBind();
		}
	}

	Gizmo2D::~Gizmo2D()
	{
		glDeleteBuffers(1, &m_QuadEBO);
	}

	void Gizmo2D::Draw(const Camera& camera, const GameObject& object)
	{
		CameraProp prop = camera.GetCameraProp();
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		glm::vec2 pos(object.GetPosition());

		m_Shader.Use();
		m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");

		glm::vec2 min, max;
		object.GetWorldAABB(prop, min, max);
		glm::vec2 boundsCenter = (min + max) * 0.5f;
		glm::vec2 boundsHalf = (max - min) * 0.5f;
		glm::vec2 outline(2.5f * sx, 2.5f * sy);
		DrawQuad(glm::vec2(boundsCenter.x, max.y), glm::vec2(boundsHalf.x + outline.x, outline.y), s_ColorOutline);
		DrawQuad(glm::vec2(boundsCenter.x, min.y), glm::vec2(boundsHalf.x + outline.x, outline.y), s_ColorOutline);
		DrawQuad(glm::vec2(min.x, boundsCenter.y), glm::vec2(outline.x, boundsHalf.y + outline.y), s_ColorOutline);
		DrawQuad(glm::vec2(max.x, boundsCenter.y), glm::vec2(outline.x, boundsHalf.y + outline.y), s_ColorOutline);

		if (m_Mode == GizmoMode::Rotate)
		{
			DrawRing(pos, glm::vec2(s_RingRadius * sx, s_RingRadius * sy), s_ColorRing);
			DrawQuad(pos, glm::vec2(4.0f * sx, 4.0f * sy), s_ColorRing);
			return;
		}

		float armCenter = s_CenterHalf + s_ArmLength * 0.5f;
		DrawQuad(glm::vec2(pos.x + armCenter * sx, pos.y),
			glm::vec2(s_ArmLength * 0.5f * sx, s_ArmHalfThickness * sy), s_ColorX);
		DrawQuad(glm::vec2(pos.x, pos.y + armCenter * sy),
			glm::vec2(s_ArmHalfThickness * sx, s_ArmLength * 0.5f * sy), s_ColorY);

		float tip = s_CenterHalf + s_ArmLength;
		if (m_Mode == GizmoMode::Translate)
		{
			DrawArrowHead(glm::vec2(pos.x + (tip + s_HeadHalfLength) * sx, pos.y),
				glm::vec2(s_HeadHalfLength * sx, 0.0f), glm::vec2(0.0f, s_HeadHalfWidth * sy), s_ColorX);
			DrawArrowHead(glm::vec2(pos.x, pos.y + (tip + s_HeadHalfLength) * sy),
				glm::vec2(0.0f, s_HeadHalfLength * sy), glm::vec2(-s_HeadHalfWidth * sx, 0.0f), s_ColorY);
		}
		else // Scale: square tips
		{
			DrawQuad(glm::vec2(pos.x + (tip + s_ScaleTipHalf) * sx, pos.y),
				glm::vec2(s_ScaleTipHalf * sx, s_ScaleTipHalf * sy), s_ColorX);
			DrawQuad(glm::vec2(pos.x, pos.y + (tip + s_ScaleTipHalf) * sy),
				glm::vec2(s_ScaleTipHalf * sx, s_ScaleTipHalf * sy), s_ColorY);
		}

		DrawQuad(pos, glm::vec2(s_CenterHalf * sx, s_CenterHalf * sy), s_ColorCenter);
	}

	void Gizmo2D::DrawCameraRange(const Camera& camera, const GameObject& cameraObject)
	{
		CameraProp prop = camera.GetCameraProp();
		m_Shader.Use();
		m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");

		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		glm::vec2 center(cameraObject.GetPosition());

		float z = glm::max(cameraObject.GetZoom(), 1e-4f);
		glm::vec2 half(1.0f / z, 1.0f / z);
		glm::vec2 thickness(2.5f * sx, 2.5f * sy);

		DrawQuad(glm::vec2(center.x, center.y + half.y), glm::vec2(half.x + thickness.x, thickness.y), s_ColorCameraRange);
		DrawQuad(glm::vec2(center.x, center.y - half.y), glm::vec2(half.x + thickness.x, thickness.y), s_ColorCameraRange);
		DrawQuad(glm::vec2(center.x - half.x, center.y), glm::vec2(thickness.x, half.y + thickness.y), s_ColorCameraRange);
		DrawQuad(glm::vec2(center.x + half.x, center.y), glm::vec2(thickness.x, half.y + thickness.y), s_ColorCameraRange);

		DrawQuad(center, glm::vec2(5.0f * sx, 5.0f * sy), s_ColorCameraRange);
		DrawQuad(center, glm::vec2(16.0f * sx, 2.0f * sy), s_ColorCameraRange);
		DrawQuad(center, glm::vec2(2.0f * sx, 16.0f * sy), s_ColorCameraRange);
	}

	GizmoAxis Gizmo2D::HitTest(const CameraProp& prop, const GameObject& object, const glm::vec2& worldPoint) const
	{
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		glm::vec2 pos(object.GetPosition());

		float dx = (worldPoint.x - pos.x) / sx;
		float dy = (worldPoint.y - pos.y) / sy;

		if (m_Mode == GizmoMode::Rotate)
		{
			float r = glm::sqrt(dx * dx + dy * dy);
			if (glm::abs(r - s_RingRadius) <= s_RingHitBand)
				return GizmoAxis::XY;
			return GizmoAxis::None;
		}

		if (glm::abs(dx) <= s_CenterHalf + s_HitPadding && glm::abs(dy) <= s_CenterHalf + s_HitPadding)
			return GizmoAxis::XY;

		float armEnd = s_CenterHalf + s_ArmLength + 2.0f * s_HeadHalfLength + s_HitPadding;
		float armBand = s_HeadHalfWidth + s_HitPadding;

		if (dx >= s_CenterHalf && dx <= armEnd && glm::abs(dy) <= armBand)
			return GizmoAxis::X;
		if (dy >= s_CenterHalf && dy <= armEnd && glm::abs(dx) <= armBand)
			return GizmoAxis::Y;

		return GizmoAxis::None;
	}

	void Gizmo2D::DrawQuad(const glm::vec2& center, const glm::vec2& halfSize, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f));
		transform = glm::scale(transform, glm::vec3(halfSize, 1.0f));

		m_Shader.SetUniformMatrix4fv(transform, "transform");
		m_Shader.SetUniform4fv(color, "color");
		m_QuadVAO.Bind();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		m_QuadVAO.UnBind();
	}

	void Gizmo2D::DrawArrowHead(const glm::vec2& center, const glm::vec2& axis0, const glm::vec2& axis1, const glm::vec4& color)
	{
		glm::mat4 transform(1.0f);
		transform[0] = glm::vec4(axis0, 0.0f, 0.0f);
		transform[1] = glm::vec4(axis1, 0.0f, 0.0f);
		transform[3] = glm::vec4(center, 0.0f, 1.0f);

		m_Shader.SetUniformMatrix4fv(transform, "transform");
		m_Shader.SetUniform4fv(color, "color");
		m_TriVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 3);
		m_TriVAO.UnBind();
	}

	void Gizmo2D::DrawRing(const glm::vec2& center, const glm::vec2& radius, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f));
		transform = glm::scale(transform, glm::vec3(radius, 1.0f));

		m_Shader.SetUniformMatrix4fv(transform, "transform");
		m_Shader.SetUniform4fv(color, "color");
		m_RingVAO.Bind();
		glDrawArrays(GL_TRIANGLE_STRIP, 0, m_RingVertexCount);
		m_RingVAO.UnBind();
	}
}
