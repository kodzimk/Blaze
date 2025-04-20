#include "GameObject.h"
#include<glad/glad.h>
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/Shader.h"
#include<gtc/type_ptr.hpp>

namespace Blaze {
	Blaze::GameObject::GameObject(std::vector<float>& vertices, glm::vec4 color)
		: m_Color(color)
	{
		m_VertexArray.Bind();


		m_VertexBuffer.Bind();
		m_VertexBuffer.SetFloat(vertices.data(), sizeof(vertices), 3);
		m_VertexArray.AttribPointer(m_VertexBuffer);
	}

	Blaze::GameObject::~GameObject()
	{
	}

	void GameObject::Draw(const CameraProp& prop,Shader& const m_Shader)
	{
		m_Shader.SetUniform4fv(m_Color, "color");

		m_VertexArray.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
}
