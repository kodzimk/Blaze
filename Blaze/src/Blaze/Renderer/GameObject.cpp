#include "GameObject.h"
#include<glad/glad.h>
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/Shader.h"
#include<gtc/type_ptr.hpp>
#include"Blaze/Logging/Log.h"

namespace Blaze {
	Blaze::GameObject::GameObject(std::vector<float>& vertices, const std::vector<unsigned int>& indices, glm::vec4 color, std::string name)
		: m_Color(color)
	{		

		m_VertexArray.Bind();
		m_VertexBuffer.Bind();
		m_VertexBuffer.SetFloat(vertices.data(), sizeof(vertices), 5,FLOAT);

		
		m_VertexArray.AttribPointer(m_VertexBuffer,3,0);
		m_Name = name;

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

		m_VertexArray.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
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
