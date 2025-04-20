#pragma once
#include"bzpch.h"
#include"glm.hpp"

namespace Blaze
{
	class BLAZE_API Shader
	{
	public:
		Shader();
		~Shader();

		void CreateProgram(const char* vertexSource, const char* fragmentSource);
		void LinkProgram();
		void Use();

		// SetUniform

		void SetUniform1f(float data, const char* uniform_name);
		void SetUniform1i(int data, const char* uniform_name);

		void SetUniform3fv(const glm::vec3& data, const char* uniform_name);
		void SetUniform4fv(const glm::vec4& data, const char* uniform_name);

		void SetUniformMatrix4fv(const glm::mat4& data, const char* uniform_name);
	    

		uint32_t GetID() const { return m_Program; }
	private:
		uint32_t m_VertexShader;
		uint32_t m_FragmentShader;
		uint32_t m_Program;
	};
}

