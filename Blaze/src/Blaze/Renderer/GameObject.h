#include"bzpch.h"
#include"Blaze/Renderer/VertexArray.h"
#include"Blaze/Renderer/Texture.h"
#include"glm.hpp"

namespace Blaze
{
	struct CameraProp;
	class Shader;

	class BLAZE_API GameObject
	{
	public:
		GameObject(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, glm::vec4 color,std::string name);
		virtual ~GameObject();

		void Draw(const CameraProp& prop, Shader& const m_Shader);
		void SetPosition(glm::vec3& pos);
		void Translate(glm::vec3& translate);
		void SetTexture(std::string path);
		inline std::string GetName() const { return m_Name; }
	protected:
		VertexArray m_VertexArray;
		VertexBuffer m_VertexBuffer;
		unsigned int m_EBO;
		Texture m_Texture;
		glm::vec4 m_Color;
		glm::mat4 m_Matrix = glm::mat4(1.0f);
		glm::vec3 m_Pos = glm::vec3(0.0f);
		std::string m_Name;
	};
}

