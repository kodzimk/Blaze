#include "GameObject.h"
#include<glad/glad.h>

namespace Blaze {
	Blaze::GameObject::GameObject(std::vector<float> vertices)
		:vertices(vertices)
	{

		const char* vertexSource = "#version 330 core\n"
			"layout (location = 0) in vec3 position;\n"
			"out vec3 currentPos;\n"
			"void main() {\n"
			"gl_Position = vec4(position.x, position.y, position.z, 1.0f);\n"
			"currentPos = position;\n"
			"}\0";

		const char* fragmentSource = "#version 330 core\n"
			"in vec3 currentPos;\n"
			"out vec4 colour;\n"
			"void main() {\n"
			"colour = vec4(1.0f, 1.0f, 0.0f, 1.0);\n"
			"}\0";

		glGenVertexArrays(1, &m_vaID);
		glBindVertexArray(m_vaID);

		glGenBuffers(1, &m_vboID);
		glBindBuffer(GL_ARRAY_BUFFER, m_vboID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data() , GL_STATIC_DRAW);

		m_vertexshaderID = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(m_vertexshaderID, 1, &vertexSource, NULL);
		glCompileShader(m_vertexshaderID);

		m_fragmentshaderID = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(m_fragmentshaderID, 1,&fragmentSource , NULL);
		glCompileShader(m_fragmentshaderID);

		m_shaderProg = glCreateProgram();

		glAttachShader(m_shaderProg, m_vertexshaderID);
		glAttachShader(m_shaderProg, m_fragmentshaderID);
		glLinkProgram(m_shaderProg);

		glUseProgram(m_shaderProg);
		glDeleteShader(m_vertexshaderID);
		glDeleteShader(m_fragmentshaderID);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
	}

	Blaze::GameObject::~GameObject()
	{
	}

	void GameObject::Draw()
	{
		glUseProgram(m_shaderProg);

		glBindVertexArray(m_vaID);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
}
