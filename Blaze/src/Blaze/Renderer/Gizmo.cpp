#include "Gizmo.h"
#include<glad/glad.h>
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/Shader.h"
#include<gtc/type_ptr.hpp>
#include"Blaze/Logging/Log.h"

namespace Blaze
{
	Gizmo* Gizmo::s_Instance = nullptr;

	Gizmo::Gizmo(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, glm::vec4 color)
		: m_Visible(false), m_Color(color)
	{
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

	Gizmo::~Gizmo()
	{
	}

	void Gizmo::MoveGizmo()
	{
		float deltaX = 0.05f;
		float deltaY = 0.00f;
		Translate(glm::vec3(deltaX, deltaY, 0.0f));

		for (auto object : m_ReferredObjects)
		{
			object->Translate(glm::vec3(deltaX, deltaY, 0));
		}

		
	}

	void Gizmo::Focus(GameObject* object)
	{
		if (std::find(m_ReferredObjects.begin(),m_ReferredObjects.end(),object) == m_ReferredObjects.end())
			m_ReferredObjects.push_back(object);

		m_Visible = true;
	}

	void Gizmo::UnFocus()
	{
		m_ReferredObjects.clear();
		m_Visible = false;
	}


	void Gizmo::Draw(const CameraProp& prop, Shader& const m_Shader)
	{
		m_Shader.SetUniform4fv(m_Color, "color");
		m_Shader.SetUniformMatrix4fv(m_Matrix, "object_matrix");

		m_Texture.Bind();
		m_VertexArray.Bind();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		m_Texture.UnBind();
	}

	void Gizmo::SetPosition(glm::vec3& pos)
	{
		m_Pos = pos;
		m_Matrix = glm::translate(glm::mat4(1.0f), m_Pos);
	}

	void Gizmo::Translate(glm::vec3& translate)
	{
		m_Pos += translate;
		m_Matrix = glm::translate(m_Matrix, translate);
	}

	void Gizmo::SetTexture(std::string path)
	{
		m_Texture.Generate(path);
	}

}
