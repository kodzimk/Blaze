#include "Shader.h"
#include"glad/glad.h"
#include<gtc/type_ptr.hpp>


namespace Blaze
{
	Shader::Shader()
		: m_VertexShader(0),m_FragmentShader(0),m_Program(0)
	{
	}

	Shader::~Shader()
	{
	}

	void Shader::CreateProgram(const char* vertexSource,const char* fragmentSource )
	{
		m_VertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(m_VertexShader, 1, &vertexSource, NULL);
		glCompileShader(m_VertexShader);

		m_FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(m_FragmentShader, 1, &fragmentSource, NULL);
		glCompileShader(m_FragmentShader);

		m_Program = glCreateProgram();
	}

	void Shader::LinkProgram()
	{
		glAttachShader(m_Program, m_VertexShader);
		glAttachShader(m_Program, m_FragmentShader);
		glLinkProgram(m_Program);

		glDeleteShader(m_VertexShader);
		glDeleteShader(m_FragmentShader);
	}
	void Shader::Use()
	{
		glUseProgram(m_Program);
	}

	void Shader::SetUniform1f(float data, const char* uniform_name)
	{
		glUniform1f(glGetUniformLocation(m_Program, uniform_name), data);
	}
	void Shader::SetUniform1i(int data, const char* uniform_name)
	{
		glUniform1f(glGetUniformLocation(m_Program, uniform_name), data);
	}
	void Shader::SetUniform3fv(const glm::vec3& data,const char* uniform_name)
	{
		glUniform3fv(glGetUniformLocation(m_Program, uniform_name), 1, glm::value_ptr(data));
	}

	void Shader::SetUniform4fv(const glm::vec4& data, const char* uniform_name)
	{
		glUniform4fv(glGetUniformLocation(m_Program, uniform_name), 1, glm::value_ptr(data));
	}

	void Shader::SetUniformMatrix4fv(const glm::mat4& data, const char* uniform_name)
	{
		glUniformMatrix4fv(glGetUniformLocation(m_Program, uniform_name), 1,GL_FALSE, glm::value_ptr(data));
	}
}