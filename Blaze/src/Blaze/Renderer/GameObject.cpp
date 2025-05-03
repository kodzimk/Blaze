#include "GameObject.h"
#include<glad/glad.h>
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/Shader.h"
#include<gtc/type_ptr.hpp>
#include"Blaze/Logging/Log.h"

namespace Blaze {
	Blaze::GameObject::GameObject(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, glm::vec4 color, std::string name)
		: m_Color(color)
	{		
		
		glGenBuffers(1, &m_EBO);
		// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
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
		m_Shader.SetUniform4fv(m_Color, "color");
		m_Shader.SetUniformMatrix4fv(m_Matrix, "object_matrix");

		glBindTexture(GL_TEXTURE_2D, m_Texture.GetTextID());
		m_VertexArray.Bind();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}

	void GameObject::SetPosition(glm::vec3& pos)
	{
		m_Pos = pos;
		m_Matrix = glm::translate(glm::mat4(1.0f), m_Pos);
	}

	void GameObject::Translate(glm::vec3& translate)
	{
		m_Pos += translate;
		m_Matrix = glm::translate(m_Matrix, translate);
	}

	void GameObject::SetTexture(std::string path)
	{
		m_Texture.Generate(path);
	}
}
