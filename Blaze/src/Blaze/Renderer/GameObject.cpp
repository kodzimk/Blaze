#include "GameObject.h"
#include<glad/glad.h>
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/Shader.h"
#include<gtc/type_ptr.hpp>
#include"Blaze/Logging/Log.h"

namespace Blaze {
	Blaze::GameObject::GameObject(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, glm::vec4 color, std::string name)
		: m_Color(color),m_Name(name)
	{		
		if (vertices.size() >= 5)
		{
			m_LocalMin = glm::vec2(vertices[0], vertices[1]);
			m_LocalMax = m_LocalMin;
			for (size_t i = 0; i + 1 < vertices.size(); i += 5)
			{
				m_LocalMin.x = std::min(m_LocalMin.x, vertices[i]);
				m_LocalMin.y = std::min(m_LocalMin.y, vertices[i + 1]);
				m_LocalMax.x = std::max(m_LocalMax.x, vertices[i]);
				m_LocalMax.y = std::max(m_LocalMax.y, vertices[i + 1]);
			}
		}

		glGenBuffers(1, &m_EBO);
		m_VertexArray.Bind();

		m_VertexBuffer.Bind();
		m_VertexBuffer.SetFloat(vertices.data(), vertices.size() * sizeof(float), 5);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_STATIC_DRAW);

		m_VertexArray.AttribPointer(m_VertexBuffer, 3, 0);	
		m_VertexArray.AttribPointer(m_VertexBuffer, 2, 3);
		m_VertexBuffer.UnBind();

		m_VertexArray.UnBind();
	}

	Blaze::GameObject::~GameObject()
	{

	}

	void GameObject::Draw(const CameraProp& prop,Shader& const m_Shader)
	{
		if (m_Type == ObjectType::Camera)
			return;

		m_Shader.SetUniform4fv(m_Color, "color");
		m_Shader.SetUniformMatrix4fv(m_Matrix, "object_matrix");

		m_Texture.Bind();
		m_VertexArray.Bind();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		m_Texture.UnBind();
	}

	void GameObject::SetPosition(glm::vec3& pos)
	{
		m_Pos = pos;
		RebuildMatrix();
	}

	void GameObject::Translate(glm::vec3& translate)
	{
		m_Pos += translate;
		RebuildMatrix();
	}

	void GameObject::SetRotation(float degrees)
	{
		m_Rotation = degrees;
		RebuildMatrix();
	}

	void GameObject::Rotate(float degrees)
	{
		m_Rotation += degrees;
		RebuildMatrix();
	}

	void GameObject::SetScale(glm::vec2 scale)
	{
		m_Scale = scale;
		RebuildMatrix();
	}

	void GameObject::SetType(ObjectType type)
	{
		m_Type = type;
	}

	void GameObject::SetZoom(float zoom)
	{
		m_Zoom = glm::max(zoom, 0.05f);
	}

	void GameObject::RebuildMatrix()
	{
		float sx = Camera::m_prop.projection[0][0];
		float sy = Camera::m_prop.projection[1][1];

		if (glm::abs(sx) < 1e-8f || glm::abs(sy) < 1e-8f)
		{
			m_Matrix = glm::translate(glm::mat4(1.0f), m_Pos);
			return;
		}

		glm::mat4 m = glm::translate(glm::mat4(1.0f), m_Pos);
		m = glm::scale(m, glm::vec3(sx, sy, 1.0f));
		m = glm::rotate(m, glm::radians(m_Rotation), glm::vec3(0.0f, 0.0f, 1.0f));
		m = glm::scale(m, glm::vec3(m_Scale, 1.0f));
		m = glm::scale(m, glm::vec3(1.0f / sx, 1.0f / sy, 1.0f));
		m_Matrix = m;
	}

	void GameObject::SetTexture(std::string path)
	{
		m_TexturePath = path;
		m_Texture.Generate(path);
	}

	void GameObject::GetWorldAABB(const CameraProp& prop, glm::vec2& outMin, glm::vec2& outMax) const
	{
		float scaleX = prop.projection[0][0];
		float scaleY = prop.projection[1][1];

		float c = glm::cos(glm::radians(m_Rotation));
		float s = glm::sin(glm::radians(m_Rotation));
		glm::vec2 corners[4] =
		{
			{ m_LocalMin.x, m_LocalMin.y },
			{ m_LocalMax.x, m_LocalMin.y },
			{ m_LocalMax.x, m_LocalMax.y },
			{ m_LocalMin.x, m_LocalMax.y },
		};

		glm::vec2 mn, mx;
		for (int i = 0; i < 4; ++i)
		{
			glm::vec2 v(corners[i].x * m_Scale.x, corners[i].y * m_Scale.y);
			glm::vec2 r(c * v.x - s * v.y, s * v.x + c * v.y);
			if (i == 0) { mn = r; mx = r; }
			else
			{
				mn.x = std::min(mn.x, r.x); mn.y = std::min(mn.y, r.y);
				mx.x = std::max(mx.x, r.x); mx.y = std::max(mx.y, r.y);
			}
		}

		outMin = glm::vec2(m_Pos.x + mn.x * scaleX, m_Pos.y + mn.y * scaleY);
		outMax = glm::vec2(m_Pos.x + mx.x * scaleX, m_Pos.y + mx.y * scaleY);
	}

	bool GameObject::IsHovered(const CameraProp& prop, const glm::vec2& worldPoint) const
	{
		glm::vec2 min, max;
		GetWorldAABB(prop, min, max);

		return worldPoint.x >= min.x && worldPoint.x <= max.x &&
			   worldPoint.y >= min.y && worldPoint.y <= max.y;
	}
}
