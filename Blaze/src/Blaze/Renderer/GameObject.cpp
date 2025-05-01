#include "GameObject.h"
#include<glad/glad.h>
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/Shader.h"
#include<gtc/type_ptr.hpp>
#include"Blaze/Logging/Log.h"

namespace Blaze {
	unsigned int VAO, VBO,EBO = 0;
	Blaze::GameObject::GameObject(std::vector<float>& vertices, const std::vector<unsigned int>& indices, glm::vec4 color, std::string name)
		: m_Color(color)
	{		
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(0);
	}

	Blaze::GameObject::~GameObject()
	{

	}

	void GameObject::Draw(const CameraProp& prop,Shader& const m_Shader)
	{
		m_Shader.SetUniform4fv(m_Color, "color");
		m_Shader.SetUniformMatrix4fv(m_Matrix, "object_matrix");

		glBindVertexArray(VAO);
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
