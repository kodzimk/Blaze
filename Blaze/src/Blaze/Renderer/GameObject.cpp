#include "GameObject.h"
#include<glad/glad.h>
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/Shader.h"
#include<gtc/type_ptr.hpp>

namespace Blaze {
	Blaze::GameObject::GameObject(std::vector<float>& vertices, glm::vec4 color, std::string name)
		: m_Color(color)
	{
		m_VertexArray.Bind();
		m_VertexBuffer.Bind();
		m_VertexBuffer.SetFloat(vertices.data(), sizeof(vertices), 5);
		m_VertexArray.AttribPointer(m_VertexBuffer,3,0);
		m_VertexArray.AttribPointer(m_VertexBuffer,2 ,3);
		m_Name = name;
	}

	Blaze::GameObject::~GameObject()
	{
	}

	void GameObject::Draw(const CameraProp& prop,Shader& const m_Shader)
	{
		m_Shader.SetUniform4fv(m_Color, "color");
		m_Shader.SetUniformMatrix4fv(m_Matrix, "object_matrix");
		m_Shader.SetUniform1i(0, "in_texture");

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_Texture.GetTextID());
		
		m_VertexArray.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 3);
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
