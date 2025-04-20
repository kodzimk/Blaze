#include"bzpch.h"
#include"Blaze/Renderer/VertexArray.h"
#include"glm.hpp"

namespace Blaze
{
	struct CameraProp;
	class Shader;

	class GameObject
	{
	public:
		GameObject(std::vector<float>& vertices, glm::vec4 color);
		virtual ~GameObject();

		void Draw(CameraProp& prop, Shader& m_Shader);
	private:
		VertexArray m_VertexArray;
		VertexBuffer m_VertexBuffer;
		glm::vec4 m_Color;
	};
}

